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

#ifndef CORE_H
#define CORE_H

// These need to be seen by both Flex and Bison.
#define YYSTYPE void *
#define YYLTYPE YYLTYPE
typedef struct YYLTYPE {
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;

#include <stdio.h>

#define YY_DECL int yylex(YYSTYPE *value, YYLTYPE *location, int *nesting, yyscan_t yyscanner)
int yylex(YYSTYPE *, YYLTYPE *, int *, void *);

#include "gc.h"

obj symbol(const char *c);
obj appendSymbols(vector);

obj message(obj, obj, vector);

obj blockLiteral(vector, vector);
obj block(obj, obj);

obj cascade(obj);
obj quote(obj);

vector emptyVector;

vector vectorAppend(vector, vector);

obj appendStrings(obj, obj);

obj promiseCode(obj);
obj targetCascade(obj, obj);

obj expressionSequence(vector);

obj string(const char *);
obj vectorObject(vector);

typedef vector pair;
#define emptyList 0
pair cons(void *, void *);
void *car(pair);
void *cdr(pair);
vector setcar(pair, void *);
vector setcdr(pair, void *);
pair list(void *);
int length(pair);
pair listToVector(pair);
pair map(void *, pair);
pair nreverse(pair);

int isEmpty(vector);

void dispatch(void);

obj callWithEnvironment(obj, obj, obj, vector);

void setupInterpreter(void);
void *loadFile(const char *, obj, obj);

#endif
