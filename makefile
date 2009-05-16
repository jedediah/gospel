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

CC?=gcc
CFLAGS?=-O3
CFLAGS+=-std=gnu99

ifndef GCC_THREADING
  CFLAGS+=-D NO_THREAD_VARIABLES
endif

gospel : death.o gc.o y.tab.o lex.yy.o core.o main.o
	$(CC) $(CFLAGS) -lgmp -pthread $^ -o $@

objgen : ;
objects : ;
objects.c objects.h : objgen objects
	./objgen objects

y.tab.c y.tab.h : parser.y core.h objects.h
	bison -d -y parser.y

lex.yy.c : parser.l death.h core.h y.tab.h
	flex parser.l

gc.h : gc.o ;
death.h : death.o ;
core.h : core.o ;
#parser.h : lex.yy.o ; # Would just create a circular dependency and be dropped.

lex.yy.o : lex.yy.c
y.tab.o : y.tab.c
gc.o : gc.c objects.h death.h
death.o : death.c
core.o : core.c objects.c objects.h death.h gc.h parser.h
main.o : main.c core.h objects.h

test : death.o gc.o y.tab.o lex.yy.o test.o cgreen/cgreen.a
	$(CC) $(CFLAGS) -lgmp -pthread $^ -o $@

cgreen/cgreen.a :
	cd ./cgreen && make

cgreen/cgreen.h : cgreen/cgreen.a ;
# Dependencies cause core.c to be compiled twice anyway, might as well depend on core.o as a convenient
# shorthand for all the things that core.c depends on.
test.o : test.c cgreen/cgreen.h core.o

.PHONY : clean
clean :
	rm -f objects.c objects.h y.tab.h y.tab.c lex.yy.c lex.yy.o y.tab.o gc.o core.o death.o test.o main.o gospel test ;
	cd ./cgreen && make clean
