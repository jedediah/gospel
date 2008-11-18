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

vector *edenRoot(vector);
vector addThread(vector *, vector);
void *topOfStack(vector);
void killThreadData(vector);
vector setShelter(vector, vector);

vector threadContinuation(vector);
vector setContinuation(vector, vector);

vector createGarbageCollectorRoot(vector *);
vector *symbolTableShelter(vector);
vector *lobbyShelter(vector);

void keep(vector, promise, vector);
