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

#include "cgreen/cgreen.h"

#include "core.c"

#include "mutex.h"

#include <string.h>

#define test(name, ...) \
  void test_##name() { \
    __VA_ARGS__ \
  } \
  add_test(suite, test_##name);


int main(int argc, char **argv) {
  TestSuite *suite = create_test_suite();
  setup(suite, setupInterpreter);

test(symbol,
  assert_false(strcmp(stringData(symbol(temp(), "foo")), "foo"));
)
test(integer,
  assert_equal(integerValue(integer(temp(), 42)), 42);
)
test(intern,
  assert_equal(symbol(temp(), "foo"), symbol(temp(), "foo"));
)
test(appendSymbols,
  assert_equal(appendSymbols(temp(),
                             cons(temp(), symbol(temp(), "foo"), list(temp(), symbol(temp(), "bar")))),
               symbol(temp(), "foobar"));
)
test(appendStrings,
  obj s1 = string(temp(), "foo"),
      s2 = string(temp(), "bar");
  assert_string_equal(stringData(appendStrings(temp(), string(temp(), "foo"), string(temp(), "bar"))),
                      "foobar");
)
test(freeSpaceCount,
  int oldCount = freeSpaceCount();
  newVector(edenRoot(garbageCollectorRoot), 0);
  // Free space should now be reduced by the size of a zero-length vector.
  assert_equal(freeSpaceCount(), oldCount - 3);
)
test(listToVector,
  vector n = integer(temp(), 42),
         m = symbol(temp(), "foo"),
         v = listToVector(temp(), cons(temp(), n, list(temp(), m)));
  assert_equal(vectorLength(v), 2);
  assert_equal(idx(v, 0), n);
  assert_equal(idx(v, 1), m);
)
test(spawn,
  void *p = newPromise(temp());
  vector i = integer(temp(), 42);
  void f(void *x) { keep(garbageCollectorRoot, p, x); }
  spawn(&makeVector(temp(), STACKDEPTH)->data[STACKDEPTH - 1], f, i);
  assert_equal(waitFor(p), i);
)
test(duplicateVector,
  obj s = integer(temp(), 42);
  vector v = duplicateVector(temp(), newVector(temp(), 1, s));
  assert_equal(vectorLength(v), 1);
  assert_equal(idx(v, 0), s);
)
test(prefix,
  char *s1 = "42", *s2 = "fourty-two";

  vector v = prefix(temp(), s1, newVector(temp(), 1, s2));
  assert_equal(vectorLength(v), 2);
  assert_equal(s1, idx(v, 0));
  assert_equal(s2, idx(v, 1));
)
test(shallowLookup,
  obj s = symbol(temp(), "slot"),
      v = symbol(temp(), "value"),
      o = newObject(temp(), oNull, newVector(temp(), 1, s), newVector(temp(), 1, v), emptyVector);
  assert_false(shallowLookup(o, symbol(temp(), "notASlot")));
  assert_equal(*shallowLookup(o, s), v);
)
test(deepLookup,
  obj s = symbol(temp(), "slot"),
      v = symbol(temp(), "value"),
      o = slotlessObject(temp(),
                         newObject(temp(),
                                   oNull,
                                   newVector(temp(), 1, s),
                                   newVector(temp(), 1, v),
                                   emptyVector),
                         emptyVector);
  assert_false(deepLookup(o, symbol(temp(), "notASlot")));
  assert_equal(*deepLookup(o, s), v); 
)
test(insertBetween,
  void *prev[3], *next[3], *middle[3], *single[3];
  vector p = (vector)prev, n = (vector)next, m = (vector)middle, s = (vector)single;
  p->next = n;
  n->prev = p;
  assert_equal(m, insertBetween(m, p, n));
  assert_equal(m, p->next);
  assert_equal(m, n->prev);
  assert_equal(m->prev, p);
  assert_equal(m->next, n);
  s->prev = s->next = s;
  assert_equal(m, insertBetween(m, s->prev, s));
  assert_equal(m->next, s);
  assert_equal(m->prev, s);
  assert_equal(s->next, m);
  assert_equal(s->prev, m);
)
test(compareAndExchange,
  int x = 42;
  assert_equal(compareAndExchange(&x, 4, 2), 42);
  assert_equal(x, 42);
  assert_equal(compareAndExchange(&x, 42, 420), 42);
  assert_equal(x, 420);
)
test(exchangeAndAdd,
  int x = 40;
  assert_equal(exchangeAndAdd(&x, 2), 40);
  assert_equal(x, 42);
)
test(increment,
  int x = 41;
  increment(&x);
  assert_equal(x, 42);
)
test(decrement,
  int x = 43;
  decrement(&x);
  assert_equal(x, 42);
)
test(collectGarbage,
  invalidateTemporaryLife();
  collectGarbage();
  // One of the placeholder vectors (either black or gray, whichever role is not being played by
  // "emptyVector") will have survived the first collection due to being created with its mark bit
  // set, although it's not really live. We must collect again to get an accurate count.
  collectGarbage();
  int n = freeSpaceCount();
  collectGarbage();
  assert_equal(freeSpaceCount(), n);
  vector *s = edenRoot(garbageCollectorRoot);
  // Testing only one vector size has failed to reveal intermittent memory leaks in the past.
  for (int i = 0; i < 1000; i++) {
    makeVector(s, 0);
    makeVector(s, i); // Orphaning the first vector.
    collectGarbage();
    assert_equal(n - freeSpaceCount(), 3 + i);
  }
  *s = 0;
  collectGarbage();
  assert_equal(freeSpaceCount(), n);
)
test(addSlot,
  obj o = newObject(temp(), oNull, emptyVector, emptyVector, 0),
      s = symbol(temp(), "foo"),
      i = integer(temp(), 42);
  void **v;
  assert_equal(addSlot(temp(), o, s, i), i);
  assert_true((int)(v = shallowLookup(o, s)));
  assert_equal(*v, i);
)
test(mark,
  obj i = integer(temp(), 42);
  mark(i);
  assert_true(isMarked(i));
  assert_equal(i->next, blackList);
)
test(scan,
  obj i = integer(temp(), 42);
  vector v = newVector(temp(), 1, i);
  mark(v);
  grayList = v;
  scan();
  assert_equal(v->next, i);
  assert_equal(i->prev, v);
  assert_equal(i->next, blackList);
)

  return run_test_suite(suite, create_text_reporter());
}
