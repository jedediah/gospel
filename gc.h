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

#ifndef GC_H
#define GC_H

// A compiler hint.
#define tailcall(f, ...) do { (f)(__VA_ARGS__); return; } while (0)

// The maximum depth, in cells, of the C stack required by each interpreter instance.
#define STACKDEPTH 2048

#define CELLS_REQUIRED_FOR_BYTES(n) (((n) + sizeof(int) - 1) / sizeof(int))

#define EDEN_OVERHEAD 5 // For the sake of testing.

typedef struct vectorStruct {
  struct vectorStruct *prev, *next;
  int type;
  void *data[];
} *vector;

int isMarked(vector);
void mark(vector);

// The following are only exposed because they're used in unit tests.
void scan(void);
void acquireFutex(volatile int *, volatile int *);
void releaseFutex(volatile int *, volatile int *);
vector insertBetween(vector, vector, vector);
extern vector blackList, grayList, whiteList, ecruList;

extern vector emptyVector;
extern __thread vector currentThread;
extern vector garbageCollectorRoot;

void acquireThreadListLock(void);
void releaseThreadListLock(void);
void acquireSymbolTableLock(void);
void releaseSymbolTableLock(void);
void acquireTempLock(void);
void releaseTempLock(void);
int freeSpaceCount(void);

void invalidateEden(void);

vector makeVector(int);
vector makeAtomVector(int);

vector newVector(int, ...);
vector newAtomVector(int, ...);

vector duplicateVector(vector);

void *idx(vector, int);
vector *idxPointer(vector, int);
vector *edenIdx(vector, int);
void *setIdx(vector, int, void *);
void *vectorData(vector);

int vectorLength(vector);

vector zero(vector);

void forbidGC(void);
void permitGC(void);

void spawn(void *, void *);
void explicitlyEndThread(void);

typedef vector obj;

typedef vector promise;
promise newPromise(void);
obj promiseValue(promise);
vector *promiseValueField(promise);
obj waitFor(void *);
void fulfillPromise(promise, obj);

typedef vector channel;
channel newChannel(vector);
vector channelTarget(channel);
int *channelCount(channel);
int *channelFlag(channel);

void acquireChannelLock(channel);
void releaseChannelLock(channel);

void initializeHeap(void);

void collectGarbage(void);
void requireGC(void);

int isPromise(vector);
int isChannel(vector);
int isInteger(obj);
int isPrimitive(obj);
int isClosure(obj);
int isString(obj);
int isSymbol(obj);
int isStackFrame(obj);
int isVectorObject(obj);

int vectorType(vector);
void setVectorType(vector, int);

vector suffix(void *, vector);
vector prefix(void *, vector);

obj dispatchMethod(obj);
void setDispatchMethod(obj, obj);

obj newSlot(obj, obj, void *, obj);
vector hiddenEntity(obj);
void *hiddenAtom(obj);
void setSlots(obj, vector);
int slotCount(obj);
obj slotName(obj, int);
obj slotNamespace(obj, int);
void **slotValuePointer(obj, int);
vector setHiddenData(obj, vector);
obj proto(obj);
obj setProto(obj, obj);
obj newObject(obj, vector, vector, void *);
obj slotlessObject(obj, vector);
obj fixnumObject(obj, int);

obj newClosure(obj, vector, vector);
vector closureEnv(obj);
vector closureParams(obj);
vector closureBody(obj);

obj primitive(void *);
void (*primitiveCode(obj))(vector);
obj integer(int);
int integerValue(obj);

obj stackFrame(obj, vector, vector, vector);
vector stackFrameContinuation(obj);

void initializePrototypeTags(void);

#endif
