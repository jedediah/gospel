/*
    Copyright © 2008, 2009 Sam Chapin

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
#include "death.h"

#include "objects.h" // Because we define some object creation functions.

#include <pthread.h>
#include <setjmp.h>

#include <gmp.h> // For bignum finalization.
#include <regex.h> // For regex finalization.

void *heap;

int liveSegmentCount = 0;

#define ARENA_CELLS (1024 * 1024)

#define TYPE_BIT_MASK 15
// "Tag bits" are the type bits plus the GC mark bit.
#define TAG_BIT_MASK  (TYPE_BIT_MASK * 2 + 1)
#define TAG_BIT_COUNT 5


#define ATOM_VECTOR    0
#define ENTITY_VECTOR  1
#define PROMISE        2
#define ACTOR          3
#define PRIMITIVE      4
#define METHOD         5
#define STACK_FRAME    6
#define VECTOR         7
#define ENVIRONMENT    8
#define BIGNUM         9
#define REGEX         10
#define MARK_BIT      16

// This provides eden space during startup, before the first real thread data object has been created.
// TODO: Merge "threadData.c" back in here, since we have to expose its implementation anyway.
struct vectorStruct dummyThreadData = {0,
                                       0,
                                       5 << TAG_BIT_COUNT | MARK_BIT | ENTITY_VECTOR,
                                       {0, 0, 0, 0, 0}};

#ifdef NO_THREAD_VARIABLES
  pthread_key_t threadLocalStorageKey;
  vector getCurrentThread() {
    return pthread_getspecific(threadLocalStorageKey);
  }
  vector setCurrentThread(vector thread) {
    if (pthread_setspecific(threadLocalStorageKey, thread))
      die("Error while initializing thread-local storage.");
    return thread;
  }
  void initializeMainThread() {
    if (pthread_key_create(&threadLocalStorageKey, NULL))
      die("Error while creating thread-local storage key.");
    setCurrentThread(&dummyThreadData);
  }
#else
  __thread vector currentThread;
  vector setCurrentThread(vector thread) {
    return currentThread = thread;
  }
  void initializeMainThread() {
    currentThread = &dummyThreadData;
  }
#endif

vector blackList, grayList, ecruList, whiteList, emptyVector, garbageCollectorRoot;

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
  v->type = v->type & ~TYPE_BIT_MASK | t;
}

int isPromise(vector v)   { return vectorType(v) == PROMISE; }
int isActor(vector v)     { return vectorType(v) == ACTOR;   }

int isPrimitive(obj o)    { return vectorType(o) == PRIMITIVE;   }
int isMethod(obj o)       { return vectorType(o) == METHOD;      }
int isStackFrame(obj o)   { return vectorType(o) == STACK_FRAME; }
int isVectorObject(obj o) { return vectorType(o) == VECTOR;      }
int isEnvironment(obj o)  { return vectorType(o) == ENVIRONMENT; }
int isInteger(obj o)      { return vectorType(o) == BIGNUM;      }
int isRegex(obj o)        { return vectorType(o) == REGEX;       }

// As long as we admit as "strings" only NUL-terminated atom vectors, we won't segfault.
int isString(obj o) {
  if (vectorType(o) == ENTITY_VECTOR) {
    vector v = hiddenEntity(o);
    if (!v || vectorType(v) != ATOM_VECTOR) return 0;
    int last = (int)idx(v, vectorLength(v) - 1);
    // FIXME: Assumes 32-bit integers.
    return !(last & 0xff000000 && last & 0x00ff0000 && last & 0x0000ff00 && last & 0x000000ff);
  }
  return 0;
}
int isSymbol(obj o) {
  return isString(o);
}

__mpz_struct *bignumData(obj o) {
  return vectorData(hiddenEntity(o));
}
vector emptyBignumVector() {
  vector v = makeAtomVector(CELLS_REQUIRED_FOR_BYTES(sizeof(__mpz_struct)));
  mpz_init((__mpz_struct *)vectorData(v));
  return v;
}
obj emptyBignum() {
  return typedObject(oInteger, emptyBignumVector());
}

obj stringFromVector(vector v) {
  return typedObject(oString, v);
}
obj string(const char *s) {
  int length = CELLS_REQUIRED_FOR_BYTES(strlen(s) + 1);
  vector v = makeAtomVector(length);
  strcpy((char *)vectorData(v), s); // TODO: Write barrier.
  return stringFromVector(v);
}
char *stringData(obj s) {
  return (char *)(hiddenEntity(s)->data);
}
int stringLength(obj s) {
  vector data = hiddenEntity(s);
  int i = vectorLength(data);
  if (!i) return 0;
  char *last = (char *)idxPointer(data, i - 1);
  return i * 4 - ( !last[0] ? 4
                 : !last[1] ? 3
                 : !last[2] ? 2
                            : 1);
}
char stringIdx(obj s, int i) {
  return stringData(s)[i];
}
// TODO: Write barrier.
void setStringIdx(obj s, int i, char c) {
  stringData(s)[i] = c;
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
#include <stdio.h>
vector constructWhiteList() {
  const vector topOfHeap = (vector)(heap + ARENA_CELLS * sizeof(void *));
  struct vectorStruct stub = {0, 0, 0};
  vector prev = &stub,
         current = endOfEmptyVector(emptyVector); // The real beginning of the heap.
  void advance() {
    current = endOfVector(current);
  }
  vector finish() {
    if (prev == emptyVector) {
      // TODO: This is very unlikely, can we make it impossible and eliminate this logic?
      die("<nothing free after garbage collection>");
    }
    vector first = stub.next;
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
      switch (vectorType(current)) { // Perform finalization for the primitive types that need it.
        case BIGNUM:
          mpz_clear(bignumData(current));
          setVectorType(current, ENTITY_VECTOR);
          break;
        case REGEX:
          regfree(vectorData(hiddenEntity(current)));
          setVectorType(current, ENTITY_VECTOR);
      }
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

// TODO: Rearrange so that this isn't necessary.
void markPromise(vector);
void markActor(vector);

// TODO: Now that e.g. primitives and integers have their own typetag values, they can be
//       implemented more efficiently.
void scan() {
  switch (vectorType(grayList)) {
    case PROMISE:
      markPromise(grayList);
      break;
    case ACTOR:
      markActor(grayList);
    case ATOM_VECTOR:
      break;
    default:
      for (int i = 0; i < vectorLength(grayList); i++) mark(idx(grayList, i));
      break;
    }
  grayList = grayList->next;
}

#include <stdio.h>
void collectGarbage() {
  mark(garbageCollectorRoot); // FIXME: Redundant with respect to flip(), above?
  mark(oObject);
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
      clearMarkBit(used);
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

vector shelter(vector, vector);
vector shelteredValue(vector);
void invalidateEden() {
  shelter(currentThread, 0);
}
vector edenAllot(int n) {
  forbidGC();
  vector v = allot(2);
  setVectorType(v, ENTITY_VECTOR);
  setIdx(v, 0, 0);
  setIdx(v, 1, shelteredValue(currentThread));
  shelter(currentThread, v);
  // Now that the eden vector is in a consistent state, we can request the second allotment and not mind
  // that it could trigger a garbage collection.
  return setIdx(v, 0, allot(n));
}
vector duplicateVector(vector v) {
  int n = vectorLength(v);
  vector nv = edenAllot(n);
  memcpy(nv, v, (n + VECTOR_HEADER_SIZE) * sizeof(void *));
  permitGC();
  return nv;
}
vector allotVector(int length, int type) {
  vector v = edenAllot(length);
  setVectorType(v, type);
  return v;
}
vector makeVector(int length) {
  vector v = allotVector(length, ENTITY_VECTOR);
  zero(v); // TODO: inline zero()
  permitGC();
  return v;
}
vector makeAtomVector(int length) {
  vector v = allotVector(length, ATOM_VECTOR);
  permitGC();
  return v;
}
vector newVector(int length, ...) {
  vector v = allotVector(length, ENTITY_VECTOR);
  va_list members;
  va_start(members, length);
  for (int i = 0; i < length; i++) setIdx(v, i, va_arg(members, void *));
  va_end(members);
  permitGC();
  return v;
}
vector newAtomVector(int length, ...) {
  vector v = allotVector(length, ATOM_VECTOR);
  va_list members;
  va_start(members, length);
  for (int i = 0; i < length; i++) setIdx(v, i, va_arg(members, void *));
  va_end(members);
  permitGC();
  return v;
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

pthread_mutex_t threadListMutex = PTHREAD_MUTEX_INITIALIZER;
void acquireThreadListLock() {
  if (pthread_mutex_lock(&threadListMutex)) die("Error while acquiring thread list lock.");
}
void releaseThreadListLock() {
  if (pthread_mutex_unlock(&threadListMutex)) die("Error while releasing thread list lock.");
}

pthread_mutex_t symbolTableMutex = PTHREAD_MUTEX_INITIALIZER;
void acquireSymbolTableLock() {
  if (pthread_mutex_lock(&symbolTableMutex)) die("Error while acquiring symbol table lock.");
}
void releaseSymbolTableLock() {
  if (pthread_mutex_unlock(&symbolTableMutex)) die("Error while releasing symbol table lock.");
}

typedef struct {
  obj value;
  pthread_cond_t conditionVariable;
  pthread_mutex_t mutex;
  vector actor;
} promiseData;

promise newPromise() {
  promise p = makeVector(CELLS_REQUIRED_FOR_BYTES(sizeof(promiseData)));
  setVectorType(p, PROMISE);
  promiseData *pd = (promiseData *)vectorData(p);
  if (pthread_mutex_init(&pd->mutex, NULL))
    die("Error while initializing promise mutex.");
  if (pthread_cond_init(&pd->conditionVariable, NULL))
    die("Error while initializing promise condition variable.");
  pd->actor = NULL;
  return p;
}
obj promiseValue(promise p) {
  return ((promiseData *)vectorData(p))->value;
}
void markPromise(promise p) {
  mark(promiseValue(p));
  mark(((promiseData *)vectorData(p))->actor);  
}
void fulfillPromise(promise p, obj o) {
  promiseData *pd = (promiseData *)vectorData(p);
  if (pthread_mutex_lock(&pd->mutex)) die("Error while acquiring a promise lock before fulfillment.");
  if (!pd->value) pd->value = o;
  if (pthread_cond_broadcast(&pd->conditionVariable))
    die("Error while waking up threads waiting on a promise.");
  if (pthread_mutex_unlock(&pd->mutex)) die("Error while releasing a promise lock after fulfillment.");
  return;
}

typedef struct {
  vector frontOfQueue, backOfQueue;
  pthread_mutex_t queueLock;
  pthread_cond_t newMessageSignal;
  obj object, env, scope;
  vector threadData; // TODO: Deprecate the whole concept of thread data objects.
  jmp_buf toplevelEscape;
  promise currentPromise;
} actorData;

void markActor(vector a) {
  mark(((actorData *)vectorData(a))->frontOfQueue);
  mark(((actorData *)vectorData(a))->object);
}
void acquireQueueLock(actorData *ad) {
  pthread_mutex_lock(&ad->queueLock);
}
void releaseQueueLock(actorData *ad) {
  pthread_mutex_unlock(&ad->queueLock);
}
promise enqueueMessage(vector a, obj selector, vector args) {
  promise p = newPromise();
  actorData *ad = vectorData(((promiseData *)vectorData(p))->actor = a);
  acquireQueueLock(ad);
  vector v = newVector(4, NULL, p, selector, args);
  if (!ad->backOfQueue) {
    ad->frontOfQueue = ad->backOfQueue = v;
    if (pthread_cond_signal(&ad->newMessageSignal)) die("Error while waking up an actor.");
    // The actor will wake up when we release the lock.
  }
  else ad->backOfQueue = setIdx(ad->backOfQueue, 0, v);
  releaseQueueLock(ad);
  return p;
}
vector setContinuation(vector);
vector subexpressionContinuation(vector, obj, obj, obj, obj, vector, vector); // TODO: Reorganize.
vector addThread(vector); // TODO: Reorganize threading code around the new actor model...
vector setCurrentActor(vector, vector); // TODO: As above.
vector currentActor(vector); // TODO: As above.
void doNext(void); // TODO: As above.
void actorLoop(vector a) {
  actorData *ad = vectorData(a);
  setCurrentThread(ad->threadData);
  setCurrentActor(currentThread, a);
  for (;;) {
    acquireQueueLock(ad);
    while (!ad->frontOfQueue)
      if (pthread_cond_wait(&ad->newMessageSignal, &ad->queueLock))
        die("Error while trying to have an actor wait for new messages.");
    promise p = idx(ad->frontOfQueue, 1);
    obj selector = idx(ad->frontOfQueue, 2);
    vector args = idx(ad->frontOfQueue, 3);
    // We set up the continuation before removing the message from the queue, because otherwise we'd lose it if caught by a garbage collection.
    setContinuation(subexpressionContinuation(p, ad->scope, ad->env, ad->object, selector, args, 0));
    if (!(ad->frontOfQueue = idx(ad->frontOfQueue, 0))) ad->backOfQueue = NULL;
    // TODO: Remove the message from the queue after handling it, not before, so that acquiring the queue lock and finding the queue empty can be taken as proof that the actor is asleep (and can be garbage-collected if nothing is holding a reference to it).
    releaseQueueLock(ad);
    ad->currentPromise = p;
    setjmp(ad->toplevelEscape);
    doNext();
  }
}
void trimStack() { // TODO: Add noreturn assertion.
  longjmp(((actorData *)vectorData(currentActor(currentThread)))->toplevelEscape, 42); // Value returned through setjmp() will be ignored.
}
promise currentPromise() {
  return ((actorData *)vectorData(currentActor(currentThread)))->currentPromise;
}
vector newActor(obj o, obj scope, obj env) {
  vector a = makeAtomVector(CELLS_REQUIRED_FOR_BYTES(sizeof(actorData)));
  actorData *ad = vectorData(a);
  if (pthread_mutex_init(&ad->queueLock, NULL))
    die("Error while initializing an actor's message queue lock.");
  if (pthread_cond_init(&ad->newMessageSignal, NULL))
    die("Error while initializing an actor's condition variable.");
  ad->object = o;
  ad->scope = scope;
  ad->env = env;
  ad->threadData = addThread(garbageCollectorRoot);
  // Queue pointers have already been initialized to NULL by makeAtomVector().
  setVectorType(a, ACTOR);
  createPrimitiveThread((void (*)(void *))actorLoop, a);
  return a;
}

obj waitFor(void *e) {
  if (!isPromise(e)) return e;
  promiseData *pd = (promiseData *)vectorData(e);
  if (pthread_mutex_lock(&pd->mutex)) die("Error while acquiring a promise lock before a wait.");
  while (!pd->value)
    if (pthread_cond_wait(&pd->conditionVariable, &pd->mutex))
      die("Error while attempting to wait on a promise.");
  if (pthread_mutex_unlock(&pd->mutex)) die("Error while releasing a promise lock after a wait.");
  return pd->value;
}

pthread_mutex_t GCLock = PTHREAD_MUTEX_INITIALIZER;
void forbidGC() {
  if (pthread_mutex_lock(&GCLock)) die("Error while acquiring GC mutex.");
}
void permitGC() {
  if (pthread_mutex_unlock(&GCLock)) die("Error while releasing GC mutex.");
}


void createPrimitiveThread(void (*f)(void *), void *a) {
  pthread_t thread;
  if (pthread_create(&thread, NULL, (void *(*)(void *))f, a)) die("Failed spawning.");
}

vector suffix(void *e, vector v) {
  int l = vectorLength(v);
  vector nv = makeVector(l + 1);
  nv->data[l] = e;
  memcpy(nv->data, v->data, l * sizeof(void *));
  return nv;
}
vector prefix(void *e, vector v) {
  int l = vectorLength(v);
  vector nv = makeVector(l + 1);
  setIdx(nv, 0, e);
  memcpy((void *)vectorData(nv) + sizeof(void *), vectorData(v), l * sizeof(void *));
  return nv;
}


obj newObject(obj proto, vector slotNames, vector slotValues, void *hidden) {
  int count = vectorLength(slotNames);
  vector slots = makeVector(count * 3);
  for (int i = 0; i < count; ++i) {
    setIdx(slots, i * 3, idx(slotNames, i));
    setIdx(slots, i * 3 + 1, idx(slotValues, i));
    setIdx(slots, i * 3 + 2, oNamespaceCanon);
  }
  return newVector(4, proto, slots, hidden, 0);
}

vector  proto(obj o)          { return     idx(o, 0);     }
vector  slots(obj o)          { return     idx(o, 1);     }
vector  hiddenEntity(obj o)   { return     idx(o, 2);     }
void   *hiddenAtom(obj o)     { return idx(idx(o, 2), 0); }
obj     dispatchMethod(obj o) { return     idx(o, 3);     }

obj setProto(obj o, obj p) {
  setIdx(o, 0, p);
  return o;
}
void setSlots(obj o, vector s) {
  setIdx(o, 1, s);
}
vector setHiddenData(obj o, vector h) {
  return setIdx(o, 2, h);
}
void setDispatchMethod(obj o, obj dm) {
  setIdx(o, 3, dm);
}

int slotCount(obj o) {
  return vectorLength(slots(o)) / 3;
}
obj slotName(obj o, int i) {
  return idx(slots(o), i * 3);
}
void **slotValuePointer(obj o, int i) {
  return (void **)idxPointer(slots(o), i * 3 + 1);
}
obj slotNamespace(obj o, int i) {
  return idx(slots(o), i * 3 + 2);
}

obj newSlot(obj o, obj s, void *v, obj namespace) {
  vector oldSlots = slots(o);
  int length = vectorLength(oldSlots);
  vector newSlots = makeVector(length + 3);
  memcpy(vectorData(newSlots), vectorData(oldSlots), length * sizeof(obj));
  setIdx(newSlots, length,     s);
  setIdx(newSlots, length + 1, v);
  setIdx(newSlots, length + 2, namespace);
  setSlots(o, newSlots);
  return v;
}

obj slotlessObject(obj proto, vector hidden) {
  return newObject(proto, emptyVector, emptyVector, hidden);
}
obj typedObject(obj proto, vector hidden) {
  obj o = slotlessObject(proto, hidden);
  setVectorType(o, vectorType(proto)); // Inherit the primitive type of our proto.
  return o;
}

obj primitive(void *code) {
  obj o = slotlessObject(oPrimitive, newAtomVector(1, code));
  setVectorType(o, PRIMITIVE);
  return o;
}
int (*primitiveCode(obj p))(void) {
  return hiddenAtom(p);
}

obj method(vector params, vector body) {
  obj o = slotlessObject(oMethod, newVector(2, params, body));
  setVectorType(o, METHOD);
  return o;
}
vector methodParams(obj c) { return idx(hiddenEntity(c), 0); }
vector methodBody(obj c)   { return idx(hiddenEntity(c), 1); }

obj stackFrame(obj parent, vector names, vector values, vector continuation) {
  obj o = newObject(parent, names, values, continuation);
  setVectorType(o, STACK_FRAME);
  return o;
}
vector stackFrameContinuation(obj sf) {
  return hiddenEntity(sf);  
}

obj vectorObject(vector v) {
  obj vo = slotlessObject(oVector, v);
  setVectorType(vo, VECTOR);
  return vo;
}
obj vectorObjectVector(obj v) {
  return hiddenEntity(v);
}

// Certain objects have to be given special type tags.
// TODO: Make it possible to specify this in the builtins file.
void initializePrototypeTags() {
  setVectorType(oPrimitive, PRIMITIVE);
  setVectorType(oMethod, METHOD);
  setVectorType(oVector, VECTOR);
  setVectorType(oDynamicEnvironment, ENVIRONMENT);
  setVectorType(oInteger, BIGNUM);
  setVectorType(oRegex, REGEX);
}

