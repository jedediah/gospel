CC=gcc
CFLAGS=-g
OBJECTS=unit.o messaging.o breadcrumb.o reporter.o assertions.o vector.o mocks.o constraint.o parameters.o text_reporter.o

cgreen.a: $(OBJECTS)
	ar -rs cgreen.a $(OBJECTS)

clean:
	rm -f $(OBJECTS)
	rm -f cgreen.a
