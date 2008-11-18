%{

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

#include "core.h"
#include "death.h"
#include "objects.h"
#include <stdio.h>

obj keywordMessage(obj target, pair args);
void yyerror(const char *s);

%}

%token INTEGER
%token SYMBOL
%token STRING

%token NAME
%token OPERATOR
%token KEYWORD
%token KEYWORDS
%token ARROW

%token ADDSLOT
%token SETSLOT

%token INTERACTIVE
%token BATCH

%glr-parser

%%

input:
  INTERACTIVE gap line '\n'
  { parserOutput = $3; YYACCEPT; }
| BATCH body
  { parserOutput = $2; YYACCEPT; }
| INTERACTIVE error '\n'
  { die("Syntax error at REPL."); }
| BATCH error
  { die("Syntax error in batch mode."); }
;

line:
  statements
  { $$ = expressionSequence(temp(), listToVector(temp(), nreverse($1))); }
;
statements:
  statement
  { $$ = list(temp(), $1); }
| statements '.' statement
  { $$ = cons(temp(), $3, $1); }
;

statement:
  expr
| carets
  { $$ = message(temp(), 0, sReturn_atDepth_, newVector(temp(), 2, oNull, integer(temp(), (int)$1))); }
| carets expr
  { $$ = message(temp(), 0, sReturn_atDepth_, newVector(temp(), 2, $2, integer(temp(), (int)$1))); }
| optarget signature '{' body '}'
  { pair sig = $2;
    $$ = message(temp(),
                 $1,
                 sAddSlot_as_,
                 newVector(temp(),
                           2,
                           car(sig),
                           block(temp(), listToVector(temp(), cdr(sig)), listToVector(temp(), $4)))); }
;
carets:
  '^'
  { $$ = (void *)1; }
| carets '^'
  { $$ = $1 + 1; }
;
signature:
  NAME
  { $$ = list(temp(), $1); }
| OPERATOR param
  { $$ = cons(temp(), $1, list(temp(), $2)); }
| keywordsignature
  { pair keywords = nreverse($1);
    $$ = cons(temp(), appendSymbols(temp(), map(temp(), car, keywords)), map(temp(), cdr, keywords)); }
;
keywordsignature:
  KEYWORD gap param
  { $$ = list(temp(), cons(temp(), $1, $3)); }
| keywordsignature KEYWORD gap param
  { $$ = cons(temp(), cons(temp(), $2, $4), $1); }
;

expr:
  parens
| literal
| name
| operation
| msg
| assignment
| arrow
;

parens:
  '(' line ')'
  { $$ = $2; }
;

arrow:
  dependency ARROW gap expr
  { $$ = arrowCode(temp(), $1, $4) ; }
;
dependency:
  parens
| literal
| name
| operation
| msg
| assignment
;

assignment:
  target param ADDSLOT gap dependency
  { $$ = message(temp(), $1, sAddSlot_as_, newVector(temp(), 2, $2, $5)); }
| target param SETSLOT gap dependency
  { $$ = message(temp(), $1, sSetSlot_to_, newVector(temp(), 2, $2, $5)); }
;
target:
  optarget
| msg
;

gap:
| gap '\n'
;

name:
  nametarget NAME
  { $$ = message(temp(), $1, $2, emptyVector); }
| nametarget '@' NAME
  { $$ = promiseCode(temp(), message(temp(), $1, $3, emptyVector)); }
;
nametarget:
  { $$ = 0; }
| parens
| literal
| name
;

operation:
  optarget OPERATOR gap operand
  { $$ = message(temp(), $1, $2, newVector(temp(), 1, $4)); }
| optarget '@' OPERATOR gap operand
  { $$ = promiseCode(temp(), message(temp(), $1, $3, newVector(temp(), 1, $5))); }
;
optarget:
  nametarget
| operation
;
operand:
  parens
| literal
| name
;

msg:
  optarget keywords
  { $$ = keywordMessage($1, nreverse($2)); }
| optarget '@' gap keywords
  { $$ = promiseCode(temp(), keywordMessage($1, nreverse($4))); }
;
keywords:
  KEYWORD gap msgarg
  { $$ = list(temp(), cons(temp(), $1, $3)); }
| keywords KEYWORD gap msgarg
  { $$ = cons(temp(), cons(temp(), $2, $4), $1); }
;
msgarg:
  operand
| operation
;

literal:
  INTEGER
| SYMBOL
| STRING
| '{' body '}'
  { $$ = block(temp(), emptyVector, listToVector(temp(), $2)); }
| '{' params '|' body '}'
  { $$ = block(temp(), listToVector(temp(), nreverse($2)), listToVector(temp(), $4)); }
| '\\' param
  { $$ = message(temp(), 0, sContentsOfSlot_, newVector(temp(), 1, $2)); }
| '\\' '(' target param ')'
  { $$ = message(temp(), $3, sContentsOfSlot_, newVector(temp(), 1, $4)); }
;
params:
  { $$ = emptyList; }
| params param
  { $$ = cons(temp(), $2, $1); }
;
param:
  NAME
| OPERATOR
| KEYWORD
| KEYWORDS
;

body:
  gap line
  { $$ = list(temp(), $2); }
| gap lines
  { $$ = nreverse($2); }
;
lines:
  { $$ = emptyList; }
| lines line '\n'
  { $$ = cons(temp(), $2, $1); }
;

%%

obj keywordMessage(obj target, pair args) {
  return message(temp(),
                 target,
                 appendSymbols(temp(), map(temp(), car, args)),
                 listToVector(temp(), map(temp(), cdr, args)));
}

void yyerror(const char *s) {
  //die("Bison sez: %s.\n", s);
}
