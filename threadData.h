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

#ifndef THREADDATA_H
#define THREADDATA_H

#include "gc.h"

vector shelteredValue(vector);
vector addThread(vector);
void killThreadData(vector);
vector shelter(vector, vector);

vector currentActor(vector);
vector setCurrentActor(vector, vector);

vector threadContinuation(vector);
vector setContinuation(vector);

vector createGarbageCollectorRoot(obj);

void fulfillPromise(promise, vector);

#endif
