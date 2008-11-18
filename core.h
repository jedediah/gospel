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

// An interface for the parser.

#ifndef CORE_H
#define CORE_H

#define YYSTYPE void * // This needs to be seen by both Flex and Bison.

#include <stdio.h>

// Void pointers are actually YY_BUFFER_STATE.
void *openInputFile(FILE *);
void closeInputFile(void *);

int (*yylex)(void);
#define YY_DECL int mainLexer()
YY_DECL;

#include "gc.h"

life temp(void);
void invalidateTemporaryLife(void);

typedef vector obj;

obj symbol(life, const char *c);
obj appendSymbols(life, vector);

obj message(life, obj, obj, vector);

obj block(life, vector, vector);

obj parserOutput;
vector parserThreadData;

vector emptyVector;

vector vectorAppend(life, vector, vector);

obj appendStrings(life, obj, obj);

obj primitive(life, void *);

obj promiseCode(life, obj);

obj arrowCode(life, obj, obj);
obj arrowPromiser(obj);
obj arrowFollower(obj);

obj expressionSequence(life, vector);

obj string(life, const char *);
obj integer(life, int);
int integerValue(obj);

typedef vector pair;
#define emptyList 0
pair cons(life, void *, void *);
void *car(pair);
void *cdr(pair);
vector setcar(pair, void *);
vector setcdr(pair, void *);
pair list(life, void *);
pair listToVector(life, pair);
pair map(life, void *, pair);
pair nreverse(pair);

int isPrimitive(obj);
int isClosure(obj);

int isEmpty(vector);

void dispatch(vector);

vector live;

void setupInterpreter(void);
void loadFile(const char *);
void REPL(void);

#endif
