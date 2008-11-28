/*
    Copyright 2008 Sam Chapin

    This file is part of Gospel.

    Gospel is free software: you can redistribute it and/or modify
    it under the terms of version 3 of the GNU General Public License
    as published by the Free Software Foundation.

    Gospel is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Gospel.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "gc.h"
#include "mutex.h"
#include "death.h"

#include "objects.h" // Because we define some object creation functions.

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <sched.h>
#include <linux/futex.h>
#include <sys/time.h>

void *heap;

int liveSegmentCount = 0;

#define ARENA_CELLS (1024 * 1024)

#define TYPE_BIT_MASK 7
// "Tag bits" are the type bits plus the GC mark bit.
#define TAG_BIT_MASK  (TYPE_BIT_MASK * 2 + 1)
#define TAG_BIT_COUNT 4


#define ATOM_VECTOR   0
#define ENTITY_VECTOR 1
#define PROMISE       2
#define CHANNEL       3
#define FIXNUM        4
#define PRIMITIVE     5
#define CLOSURE       6
#define STACK_FRAME   7
#define MARK_BIT      8

int vectorLength(vector v) {
  return v->type >> TAG_BIT_COUNT;
}
vector endOfVector(vector v) {
  return (vector)&v->data[vectorLength(v)];
}
vector endOfEmptyVector(vector v) {
  return (vector)v->data;
}
void setVectorLength(vector v, int l) {
  v->type = l << TAG_BIT_COUNT | v->type & TAG_BIT_MASK;
}

int vectorType(vector v) {
  return v->type & TYPE_BIT_MASK;
}
void setVectorType(vector v, int t) {
  v->type = v->type & ~TAG_BIT_MASK | t; // Clears the GC mark bit.
}

// Strictly speaking, we don't need to distinguish closures at the primitive-type level to avoid segfaults;
// they could just be entity vectors. But requiring the dispatch code to traverse every object's prototype
// chain in case it leads to the closure prototype would really suck, and in fact it might be desirable to
// allow the possibility of executable closure-like objects that don't inherit from the closure prototype.
// In any event, the extra type safety doesn't hurt.

int isPromise(vector v)   { return vectorType(v) == PROMISE;   }
int isChannel(vector v)   { return vectorType(v) == CHANNEL;   }

int isInteger(obj o)    { return vectorType(o) == FIXNUM;      }
int isPrimitive(obj o)  { return vectorType(o) == PRIMITIVE;   }
int isClosure(obj o)    { return vectorType(o) == CLOSURE;     }
int isStackFrame(obj o) { return vectorType(o) == STACK_FRAME; }

// These typechecks are at least sufficient to prevent a segmentation fault, unless there is no null
// terminator between the beginning of the hidden atom vector and the end of the program's segment.
// TODO: Consider giving strings their own type label, in future tagging schemes that have the room?
int isString(obj o) {
  vector v = hiddenEntity(o);
  return vectorType(v) == ATOM_VECTOR;
}
int isSymbol(obj o) {
  return isString(o);
}

// Used only during a garbage collection cycle.
int isMarked(vector v) {
  return v->type & MARK_BIT;
}
void setMarkBit(vector v) {
  v->type |= MARK_BIT;
}
void clearMarkBit(vector v) {
  v->type &= ~MARK_BIT;
}

vector replace(vector v, vector replacement) {
  if (v == v->next) return replacement->prev = replacement->next = replacement;
  replacement->prev = v->prev;
  replacement->next = v->next;
  v->prev->next = v->next->prev = replacement;
  return replacement;
}
vector insertBetween(vector v, vector prev, vector next) {
  return v == prev || v == next ? v : ((v->next = next)->prev = (v->prev = prev)->next = v);
}
vector insertBefore(vector v, vector next) {
  return insertBetween(v, next->prev, next);
}
vector extract(vector v) {
  v->prev->next = v->next;
  v->next->prev = v->prev;
  return v;
}

#define VECTOR_HEADER_SIZE 3

// Uses emptyVector as buffer space, mangling its pointers.
vector constructWhiteList() {
  const vector topOfHeap = (vector)(heap + ARENA_CELLS * sizeof(void *));
  vector prev = emptyVector,
         current = endOfEmptyVector(emptyVector); // The real beginning of the heap.
  void advance() {
    current = endOfVector(current);
  }
  vector finish() {
    if (prev == emptyVector) {
      // TODO: This is very unlikely, can we make it impossible and eliminate this logic?
      die("<nothing free after garbage collection>");
    }
    vector first = emptyVector->next;
    first->prev = prev;
    prev->next = first;
    return first;
  }
  auto vector coalesce(void);
  vector sweep() {
    liveSegmentCount++;
    clearMarkBit(current);
    advance();
    return current == topOfHeap ? finish()
         : !isMarked(current)   ? coalesce()
         :                        sweep();
  }
  vector coalesce() {
    vector base = current;
    void merge() {
      // Combine contiguous white segments and link the result to the previous white segment.
      setVectorLength(base, ((int)current - (int)base->data) / sizeof(void *));
      base->prev = prev;
      prev = prev->next = base;
    }
    for (;;) {
      advance();
      if (current == topOfHeap) {
        merge();
        return finish();
      }
      if (isMarked(current)) {
        merge();
        return sweep();
      }
    }
  }

  liveSegmentCount = 0;
  return isMarked(current) ? sweep() : coalesce();
}

// Return the number of cells occupied by the segments of the white list, including headers.
// Should only be called from inside the allocator lock, to avoid the possibility of a GC interrupting.
int freeSpaceCount() {
  int result = 0;
  vector current = whiteList;
  do {
    result += vectorLength(current) + VECTOR_HEADER_SIZE;
  } while ((current = current->next) != whiteList);
  return result;
}

void mark(vector v) {
  if (v && !isMarked(v)) {
    setMarkBit(v);
    insertBefore(v, blackList);
  }
}

vector symbolTable;
void flip() {
  whiteList = constructWhiteList();
  blackList = emptyVector;
  blackList->next = blackList->prev = grayList = garbageCollectorRoot;
  grayList->next = grayList->prev = blackList;
  setMarkBit(emptyVector);
  setMarkBit(garbageCollectorRoot);
}

// TODO: Now that e.g. primitives and integers have their own typetag values, they can be
//       implemented more efficiently.
void scan() {
  switch (vectorType(grayList)) {
    case ENTITY_VECTOR:
    case CLOSURE:
    case STACK_FRAME:
      for (int i = 0; i < vectorLength(grayList); i++) mark(idx(grayList, i));
      break;
    case PROMISE:
      mark(promiseValue(grayList));
      break;
    case CHANNEL:
      mark(channelTarget((channel)grayList));
    // Atom vectors, fixnums abd primitives have nothing in them to scan.
  }
  grayList = grayList->next;
}

void collectGarbage() {
  int freeSpaceBefore = freeSpaceCount();
  while (grayList != blackList) scan();
  flip();
}

vector doAllotment(int size) {
  vector firstWhiteSegment = whiteList;
  do {
    int remainder = vectorLength(whiteList) - size;
    if (!remainder) {
      vector next = whiteList->next;
      if (next == whiteList) {
        // The request was for exactly the amount of the last remaining white segment.
        // TODO: This is unlikely, can we make it impossible and avoid the need for this logic?
        vector used = whiteList;
        collectGarbage();
        if (next == whiteList) die("Free list still empty after garbage collection.");
        return vectorLength(used) == size ? extract(used) : doAllotment(size);
      }
      vector used = extract(whiteList);
      whiteList = next;
      return used;
    }
    if (remainder >= VECTOR_HEADER_SIZE) {
      vector excess = replace(whiteList, (vector)&whiteList->data[size]),
             used = whiteList;
      setVectorLength(excess, remainder - VECTOR_HEADER_SIZE);
      clearMarkBit(excess); // TODO: Combine this with setting the length.
      setVectorLength(used, size);
      whiteList = excess;
      return used;
    }
  } while ((whiteList = whiteList->next) != firstWhiteSegment);
  return 0;
}

vector allot(int size) {
  forbidGC();
  vector n = doAllotment(size);
  if (!n) {
    collectGarbage();
    if (!(n = doAllotment(size))) die("Unable to fulfill allocation request.");
  }
  return n;
}

vector zero(vector v) {
  memset(v->data, 0, vectorLength(v) * sizeof(void *));
  return v;
}

void *idx(vector v, int i) {
  return v->data[i];
}
vector *idxPointer(vector v, int i) {
  return (vector *)&v->data[i];
}
vector *edenIdx(vector v, int i) {
  return (vector *)&v->data[i];
}
void *setIdx(vector v, int i, void *e) {
  return v->data[i] = e;
}
void *vectorData(vector v) {
  return v->data;
}

vector duplicateVector(vector *live, vector v) {
  int n = vectorLength(v);
  memcpy(*live = allot(n), v, (n + VECTOR_HEADER_SIZE) * sizeof(void *));
  permitGC();
  return *live;
}
void allotVector(vector *live, int length, int type) {
  setVectorType(*live = allot(length), type);
}
vector makeVector(vector *live, int length) {
  allotVector(live, length, ENTITY_VECTOR);
  zero(*live); // TODO: inline zero()
  permitGC();
  return *live;
}
vector makeAtomVector(vector *live, int length) {
  allotVector(live, length, ATOM_VECTOR);
  permitGC();
  return *live;
}
vector newVector(vector *live, int length, ...) {
  allotVector(live, length, ENTITY_VECTOR);
  va_list members;
  va_start(members, length);
  for (int i = 0; i < length; i++) setIdx(*live, i, va_arg(members, void *));
  va_end(members);
  permitGC();
  return *live;
}
vector newAtomVector(vector *live, int length, ...) {
  allotVector(live, length, ATOM_VECTOR);
  va_list members;
  va_start(members, length);
  for (int i = 0; i < length; i++) setIdx(*live, i, va_arg(members, void *));
  va_end(members);
  permitGC();
  return *live;
}

void initializeHeap() {
  void *memory;
  if (!(memory = malloc(ARENA_CELLS * sizeof(void *) + 1024))) die("Could not allocate heap.");
  // The gray list and black list (being in the same cycle) must have distinct pointers.
  // "emptyVector" is treated specially by the garbage collector: The heap is considered to begin
  // immediately after its end, so that it is never collected. It must always be at the lowest address.
  
  blackList = emptyVector = heap = memory;
  setVectorLength(blackList, 0);
  setVectorType(blackList, ATOM_VECTOR);
  grayList = endOfEmptyVector(blackList);
  setVectorLength(grayList, 0);
  setVectorType(grayList, ATOM_VECTOR);
  grayList->prev = grayList->next = blackList;
  blackList->prev = blackList->next = grayList;
  setMarkBit(blackList);
  setMarkBit(grayList);
  whiteList = endOfEmptyVector(grayList);
  setVectorLength(whiteList, ARENA_CELLS - VECTOR_HEADER_SIZE * 3);
  whiteList->prev = whiteList->next = whiteList;
}

void acquireFutex(volatile int *futexCount, volatile int *futexFlag) {
  increment(futexCount);
  while (compareAndExchange(futexFlag, 0, -1)) futexWait(futexFlag, -1);
}
void releaseFutex(volatile int *futexCount, volatile int *futexFlag) {
  *futexFlag = 0;
  if (exchangeAndAdd(futexCount, -1) == 1) return;
  // It's okay if another thread grabs the futex here. The thread we're waking up will see
  // that *futexFlag is set when it does its spurious wakeup check, and go back to sleep.
  futexWake(futexFlag, 1);
}

#define WATCHED_PROMISE   -1
#define UNWATCHED_PROMISE  0

promise newPromise(vector *live) {
  vector n = makeVector(live, 1);
  setVectorType(n, PROMISE);
  setIdx(n, 0, 0);
  return n;
}
vector *promiseValueField(promise p) {
  return (vector *)&p->data[0];
}
vector promiseValue(promise p) {
  // The external interface to promises, hiding whether they're watched or unwatched and only
  // revealing whether or not they've been fulfilled.
  vector v = *promiseValueField(p);
  return v == (vector)WATCHED_PROMISE || v == (vector)UNWATCHED_PROMISE ? 0 : v;
}
void fulfillPromise(promise p, vector o) {
  int *pvf = (int *)promiseValueField(p);
  int oldValue = compareAndExchange(pvf, UNWATCHED_PROMISE, (int)o);
  if (oldValue == WATCHED_PROMISE) {
    if (compareAndExchange(pvf, WATCHED_PROMISE, (int)o) != WATCHED_PROMISE)
      return; // Someone else fulfilled this promise between our comparisons.
    if (syscall(SYS_futex, pvf, FUTEX_WAKE, -1) == -1)
      die("Weird error while waking up threads waiting for promise fulfillment.");
  }
  else if (oldValue != UNWATCHED_PROMISE) return; // This promise has already been fulfilled.
}

vector waitFor(void *e) {
  if (!isPromise(e)) return e;
  int *pvf = (int *)promiseValueField(e);
  int existingValue = compareAndExchange(pvf, UNWATCHED_PROMISE, WATCHED_PROMISE);
  if (existingValue != WATCHED_PROMISE && existingValue != UNWATCHED_PROMISE)
    return (vector)existingValue;
  while (*pvf == WATCHED_PROMISE) futexWait(pvf, WATCHED_PROMISE);
  return waitFor((void *)*pvf);
}

// The allocator lock.
int GCMutexCount = 0, GCMutexFlag = 0;
void forbidGC() { acquireFutex(&GCMutexCount, &GCMutexFlag); }
void permitGC() { releaseFutex(&GCMutexCount, &GCMutexFlag); }

vector  channelTarget(vector c) { return (vector)idx(c, 0);  }
int    *channelCount(vector c)  { return (int *)&c->data[1]; }
int    *channelFlag(vector c)   { return (int *)&c->data[2]; }
vector newChannel(vector *live, vector o) {
  vector n = makeVector(live, 3);
  setVectorType(n, CHANNEL);
  setIdx(n, 0, o);
  // Other two elements remain zeroed.
  return n;
}
void acquireChannelLock(vector c) { acquireFutex(channelCount(c), channelFlag(c)); }
void releaseChannelLock(vector c) { releaseFutex(channelCount(c), channelFlag(c)); }

void spawn(void *topOfStack, void *f, void *a) {
  if (clone((int (*)(void *))f,
            topOfStack,
            CLONE_FS | CLONE_FILES | CLONE_PTRACE | CLONE_VM | CLONE_THREAD | CLONE_SYSVSEM | CLONE_SIGHAND,
            a)
      == -1) die("Failed spawning.");
}

vector suffix(vector *live, void *e, vector v) {
  int l = vectorLength(v);
  vector living = newVector(live, 3, e, v, 0),
         nv = makeVector(edenIdx(living, 2), l + 1);
  nv->data[l] = e;
  memcpy(nv->data, v->data, l * sizeof(void *));
  return nv;
}
vector prefix(vector *live, void *e, vector v) {
  int l = vectorLength(v);
  vector living = newVector(live, 3, e, v, 0),
         nv = makeVector(edenIdx(living, 2), l + 1);
  setIdx(nv, 0, e);
  memcpy(edenIdx(nv, 1), vectorData(v), l * sizeof(void *));
  return nv;
}

vector  class(obj o)        { return     idx(o, 0);     }
vector  instance(obj o)     { return     idx(o, 1);     }
vector  hiddenEntity(obj o) { return     idx(o, 2);     }
void   *hiddenAtom(obj o)   { return idx(idx(o, 2), 0); }

// Encapsulate the way slot names are stored within the class vector.
int slotCount(obj o) {
  return vectorLength(idx(o, 0)) - 1;
}
void *slotName(obj o, int i) {
  return idx(class(o), i + 1);
}
vector slotNameVector(vector *eden, obj o) {
  vector c = class(o);
  int l = vectorLength(c) - 1;
  vector s = makeVector(edenIdx(newVector(eden, 2, c, 0), 1), l);
  memcpy(vectorData(s), idxPointer(c, 1), l * sizeof(vector));
  return s;
}

// Encapsulate the way slot values are stored within the instance vector.
void *setSlotByIndex(obj o, int i, obj v) {
  return setIdx(instance(o), i, v);
}

void setClass(obj o, vector c)      { setIdx(o, 0, c); }
void setInstance(obj o, vector i)   { setIdx(o, 1, i); }
void setHiddenData(obj o, vector h) { setIdx(o, 2, h); }

void setSlotNames(vector *live, obj o, vector slotNames) {
  setClass(o, prefix(live, proto(o), slotNames));
}
void setSlotValues(obj o, vector slotValues) {
  setInstance(o, slotValues);
}

obj proto(obj o) {
  return idx(class(o), 0);
}
obj setProto(vector *live, obj o, obj p) {
  vector c = duplicateVector(live, class(o));
  setIdx(c, 0, p);
  setClass(o, c);
  return o;
}
obj newObject(vector *live, obj proto, vector slotNames, vector slotValues, void *hidden) {
  vector eden = newVector(live, 4, proto, slotNames, slotValues, hidden);
  return newVector(live,
                   3,
                   prefix(edenIdx(eden, 0), proto, slotNames),
                   slotValues,
                   hidden);
}
obj slotlessObject(vector *live, obj proto, vector hidden) {
  return newObject(live, proto, emptyVector, emptyVector, hidden);
}

obj fixnumObject(vector *live, obj proto, int hidden) {
  obj o = slotlessObject(live, proto, newAtomVector(live, 1, hidden));
  setVectorType(o, FIXNUM);
  return o;
}

obj primitive(vector *live, void *code) {
  obj o = slotlessObject(live, oPrimitive, newAtomVector(live, 1, code));
  setVectorType(o, PRIMITIVE);
  return o;
}

obj newClosure(vector *live, obj env, vector params, vector body) {
  obj o = slotlessObject(live, oClosure, newVector(live, 3, env, params, body));
  setVectorType(o, CLOSURE);
  return o;
}
vector closureEnv(obj c)    { return idx(hiddenEntity(c), 0); }
vector closureParams(obj c) { return idx(hiddenEntity(c), 1); }
vector closureBody(obj c)   { return idx(hiddenEntity(c), 2); }

obj integer(vector *live, int value) {
  return fixnumObject(live, oInteger, value);
}
int integerValue(obj i) {
  return (int)hiddenAtom(i);
}

obj stackFrame(vector *life, obj parent, vector names, vector values, vector continuation) {
  obj o = newObject(life, parent, names, values, continuation);
  setVectorType(o, STACK_FRAME);
  return o;
}
vector stackFrameContinuation(obj sf) {
  return hiddenEntity(sf);  
}

// Certain objects have to be given special type tags.
void initializePrototypeTags() {
  setVectorType(oInteger, FIXNUM);
  setVectorType(oPrimitive, PRIMITIVE);
  setVectorType(oClosure, CLOSURE);
}
