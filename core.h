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

//int (*yylex)(YYSTYPE *, YYLTYPE *, int *, void *);
int mainLexer(YYSTYPE *, YYLTYPE *, int *, void *);
#define YY_DECL int yylex(YYSTYPE *value, YYLTYPE *location, int *nesting, yyscan_t yyscanner)
int yylex(YYSTYPE *, YYLTYPE *, int *, void *);

#include "gc.h"

life temp(void);
void invalidateTemporaryLife(void);

obj symbol(life, const char *c);
obj appendSymbols(life, vector);

obj message(life, obj, obj, vector);

obj block(life, vector, vector);

vector emptyVector;

vector vectorAppend(life, vector, vector);

obj appendStrings(life, obj, obj);

obj promiseCode(life, obj);

obj expressionSequence(life, vector);

obj string(life, const char *);
obj vectorObject(life, vector);

typedef vector pair;
#define emptyList 0
pair cons(life, void *, void *);
void *car(pair);
void *cdr(pair);
vector setcar(pair, void *);
vector setcdr(pair, void *);
pair list(life, void *);
int length(pair);
pair listToVector(life, pair);
pair map(life, void *, pair);
pair nreverse(pair);

int isEmpty(vector);

void dispatch(vector);

void setupInterpreter(void);
void *loadFile(life, const char *);
void REPL(void);

#endif
