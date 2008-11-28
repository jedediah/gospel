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
#include "y.tab.h" // This needs to be read after core.h or else the default for YYSTYPE gets used.

// To insulate file-handling code in core.c from knowledge of Flex.
void setLexerBuffer(void *b) {
  yy_switch_to_buffer(b);
}
void *createLexerBuffer(FILE *f) {
  YY_BUFFER_STATE b = yy_create_buffer(f, YY_BUF_SIZE);
  setLexerBuffer(b);
  return b;
}
void deleteLexerBuffer(void *b) {
  yy_delete_buffer(b);
}

char *stringLiteral;

#define oneLine (location->first_line = location->last_line = currentLine)

%}

%x NEWLINES

%%

<INITIAL>{
\"([^"]|\\\")*\"                   { char *s = yytext + 1;
                                     int remainder = strlen(s);
                                     location->first_line = currentLine; 
                                     while (*s != '"') {
                                       --remainder;
                                       if (*s == '\n') {
                                         currentLine++;
                                         continue;
                                       }
                                       if (*s++ != '\\') continue;
                                       switch (*s) {
                                         case 'n': s[-1] = '\n'; break;
                                         case 't': s[-1] = '\t';
                                       }
                                       memmove(s, s + 1, --remainder);
                                     }
                                     *s = 0;
                                     *value = string(temp(), yytext + 1);
                                     location->last_line = currentLine;
                                     return STRING;
                                   }
\$[^ \[\]\(\)\{\}\n.,;]*           { *value = symbol(temp(), yytext + 1);
                                     oneLine;
                                     return SYMBOL;
                                   }
-?[0-9]+                           { *value = integer(temp(), atoi(yytext));
                                     location->first_line = location->last_line = currentLine;
                                     return INTEGER;
                                   }
=                                  oneLine; return ADDSLOT;
:=                                 oneLine; return SETSLOT;
[a-zA-Z_0-9]+                      *value = symbol(temp(), yytext); oneLine; return NAME;
[a-zA-Z_0-9]+:                     *value = symbol(temp(), yytext); oneLine; return KEYWORD;
:[a-zA-Z_0-9]+:                    *value = symbol(temp(), yytext + 1); oneLine; return OPERATOR;
([a-zA-Z_0-9]+:){2,}               *value = symbol(temp(), yytext); oneLine; return KEYWORDS;
->                                 oneLine; return ARROW;
[^ \n.,;#^@a-zA-Z0-9|{}()\[\]\\]+  *value = symbol(temp(), yytext); oneLine; return OPERATOR;
(#[^\n]*)?\n                       { BEGIN(NEWLINES);
                                     oneLine;
                                     currentLine++;
                                     return '\n';
                                   }
\                                  /* discard */
.                                  oneLine; return *yytext;
}
<NEWLINES>{
\n|#[^\n]*\n                       currentLine++;
\                                  /* discard */
.                                  yyless(0); BEGIN(INITIAL);
}
