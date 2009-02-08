%{

/*
    Copyright 2008, 2009 Sam Chapin

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
#include "objects.h"
#include <stdio.h>

obj keywordMessage(obj target, pair args);
void yyerror(YYLTYPE *, int *, int *, void **, void *, char const *);

#define fail(...) do { \
  printf(__VA_ARGS__); \
  fflush(stdout); \
  *parserOutput = oNull; \
  YYABORT; \
} while (0)

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

%token ESCAPE

%token ENDOFFILE

%glr-parser
%locations
%pure-parser
%lex-param {int *nesting}
%lex-param {void *scanner}

%parse-param {int *lineNumber}
%parse-param {int *nesting}
%parse-param {void **parserOutput}
%parse-param {void *scanner}

%initial-action { @$.first_line = @$.last_line = *lineNumber; }

%%

input:
  gap ENDOFFILE
  { *parserOutput = 0; YYACCEPT; }
| gap
  { *parserOutput = oNull; *lineNumber = @1.last_line; YYACCEPT; }
| gap line gap
  { *parserOutput = $2; *lineNumber = @3.last_line; YYACCEPT; }
| error
  { *parserOutput = oNull; *lineNumber = @1.last_line; YYABORT; }
;

line:
  statements
  { $$ = expressionSequence(listToVector(nreverse($1))); }
;
statements:
  statement
  { $$ = list($1); }
| statements '.' statement
  { $$ = cons($3, $1); }
;

declaration:
  optarget signature '{' body '}'
  { pair sig = $2;
    $$ = message($1,
                 sAddSlot_as_,
                 newVector(2,
                           car(sig),
                           block(listToVector(cons(sSelf, cdr(sig))),
                                 listToVector($4)))); }
;
statement:
  expr
| carets
  { $$ = message(0, sReturn_atDepth_, newVector(2, oNull, integer((int)$1))); }
| carets expr
  { $$ = message(0, sReturn_atDepth_, newVector(2, $2, integer((int)$1))); }
;
carets:
  '^'
  { $$ = (void *)1; }
| carets '^'
  { $$ = $1 + 1; }
;
signature:
  NAME
  { $$ = list($1); }
| OPERATOR signatureparam
  { $$ = cons($1, list($2)); }
| keywordsignature
  { pair keywords = nreverse($1);
    $$ = cons(appendSymbols(map(car, keywords)), map(cdr, keywords)); }
;
keywordsignature:
  KEYWORD gap signatureparam
  { $$ = list(cons($1, $3)); }
| keywordsignature KEYWORD gap signatureparam
  { $$ = cons(cons($2, $4), $1); }
;
signatureparam:
  NAME
| ESCAPE
;

expr:
  arrow
| dependency
;

parens:
 '(' body ')'
  { $$ = expressionSequence(listToVector($2)); }
;

arrow:
  dependency ARROW gap expr
  { $$ = message(slotlessObject(oArrowCode, $4), sFrom_, newVector(1, $1)); }
;
dependency:
  parens
| literal
| name
| operation
| msg
| assignment
| declaration
;

assignment:
  target param ADDSLOT gap dependency
  { $$ = message($1, sAddSlot_as_, newVector(2, $2, $5)); }
| target param SETSLOT gap dependency
  { $$ = message($1, sSetSlot_to_, newVector(2, $2, $5)); }
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
  { $$ = message($1, $2, emptyVector); }
| nametarget '@' NAME
  { $$ = promiseCode(message($1, $3, emptyVector)); }
| nametarget ESCAPE
  { $$ = message($1, sContentsOfSlot_, newVector(1, $2)); }
;
nametarget:
  { $$ = 0; }
| parens
| literal
| name
| declaration
| cascade
;

impliedoperation:
  OPERATOR gap operand
  { $$ = message(0, $1, newVector(1, $3)); }
| '@' gap OPERATOR gap operand
  { $$ = promiseCode(message(0, $3, newVector(1, $5))); }
;

cascade:
  cascader ';'
  { $$ = cascade($1); }
;
cascader:
  name
| operation
| msg
| assignment
| declaration
| '(' cascadeBody ')'
  { $$ = $2; }
;
cascadeBody:
  cascader
  /* TODO: Allow as a cascader any parenthesized statement list whose last statement is itself a valid
           cascader. Doing this cleanly seems to indicate changing "bodies" from an internal message
           to a new type of AST object, which is probably overdue anyway. */
;

operation:
  optarget OPERATOR gap operand
  { $$ = message($1, $2, newVector(1, $4)); }
| optarget '@' gap OPERATOR gap operand
  { $$ = promiseCode(message($1, $4, newVector(1, $6))); }
;
optarget:
  nametarget
| operation
;
operand:
  parens
| literal
| name
| impliedoperation
| impliedmsg
;

impliedmsg:
  keywords
  { $$ = keywordMessage(0, nreverse($1)); }
| '@' gap keywords
  { $$ = promiseCode(keywordMessage(0, nreverse($3))); }
;

msg:
  optarget keywords
  { $$ = keywordMessage($1, nreverse($2)); }
| optarget '@' gap keywords
  { $$ = promiseCode(keywordMessage($1, nreverse($4))); }
| optarget KEYWORD gap impliedoperation
  { $$ = message($1, $2, newVector(1, $4)); }
| optarget '@' gap KEYWORD impliedmsg
  { $$ = promiseCode(message($1, $4, newVector(1, $5))); }
;
keywords:
  KEYWORD gap msgarg
  { $$ = list(cons($1, $3)); }
| keywords KEYWORD gap msgarg
  { $$ = cons(cons($2, $4), $1); }
;
msgarg:
  operand
| operation
;

literal:
  INTEGER
| SYMBOL
| STRING
| '[' gap ']'
  { $$ = vectorObject(emptyVector); }
| '[' gap list gap ']'
  { $$ = message(oInternals, sVectorLiteral, listToVector(nreverse($3))); }
| '{' body '}'
  { $$ = block(newVector(1, sCurrentMessageTarget), listToVector($2)); }
| '{' params '|' body '}'
  { $$ = block(listToVector(cons(sCurrentMessageTarget, nreverse($2))),
               listToVector($4)); }
;
list:
  expr
  { $$ = list($1); }
| list gap ',' gap expr
  { $$ = cons($5, $1); }
;
params:
  { $$ = emptyList; }
| params param
  { $$ = cons($2, $1); }
;
param:
  NAME
| OPERATOR
| KEYWORD
| KEYWORDS
;

body:
  gap
  { $$ = list(oNull); }
| gap lines gap
  { $$ = nreverse($2); }
;
lines:
  line
  { $$ = list($1); }
| lines '\n' line
  { $$ = cons($3, $1); }
;

%%

obj keywordMessage(obj target, pair args) {
  return message(target,
                 appendSymbols(map(car, args)),
                 listToVector(map(cdr, args)));
}

void yyerror (YYLTYPE *location,
              int *line,
              int *nesting,
              void **parserOutput,
              void *scanner,
              char const *msg) {
  if (location->first_line == (*line = location->last_line))
    printf("\nSyntax error at line %d.\n", location->last_line);
  else
    printf("\nSyntax error between lines %d and %d.\n", location->first_line, location->last_line);
}
