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

%}

%x NEWLINES

%%

<INITIAL>{
\"([^"]|\\\")*\"                   { char *s = yytext + 1;
                                     int remainder = strlen(s);
                                     yylloc.first_line = currentLine; 
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
                                     yylval = string(temp(), yytext + 1);
                                     return STRING;
                                   }
\$[^ \n.,;]+                       { yylval = symbol(temp(), yytext + 1);
                                     yylloc.first_line = currentLine;
                                     return SYMBOL;
                                   }
-?[0-9]+                           { yylval = integer(temp(), atoi(yytext));
                                     yylloc.first_line = currentLine;
                                     return INTEGER;
                                   }
=                                  { yylloc.first_line = currentLine;
                                     return ADDSLOT;
                                   }
:=                                 { yylloc.first_line = currentLine;
                                     return SETSLOT;
                                   }
[a-zA-Z_0-9]+                      { yylval = symbol(temp(), yytext);
                                     yylloc.first_line = currentLine;
                                     return NAME;
                                   }
[a-zA-Z_0-9]+:                     { yylval = symbol(temp(), yytext);
                                     yylloc.first_line = currentLine;
                                     return KEYWORD;
                                   }
:[a-zA-Z_0-9]+:                    { yylval = symbol(temp(), yytext + 1);
                                     yylloc.first_line = currentLine;
                                     return OPERATOR;
                                   }
([a-zA-Z_0-9]+:){2,}               { yylval = symbol(temp(), yytext);
                                     yylloc.first_line = currentLine;
                                     return KEYWORDS;
                                   }
->                                 { yylloc.first_line = currentLine;
                                     return ARROW;
                                   }
[^ \n.,;#^@a-zA-Z0-9|{}()\[\]\\]+  { yylval = symbol(temp(), yytext);
                                     yylloc.first_line = currentLine;
                                     return OPERATOR;
                                   }
(#[^\n]*)?\n                       { BEGIN(NEWLINES);
                                     currentLine++;
                                     yylloc.first_line = currentLine;
                                     return '\n';
                                   }
\                                    /* discard */
.                                  { yylloc.first_line = currentLine;
                                     return *yytext;
                                   }
}
<NEWLINES>{
\n|#[^\n]*\n                         currentLine++;
\                                    /* discard */
.                                    yyless(0); BEGIN(INITIAL);
}
