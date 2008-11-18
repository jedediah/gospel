%option noyywrap

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
#include "y.tab.h" // This needs to be read after mytalk.h or else the default for YYSTYPE gets used.

void *openInputFile(FILE *f) {
  YY_BUFFER_STATE b = yy_create_buffer(f, YY_BUF_SIZE);
  yy_switch_to_buffer(b);
  return b;
}
void closeInputFile(void *b) {
  yy_delete_buffer(b);
}

%}

%x NEWLINES

%%

<INITIAL>{
\"([^"]|\\\")*\"                   { yytext[strlen(yytext) - 1] = 0;
                                     yylval = string(temp(), yytext + 1);     return STRING; }
\$[^ \n.,;]+                         yylval = symbol(temp(), yytext + 1);     return SYMBOL;
[0-9]+                               yylval = integer(temp(), atoi(yytext));  return INTEGER;
=                                                                             return ADDSLOT;
:=                                                                            return SETSLOT;
[a-zA-Z_0-9]+                        yylval = symbol(temp(), yytext);         return NAME;
[a-zA-Z_0-9]+:                       yylval = symbol(temp(), yytext);         return KEYWORD;
:[a-zA-Z_0-9]+:                      yylval = symbol(temp(), yytext + 1);     return OPERATOR;
([a-zA-Z_0-9]+:){2,}                 yylval = symbol(temp(), yytext);         return KEYWORDS;
->                                                                            return ARROW;
[^ \n.,;#^@a-zA-Z0-9|{}()\[\]\\]+    yylval = symbol(temp(), yytext);         return OPERATOR;
(#[^\n]*)?\n                         BEGIN(NEWLINES);                         return '\n';
\                                    /* discard */
.                                                                             return *yytext;
}
<NEWLINES>{
[\n\ ]|#[^\n]*\n                     /* discard */
.                                    yyless(0); BEGIN(INITIAL);
}
