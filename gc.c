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

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <sched.h>
#include <linux/futex.h>
#include <sys/time.h>

void *heap;

int liveSegmentCount = 0;

#define ARENA_CELLS (1024 * 1024)

#define ATOM_VECTOR   0
#define ENTITY_VECTOR 1
#define PROMISE       2
#define CHANNEL       3

int vectorLength(vector v) {
  return v->type >> 3;
}
vector endOfVector(vector v) {
  return (vector)&v->data[vectorLength(v)];
}
vector endOfEmptyVector(vector v) {
  return (vector)v->data;
}
void setVectorLength(vector v, int l) {
  v->type = l << 3 | v->type & 7;
}

int vectorType(vector v) {
  return v->type & 3;
}
void setVectorType(vector v, int t) {
  v->type = v->type & ~7 | t; // Clears the GC mark bit.
}

// Used only during a garbage collection cycle.
int isMarked(vector v) {
  return v->type & 4;
}
void setMarkBit(vector v) {
  v->type |= 4;
}
void clearMarkBit(vector v) {
  v->type &= ~4;
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

vector symbolTable, oLobby, oInterpreter;
void flip() {
  whiteList = constructWhiteList();
  blackList = emptyVector;
  blackList->next = blackList->prev = grayList = garbageCollectorRoot;
  grayList->next = grayList->prev = blackList;
  setMarkBit(emptyVector);
  setMarkBit(garbageCollectorRoot);
}

void scan() {
  switch (vectorType(grayList)) {
    case ENTITY_VECTOR:
      for (int i = 0; i < vectorLength(grayList); i++) mark(idx(grayList, i));
      break;
    case PROMISE:
      mark(promiseValue(grayList));
      break;
    case CHANNEL:
      mark(channelTarget((channel)grayList));
    // Atom vectors have nothing in them to scan.
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

int isPromise(void *e) {
  return vectorType((vector)e) == PROMISE;
}
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
      die("Promise fulfillment stolen between comparisons.");
    if (syscall(SYS_futex, pvf, FUTEX_WAKE, -1) == -1)
      die("Weird error while waking up threads waiting for promise fulfillment.");
  }
  else if (oldValue != UNWATCHED_PROMISE) die("Promise fulfillment stolen.");
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

int isChannel(vector v) {
  return vectorType(v) == CHANNEL;
}
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
