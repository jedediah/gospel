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
vector blackList, grayList, whiteList, ecruList;

typedef vector *life;

vector emptyVector;

int freeSpaceCount(void);

vector makeVector(life, int);
vector makeAtomVector(life, int);

vector newVector(life, int, ...);
vector newAtomVector(life, int, ...);

vector duplicateVector(life, vector);

void *idx(vector, int);
vector *idxPointer(vector, int);
vector *edenIdx(vector, int);
void *setIdx(vector, int, void *);
void *vectorData(vector);

int vectorLength(vector);

vector zero(vector);

void forbidGC(void);
void permitGC(void);

void spawn(void *, void *, void *);

typedef vector promise;
promise newPromise(life);
vector promiseValue(promise);
vector *promiseValueField(promise);
vector waitFor(void *);
void fulfillPromise(promise, vector);

typedef vector channel;
channel newChannel(life, vector);
vector channelTarget(channel);
int *channelCount(channel);
int *channelFlag(channel);

void acquireChannelLock(channel);
void releaseChannelLock(channel);

void initializeHeap(void);

void collectGarbage(void);
void requireGC(void);

vector garbageCollectorRoot;

typedef vector obj;

int isPromise(vector);
int isChannel(vector);
int isInteger(obj);
int isPrimitive(obj);
int isClosure(obj);
int isString(obj);
int isSymbol(obj);
int isStackFrame(obj);

vector suffix(life, void *, vector);
vector prefix(life, void *, vector);

vector class(obj);
vector instance(obj);
vector hiddenEntity(obj);
void *hiddenAtom(obj);
void *setSlotByIndex(obj, int, obj);
void setSlotNames(life, obj, vector);
void setSlotValues(obj, vector);
int slotCount(obj);
void *slotName(obj, int);
vector slotNameVector(life, obj o);
void setClass(obj, vector);
void setInstance(obj, vector);
void setHiddenData(obj, vector);
obj proto(obj);
obj setProto(life, obj, obj);
obj newObject(life, obj, vector, vector, void *);
obj slotlessObject(life, obj, vector);
obj fixnumObject(life, obj, int);

obj newClosure(life, obj, vector, vector);
vector closureEnv(obj);
vector closureParams(obj);
vector closureBody(obj);

obj primitive(life, void *);
obj integer(life, int);
int integerValue(obj);

obj stackFrame(life, obj, vector, vector, vector);
vector stackFrameContinuation(obj);

void initializePrototypeTags();

#endif
