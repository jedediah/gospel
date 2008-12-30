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
  assert_false(strcmp(stringData(symbol("foo")), "foo"));
)
test(integer,
  assert_equal(integerValue(integer(42)), 42);
)
test(intern,
  assert_equal(symbol("foo"), symbol("foo"));
)
test(appendSymbols,
  assert_equal(appendSymbols(cons(symbol("foo"), list(symbol("bar")))),
               symbol("foobar"));
)
test(appendStrings,
  obj s1 = string("foo"),
      s2 = string("bar"),
      s3 = string("");
  assert_string_equal(stringData(appendStrings(s1, s3)), "foo");
  assert_string_equal(stringData(appendStrings(s3, s1)), "foo");
  assert_string_equal(stringData(appendStrings(s1, s2)), "foobar");
)
test(freeSpaceCount,
  int oldCount = freeSpaceCount();
  newVector(0);
  // Free space should now be reduced by the size of a zero-length vector and its edenspace.
  assert_equal(freeSpaceCount(), oldCount - 3 - EDEN_OVERHEAD);
)
test(listToVector,
  vector n = integer(42),
         m = symbol("foo"),
         v = listToVector(cons(n, list(m)));
  assert_equal(vectorLength(v), 2);
  assert_equal(idx(v, 0), n);
  assert_equal(idx(v, 1), m);
)
test(spawn,
  void *p = newPromise();
  vector i = integer(42);
  void f(void *x) { keep(garbageCollectorRoot, p, x); }
  spawn(f, i);
  assert_equal(waitFor(p), i);
)
test(duplicateVector,
  obj s = integer(42);
  vector v = duplicateVector(newVector(1, s));
  assert_equal(vectorLength(v), 1);
  assert_equal(idx(v, 0), s);
)
test(prefix,
  char *s1 = "42", *s2 = "fourty-two";

  vector v = prefix(s1, newVector(1, s2));
  assert_equal(vectorLength(v), 2);
  assert_equal(s1, idx(v, 0));
  assert_equal(s2, idx(v, 1));
)
test(shallowLookup,
  obj s = symbol("slot"),
      v = symbol("value"),
      o = newObject(oNull, newVector(1, s), newVector(1, v), emptyVector);
  continuation c = newContinuation(0, 0, 0, 0, 0, oDynamicEnvironment);
  assert_false(shallowLookup(o, symbol("notASlot"), c));
  assert_equal(*shallowLookup(o, s, c), v);
)
test(deepLookup,
  obj s = symbol("slot"),
      v = symbol("value"),
      o = slotlessObject(newObject(oNull,
                                   newVector(1, s),
                                   newVector(1, v),
                                   emptyVector),
                         emptyVector);
  continuation c = newContinuation(0, 0, 0, 0, 0, oDynamicEnvironment);
  assert_false(deepLookup(o, symbol("notASlot"), c));
  assert_equal(*deepLookup(o, s, c), v); 
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
test(collectGarbage,
  invalidateEden();
  collectGarbage();
  // One of the placeholder vectors (either black or gray, whichever role is not being played by
  // "emptyVector") will have survived the first collection due to being created with its mark bit
  // set, although it's not really live. We must collect again to get an accurate count.
  collectGarbage();
  int n = freeSpaceCount();
  collectGarbage();
  assert_equal(freeSpaceCount(), n);
  // Testing only one vector size has failed to reveal intermittent memory leaks in the past.
  for (int i = 0; i < 1000; i++) {
    makeVector(0);
    invalidateEden();
    makeVector(i);
    collectGarbage();
    assert_equal(n - freeSpaceCount(), 3 + i + EDEN_OVERHEAD);
  }
  invalidateEden();
  collectGarbage();
  assert_equal(freeSpaceCount(), n);
)

test(addSlot,
  obj o = newObject(oNull, emptyVector, emptyVector, 0),
      s = symbol("foo"),
      i = integer(42);
  continuation c = newContinuation(0, 0, 0, 0, 0, oDynamicEnvironment);
  void **v;
  assert_equal(addSlot(o, s, i, c), i);
  assert_true((int)(v = shallowLookup(o, s, c)));
  assert_equal(*v, i);
)
test(mark,
  obj i = integer(42);
  mark(i);
  assert_true(isMarked(i));
  assert_equal(i->next, blackList);
)
test(scan,
  obj i = integer(42);
  vector v = newVector(1, i);
  mark(v);
  grayList = v;
  scan();
  assert_equal(v->next, i);
  assert_equal(i->prev, v);
  assert_equal(i->next, blackList);
)
test(stringLength,
  obj s = string("");
  assert_equal(stringLength(s), 0);
  s = string("foo");
  assert_equal(stringLength(s), 3);
  s = string("foobar");
  assert_equal(stringLength(s), 6);
)
test(vectorAppend,
  obj a = integer(1),
      b = integer(2),
      x = newVector(0),
      y = newVector(1, a),
      z = newVector(2, a, b),
      t = vectorAppend(x, y),
      u = vectorAppend(y, x),
      v = vectorAppend(y, z);
  assert_equal(vectorLength(t), 1);
  assert_equal(idx(t, 0), a);
  assert_equal(vectorLength(u), 1);
  assert_equal(idx(u, 0), a);
  assert_equal(vectorLength(v), 3);
  assert_equal(idx(v, 0), a);
  assert_equal(idx(v, 1), a);
  assert_equal(idx(v, 2), b);
)

  return run_test_suite(suite, create_text_reporter());
}
