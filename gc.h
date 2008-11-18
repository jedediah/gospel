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
void mark(vector v);

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
int isPromise(void *);
promise newPromise(life);
vector promiseValue(promise);
vector *promiseValueField(promise);
vector waitFor(void *);
void fulfillPromise(promise, vector);

typedef vector channel;
channel newChannel(life, vector);
int isChannel(vector);
vector channelTarget(channel);
int *channelCount(channel);
int *channelFlag(channel);

void acquireChannelLock(channel);
void releaseChannelLock(channel);

void initializeHeap(void);

void collectGarbage(void);
void requireGC(void);

vector garbageCollectorRoot;

#endif
