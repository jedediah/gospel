/*
    Copyright © 2008 Sam Chapin

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

vector newThreadData(vector cc,
                     vector prev,
                     vector next,
                     vector scratch) {
  return newVector(5, cc, prev, next, scratch, 0);
}

continuation threadContinuation(vector td) { return idx(td, 0); }
vector       previousThreadData(vector td) { return idx(td, 1); }
vector       nextThreadData(vector td)     { return idx(td, 2); }
vector       shelteredValue(vector td)     { return idx(td, 3); }
vector       currentActor(vector td)       { return idx(td, 4); }

vector setContinuation(continuation c) {
  setIdx(currentThread, 0, c);
  return currentThread;
}
vector setPreviousThreadData(vector td, vector ptd) { return setIdx(td, 1, ptd); }
vector setNextThreadData(vector td, vector ntd)     { return setIdx(td, 2, ntd); }
vector shelter(vector td, vector v)                 { return setIdx(td, 3, v);   }
vector setCurrentActor(vector td, vector a)         { return setIdx(td, 4, a);   }

vector createGarbageCollectorRoot(obj rootLiveObject) {
  vector root = newThreadData(rootLiveObject, 0, 0, 0);
  mark(setNextThreadData(root, setPreviousThreadData(root, root)));
  return root;
}

vector addThread(vector root) {
  acquireThreadListLock();
  vector next = nextThreadData(root),
         new = newThreadData(0, root, next, 0);
  setNextThreadData(root, setPreviousThreadData(next, new));
  releaseThreadListLock();
  return new;
}

void killThreadData(vector td) {
  // Remove td from the doubly-linked list of live threadData objects.
  acquireThreadListLock();
  setNextThreadData(previousThreadData(td), nextThreadData(td));
  setPreviousThreadData(nextThreadData(td), previousThreadData(td));
  releaseThreadListLock();
}

