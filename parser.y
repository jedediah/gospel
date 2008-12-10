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
  { $$ = expressionSequence(temp(), listToVector(temp(), nreverse($1))); }
;
statements:
  statement
  { $$ = list(temp(), $1); }
| statements '.' statement
  { $$ = cons(temp(), $3, $1); }
;

declaration:
  optarget signature '{' body '}'
  { pair sig = $2;
    $$ = message(temp(),
                 $1,
                 sAddSlot_as_,
                 newVector(temp(),
                           2,
                           car(sig),
                           block(temp(),
                                 listToVector(temp(), cons(temp(), sSelf, cdr(sig))),
                                 listToVector(temp(), $4)))); }
;
statement:
  expr
| carets
  { $$ = message(temp(), 0, sReturn_atDepth_, newVector(temp(), 2, oNull, integer(temp(), (int)$1))); }
| carets expr
  { $$ = message(temp(), 0, sReturn_atDepth_, newVector(temp(), 2, $2, integer(temp(), (int)$1))); }
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
| OPERATOR signatureparam
  { $$ = cons(temp(), $1, list(temp(), $2)); }
| keywordsignature
  { pair keywords = nreverse($1);
    $$ = cons(temp(), appendSymbols(temp(), map(temp(), car, keywords)), map(temp(), cdr, keywords)); }
;
keywordsignature:
  KEYWORD gap signatureparam
  { $$ = list(temp(), cons(temp(), $1, $3)); }
| keywordsignature KEYWORD gap signatureparam
  { $$ = cons(temp(), cons(temp(), $2, $4), $1); }
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
  { $$ = expressionSequence(temp(), listToVector(temp(), $2)); }
;

arrow:
  dependency ARROW gap expr
  { $$ = message(temp(), slotlessObject(temp(), oArrowCode, $4), sFrom_, newVector(temp(), 1, $1)); }
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
| nametarget ESCAPE
  { $$ = message(temp(), $1, sContentsOfSlot_, newVector(temp(), 1, $2)); }
;
nametarget:
  { $$ = 0; }
| parens
| literal
| name
| declaration
;

impliedoperation:
  OPERATOR gap operand
  { $$ = message(temp(), 0, $1, newVector(temp(), 1, $3)); }
| '@' gap OPERATOR gap operand
  { $$ = promiseCode(temp(), message(temp(), 0, $3, newVector(temp(), 1, $5))); }
;
 
operation:
  optarget OPERATOR gap operand
  { $$ = message(temp(), $1, $2, newVector(temp(), 1, $4)); }
| optarget '@' gap OPERATOR gap operand
  { $$ = promiseCode(temp(), message(temp(), $1, $4, newVector(temp(), 1, $6))); }
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
  { $$ = promiseCode(temp(), keywordMessage(0, nreverse($3))); }
;

msg:
  optarget keywords
  { $$ = keywordMessage($1, nreverse($2)); }
| optarget '@' gap keywords
  { $$ = promiseCode(temp(), keywordMessage($1, nreverse($4))); }
| optarget KEYWORD gap impliedoperation
  { $$ = message(temp(), $1, $2, newVector(temp(), 1, $4)); }
| optarget '@' gap KEYWORD impliedmsg
  { $$ = promiseCode(temp(), message(temp(), $1, $4, newVector(temp(), 1, $5))); }
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
| '[' ']'
  { $$ = vectorObject(temp(), emptyVector); }
| '[' gap list gap ']'
  { $$ = message(temp(), oInternals, sVectorLiteral, listToVector(temp(), nreverse($3))); }
| '{' body '}'
  { $$ = block(temp(), newVector(temp(), 1, sCurrentMessageTarget), listToVector(temp(), $2)); }
| '{' params '|' body '}'
  { $$ = block(temp(),
               listToVector(temp(), cons(temp(), sCurrentMessageTarget, nreverse($2))),
               listToVector(temp(), $4)); }
;
list:
  expr
  { $$ = list(temp(), $1); }
| list gap ',' gap expr
  { $$ = cons(temp(), $5, $1); }
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
  gap
  { $$ = list(temp(), oNull); }
| gap lines gap
  { $$ = nreverse($2); }
;
lines:
  line
  { $$ = list(temp(), $1); }
| lines '\n' line
  { $$ = cons(temp(), $3, $1); }
;

%%

obj keywordMessage(obj target, pair args) {
  return message(temp(),
                 target,
                 appendSymbols(temp(), map(temp(), car, args)),
                 listToVector(temp(), map(temp(), cdr, args)));
}

void yyerror (YYLTYPE *location,
              int *line,
              int *nesting,
              void **parserOutput,
              void *scanner,
              char const *msg) {
  if (location->first_line == (*line = location->last_line))
    printf("\nSyntax error at line %d.", location->last_line);
  else
    printf("\nSyntax error between lines %d and %d.", location->first_line, location->last_line);
}
