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

vector nreverse(vector);
vector listToVector(vector);
obj appendSymbols(vector);
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

%token ADDSLOT
%token SETSLOT

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

//%debug
%initial-action {
  //yydebug = 1;
  @$.first_line = @$.last_line = *lineNumber;
}

%%

input:
  gap ENDOFFILE
  { *parserOutput = oEndOfFile; YYACCEPT; }
| gap
  { *parserOutput = oNull; *lineNumber = @1.last_line; }
| gap line gap
  { *parserOutput = $2; *lineNumber = @3.last_line; }
| error
  { *parserOutput = eSyntaxError; *lineNumber = @1.last_line; }
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
  unaryTarget signature '{' body '}'
  { pair sig = $2;
    $$ = message($1,
                 sAddMethod_as_,
                 newVector(2,
                           car(sig),
                           blockLiteral(listToVector(cdr(sig)),
                                        listToVector($4)))); }
;
statement:
  cascade
| carets
  { $$ = message(oDefaultMessageTarget, sReturn_atDepth_, newVector(2, oNull, integer((atom)$1))); }
| carets cascade
  { $$ = message(oDefaultMessageTarget, sReturn_atDepth_, newVector(2, $2, integer((atom)$1))); }
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
| OPERATOR NAME
  { $$ = cons($1, list($2)); }
| keywordsignature
  { pair keywords = nreverse($1);
    $$ = cons(appendSymbols(map(car, keywords)), map(cdr, keywords)); }
;
keywordsignature:
  KEYWORD gap NAME
  { $$ = list(cons($1, $3)); }
| keywordsignature KEYWORD gap NAME
  { $$ = cons(cons($2, $4), $1); }
;

cascade:
  expr
| cascade ';' gap message
  { $$ = call(quote($4), sCascading_, newVector(1, quote($1))); }
;
message:
  unaryMessage
| binaryMessage
| keywordMessage
| declaration
| assignment
;

parens:
 '(' body ')'
  { $$ = expressionSequence(listToVector($2)); }
;

expr:
  parens
| literal
| unaryMessage
| binaryMessage
| keywordMessage
| assignment
| declaration
;

assignment:
  unaryTarget param ADDSLOT gap expr
  { $$ = message($1, sAddSlot_as_, newVector(2, $2, $5)); }
| unaryTarget param SETSLOT gap expr
  { $$ = message($1, sSetSlot_to_, newVector(2, $2, $5)); }
;

gap:
| gap '\n'
;

unaryMessage:
  unaryTarget NAME
  { $$ = message($1, $2, emptyVector); }
;
unaryTarget:
  { $$ = oDefaultMessageTarget; }
| parens
| literal
| unaryMessage
| declaration
;

untargettedBinaryMessage:
  OPERATOR gap binaryArgument
  { $$ = message(oDefaultMessageTarget, $1, newVector(1, $3)); }
;
binaryMessage:
  binaryTarget OPERATOR gap binaryArgument
  { $$ = message($1, $2, newVector(1, $4)); }
;
binaryTarget:
  unaryTarget
| binaryMessage
;
binaryArgument:
  parens
| literal
| unaryMessage
| untargettedBinaryMessage
| untargettedKeywordMessage
;

untargettedKeywordMessage:
  keywords
  { $$ = keywordMessage(oDefaultMessageTarget, nreverse($1)); }
;
keywordMessage:
  binaryTarget keywords
  { $$ = keywordMessage($1, nreverse($2)); }
;
keywords:
  completeKeywords
| cutOffKeywords
;
completeKeywords:
  KEYWORD gap keywordArgument
  { $$ = list(cons($1, $3)); }
| completeKeywords KEYWORD gap keywordArgument
  { $$ = cons(cons($2, $4), $1); }
;
cutOffKeywords:
  keywordsPrecedingCutoff KEYWORD gap untargettedKeywordMessage
  { $$ = cons(cons($2, $4), $1); }
;
keywordsPrecedingCutoff:
  { $$ = emptyList; }
| completeKeywords
;
keywordArgument:
  parens
| literal
| unaryMessage
| binaryMessage
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
  { $$ = blockLiteral(emptyVector, listToVector($2)); }
| '{' params '|' body '}'
  { $$ = blockLiteral(listToVector(nreverse($2)), listToVector($4)); }
;
list:
  cascade
  { $$ = list($1); }
| list gap ',' gap cascade
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

pair nreverse(pair l) {
  pair next, last = emptyList;
  while (l) {
    next = cdr(l);
    setcdr(l, last);
    last = l;
    l = next;
  }
  return last;
}

int length(pair l) {
  int i = 0;
  while (l) {
    l = cdr(l);
    i++;
  }
  return i;
}

vector listToVector(vector list) {
  vector v = makeVector(length(list));
  for (int i = 0; list; i++, list = cdr(list)) setIdx(v, i, car(list));
  return v;
}

pair map(void *fn, pair list) {
  return list ? cons(((void *(*)(void *))fn)(car(list)), map(fn, cdr(list))) : list;
}

obj appendSymbols(pair symbols) {
  obj s = string("");
  for (; symbols; symbols = cdr(symbols))
    s = appendStrings(s, car(symbols));
  setProto(s, oSymbol);
  return intern(s);
}

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
/*
  if (location->first_line == (*line = location->last_line))
    printf("\nSyntax error at line %d.\n", location->last_line);
  else
    printf("\nSyntax error between lines %d and %d.\n", location->first_line, location->last_line);
*/
}

