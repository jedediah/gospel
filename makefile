#    Copyright © 2008, 2009 Sam Chapin
#
#    This file is part of Gospel.
#
#    Gospel is free software: you can redistribute it and/or modify
#    it under the terms of version 3 of the GNU General Public License
#    as published by the Free Software Foundation.
#
#    Gospel is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Gospel.  If not, see <http://www.gnu.org/licenses/>.

CC=gcc
CFLAGS=-std=gnu99 -O3 -ggdb

gospel : threadData.o death.o gc.o y.tab.o lex.yy.o core.o main.o
	gcc -pthread $^ -o $@

portable : threadData.o death.o gc.o y.tab.o lex.yy.o core.o portability.o
	gcc -pthread $^ -o gospel

objgen : ;
objects : ;
objects.c objects.h : objgen objects
	./objgen objects

y.tab.c y.tab.h : parser.y
	bison -d -y $^

lex.yy.c : parser.l
	flex $^

threadData.h : threadData.o gc.h ;
gc.h : gc.o ;
death.h : death.o ;
core.h : core.o ;
#parser.h : lex.yy.o ; # Would just create a circular dependency and be dropped.

lex.yy.o : lex.yy.c death.h core.h y.tab.h
y.tab.o : y.tab.c core.h objects.h
threadData.o : threadData.c gc.h
gc.o : gc.c objects.h death.h
death.o : death.c
core.o : core.c objects.c objects.h death.h gc.h threadData.h parser.h
main.o : main.c core.h objects.h
portability.o : main.c core.h objects.h portability.c

test : threadData.o death.o gc.o y.tab.o lex.yy.o test.o cgreen/cgreen.a
	gcc -pthread $^ -o $@

cgreen/cgreen.a : ;
cgreen/cgreen.h : cgreen/cgreen.a ;
# Dependencies cause core.c to be compiled twice anyway, might as well depend on core.o as a convenient
# shorthand for all the things that core.c depends on.
test.o : test.c cgreen/cgreen.h core.o


.PHONY : clean
clean :
	rm -f objects.c objects.h y.tab.h y.tab.c lex.yy.c lex.yy.o y.tab.o gc.o core.o death.o threadData.o test.o main.o portability.o gospel test
