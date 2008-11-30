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

#include "gc.h"

typedef vector continuation;

vector newThreadData(vector *life,
                     vector stack,
                     obj dynamicEnv,
                     vector cc,
                     vector prev,
                     vector next,
                     vector scratch) {
  return newVector(life, 6, stack, dynamicEnv, cc, prev, next, scratch);
}

continuation threadContinuation(vector td) { return idx(td, 2); }
vector       previousThreadData(vector td) { return idx(td, 3); }
vector       nextThreadData(vector td)     { return idx(td, 4); }
vector *edenRoot(vector td) {
  return idxPointer(td, 5);
}
vector       setPreviousThreadData(vector td, vector ptd) { return setIdx(td, 3, ptd); } 
vector       setNextThreadData(vector td, vector ntd)     { return setIdx(td, 4, ntd); }

void allocateThreadStack(vector td) {
  setIdx(td, 0, makeAtomVector(edenRoot(td), STACKDEPTH));
}

vector createGarbageCollectorRoot(vector *eden) {
  vector garbageCollectorRoot = newThreadData(eden, 0, 0, 0, 0, 0, 0);
  setNextThreadData(garbageCollectorRoot,
                    setPreviousThreadData(garbageCollectorRoot, garbageCollectorRoot));
  allocateThreadStack(garbageCollectorRoot);
  mark(garbageCollectorRoot);
  return garbageCollectorRoot;
}

vector setContinuation(vector threadData, continuation c) {
  setIdx(threadData, 2, c);
  return threadData;
}

// Used for stashing important objects in the garbageCollectorRoot vector to keep them alive.
vector *symbolTableShelter(vector root) { return idxPointer(root, 1); }
vector *lobbyShelter(vector root)       { return idxPointer(root, 2); }

vector addThread(vector *life, vector root) {
  acquireThreadListLock();
  vector next = nextThreadData(root),
         new = newThreadData(life, 0, 0, 0, root, next, 0);
  setNextThreadData(root, setPreviousThreadData(next, new));
  releaseThreadListLock();
  allocateThreadStack(new);
  return new;
}
void *topOfStack(vector threadData) {
  return &((vector)idx(threadData, 0))->data[STACKDEPTH - 1];
}

void killThreadData(vector td) {
  // Remove td from the doubly-linked list of live threadData objects.
  acquireThreadListLock();
  setNextThreadData(previousThreadData(td), nextThreadData(td));
  setPreviousThreadData(nextThreadData(td), previousThreadData(td));
  releaseThreadListLock();
}
vector setShelter(vector t, vector v) {
  return setIdx(t, 5, v);
}

void keep(vector thread, promise p, vector o) {
  fulfillPromise(p, o);
  killThreadData(thread);
  // The return from this function is intended to terminate the thread as per clone(2).
  // It must therefore only be called in tail context.
}
