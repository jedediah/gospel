#    Copyright 2008 Sam Chapin
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

gospel : threadData.c death.c gc.c mutex.c core.c objects.c y.tab.c lex.yy.c main.c
	gcc -ggdb -O3 -std=gnu99 lex.yy.c y.tab.c death.c threadData.c gc.c mutex.c core.c main.c -o gospel

lex.yy.o y.tab.o gc.o mutex.o death.o threadData.o test.o : test.c gc.c core.c objects.c y.tab.c lex.yy.c mutex.c death.c threadData.c
	gcc -c -O3 -std=gnu99 lex.yy.c y.tab.c gc.c mutex.c death.c threadData.c test.c
test : lex.yy.o y.tab.o gc.o test.o death.o mutex.o threadData.o cgreen/cgreen.a
	gcc lex.yy.o y.tab.o gc.o test.o death.o mutex.o threadData.o cgreen/cgreen.a -o test

main.c : core.h ;

objgen : ;
objects : ;

threadData.h : threadData.c gc.h ;
threadData.c : mutex.h gc.h ;
gc.h : gc.c ;
gc.c : mutex.h death.h ;
death.h: death.c ;
death.c: ;
mutex.h: mutex.c ;
mutex.c: death.h ;
core.h : core.c ;
core.c : objects.c death.h gc.h objects.h threadData.h ;
parser.y : core.h objects.h ;
parser.l : core.h y.tab.h ;

cgreen/cgreen.h : ;
cgreen/cgreen.a : ;
test.c : core.c mutex.h ;

objects.c objects.h : objgen objects
	./objgen objects

y.tab.c y.tab.h : parser.y
	bison -d -y parser.y

lex.yy.c : parser.l
	flex parser.l

.PHONY : clean
clean :
	rm -f objects.c objects.h y.tab.h y.tab.c lex.yy.c lex.yy.o y.tab.o gc.o core.o death.o mutex.o threadData.o test.o gospel test
