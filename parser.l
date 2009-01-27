%{

/*
    Copyright © 2008 Sam Chapin

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

#include <string.h> // Flex needs this, but on some systems fails to generate the inclusion.
#include <stdlib.h>

#include "death.h"
#include "core.h"
#include "y.tab.h" // This needs to be read after core.h or else the default for YYSTYPE gets used.

%}

%option noyywrap
%option reentrant

%x NEWLINES
%x ENDING

ID  [a-zA-Z_0-9]+
OP  [!%&*\-+=<>?/]+

%%

<ENDING><<EOF>>           return ENDOFFILE;
<INITIAL>{
<<EOF>>                   BEGIN(ENDING); return '\n';
\"([^"]|\\\")*\"          { char *s = yytext + 1;
                            int remainder = strlen(s);
                            location->last_line = location->first_line;
                            while (*s != '"') {
                              --remainder;
                              if (*s == '\n') {
                                location->last_line++;
                                s++;
                                continue;
                              }
                              if (*s++ != '\\') continue;
                              switch (*s) {
                                case 'n': s[-1] = '\n'; break;
                                case 't': s[-1] = '\t'; break;
                                default:  s[-1] = *s;
                              }
                              memmove(s, s + 1, --remainder);
                            }
                            *s = 0;
                            *value = string(yyget_text(yyscanner) + 1);
                            return STRING;
                          }
\$[^ \[\]\(\)\{\}\n.,;]*  { *value = symbol(yyget_text(yyscanner) + 1);
                            return SYMBOL;
                          }
-?[0-9]+                  { *value = integer(atoi(yyget_text(yyscanner)));
                            return INTEGER;
                          }
=                         { BEGIN(NEWLINES);
                            return ADDSLOT;
                          }
:=                        { BEGIN(NEWLINES);
                            return SETSLOT;
                          }
{ID}                      { *value = symbol(yyget_text(yyscanner));
                            return NAME;
                          }
{ID}:                     { *value = symbol(yyget_text(yyscanner));
                            BEGIN(NEWLINES);
                            return KEYWORD;
                          }
:{ID}:                    { *value = symbol(yyget_text(yyscanner) + 1);
                            BEGIN(NEWLINES);
                            return OPERATOR;
                          }
({ID}:){2,}               { *value = symbol(yyget_text(yyscanner));
                            BEGIN(NEWLINES);
                            return KEYWORDS;
                          }
\\({ID}|({ID}:)+|{OP})    { *value = symbol(yyget_text(yyscanner) + 1);
                            return ESCAPE;
                          }
->                        { BEGIN(NEWLINES);
                            return ARROW;
                          }
[\(\[\{]                  { ++*nesting;
                            return *yyget_text(yyscanner);
                          }
[\)\]\}]                  { --*nesting;
                            return *yyget_text(yyscanner);
                          }
{OP}                      { *value = symbol(yyget_text(yyscanner));
                            BEGIN(NEWLINES);
                            return OPERATOR;
                          }
(#[^\n]*)?\n              { BEGIN(NEWLINES);
                            location->last_line = ++location->first_line;
                            if (*nesting) return '\n';
                            return 0;
                          }
\                         /* discard */
.                         return *yyget_text(yyscanner);
}
<NEWLINES>{
<<EOF>>                   BEGIN(ENDING); return '\n';
\ *(#[^\n]*)?\n           location->last_line = ++location->first_line;
.                         yyless(0); BEGIN(INITIAL);
}

%%

// TODO: Find a better way than this to get rid of the compiler warning.
int yyparse(int *, int *, void **, yyscan_t);

typedef struct {
  FILE *file;
  int line;
  YY_BUFFER_STATE buffer;
  yyscan_t scanner;
} scannerData;

scannerData *newScanner() {
  scannerData *sd = malloc(sizeof(scannerData));
  sd->line = 0;
  if (yylex_init(&sd->scanner)) die("Unable to initialize scanner.");
  return sd;
}

scannerData *beginParsing(FILE *input) {
  scannerData *sd = newScanner();
  yy_switch_to_buffer(sd->buffer = yy_create_buffer(sd->file = input, YY_BUF_SIZE, sd->scanner),
                      sd->scanner);
  return sd;
}
void *parse(scannerData *sd) {
  void *result;
  int nesting = 0;
  yyparse(&sd->line, &nesting, &result, sd->scanner);
  return result;
}
void endParsing(scannerData *sd) {
  yy_delete_buffer(sd->buffer, sd->scanner);
  yylex_destroy(sd->scanner);
  free(sd);
}

void *parseString(char *string) {
  scannerData *sd = newScanner();
  sd->buffer = yy_scan_string(string, sd->scanner);
  void *result = parse(sd);
  endParsing(sd);
  return result;
}
