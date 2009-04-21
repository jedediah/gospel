/*
    Copyright © 2008, 2009 Sam Chapin

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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

// Used for bignum arithmetic.
#include <gmp.h>

#include <regex.h>

// Used by socket objects:
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

// Used by fileStream objects. 
#include <sys/stat.h>
#include <fcntl.h>


#include "death.h"
#include "core.h"
#include "gc.h"
#include "objects.h"
#include "threadData.h"
#include "parser.h"

#define NIL 0

int empty(pair l) {
  return l == emptyList;
}

pair list(void *o) {
  return cons(o, emptyList);
}

typedef vector continuation;

continuation newContinuation(void *origin,
                             obj selector,
                             vector evaluated,
                             vector unevaluated,
                             vector scope,
                             vector env,
                             vector old) {
  return newVector(8, origin, selector, evaluated, unevaluated, scope, env, NULL, old);
}

void *oldContinuation(continuation c) {
  return idx(c, 7);
}

obj receiver(continuation c) {
  return idx(c, 6);
}
vector setThreadReceiver(vector thread, obj o) {
  setIdx(threadContinuation(thread), 6, o);
  return thread;
}

void   *origin(continuation c)      { return idx(c, 0); } // either a promise or another continuation
obj     selector(continuation c)    { return idx(c, 1); }
vector  evaluated(continuation c)   { return idx(c, 2); }
vector  unevaluated(continuation c) { return idx(c, 3); }
obj     env(continuation c)         { return idx(c, 4); }
obj     dynamicEnv(continuation c)  { return idx(c, 5); }

void setEvaluated(continuation c, vector v)   { setIdx(c, 2, v); }
void setUnevaluated(continuation c, vector v) { setIdx(c, 3, v); }
void setEnv(continuation c, obj o)            { setIdx(c, 4, o); }

int isVisible(continuation c, obj namespace) {
  vector namespaces = hiddenEntity(dynamicEnv(c));
  for (int i = 0; i < vectorLength(namespaces); ++i)
    if (idx(namespaces, i) == namespace) return -1;
  return 0;
}
obj currentNamespace(continuation c) {
  return idx(hiddenEntity(dynamicEnv(c)), 0);
}

obj continuationTarget(continuation c) {
  return waitFor(idx(evaluated(c), 0));
}

vector subexpressionContinuation(continuation parent,
                                 obj scope,
                                 obj env,
                                 obj target,
                                 obj selector,
                                 vector args,
                                 vector old) {
  return newContinuation(parent,
                         selector,
                         emptyVector, // initially no subexpressions evaluated
                         prefix(target, args),
                         scope,
                         env,
                         old);
}

#define prepareMessageReturn(expr, escape) do { \
  obj value = (expr); \
  continuation c = origin(threadContinuation(currentThread)); \
  if (isPromise(c)) { \
    fulfillPromise(c, value); \
    escape; \
  }   \
  setContinuation(newContinuation(origin(c), \
                                  selector(c), \
                                  suffix(value, evaluated(c)), \
                                  unevaluated(c), \
                                  env(c), \
                                  dynamicEnv(c), \
                                  oldContinuation(c))); \
} while (0)

#define gotoNext return 0
#define callPrimitiveMethod(m) do { if (primitiveCode((m))()) return; tailcall(doNext); } while (0)
#define messageReturn(mr_v) do { \
  prepareMessageReturn(mr_v, return -1); \
  gotoNext; \
} while (0)
#define staticMessageReturn(smr_v) do { \
  prepareMessageReturn(smr_v, return); \
  tailcall(doNext); \
} while (0)

vector exceptionContinuation(obj exception) {
}

void prepareToRaise(obj exception) {
  continuation c = threadContinuation(currentThread);
  setContinuation(subexpressionContinuation(origin(c),
                                            env(c),
                                            dynamicEnv(c),
                                            exception,
                                            sRaise,
                                            emptyVector,
                                            oldContinuation(c)));
}

#define raise(exception) do { \
  prepareToRaise(exception); \
  gotoNext; \
} while (0)

obj appendStrings(obj s1, obj s2) {
  int length1 = stringLength(s1);
  vector s = makeAtomVector(CELLS_REQUIRED_FOR_BYTES(length1 + stringLength(s2) + 1));
  strcpy((char *)vectorData(s), stringData(s1));
  strcpy((char *)vectorData(s) + length1, stringData(s2));
  return slotlessObject(oString, s);
}

obj blockLiteral(vector params, vector body) {
  return slotlessObject(oBlockLiteral, method(params, body));
}
int isBlockLiteral(obj bl) { return isMethod(hiddenEntity(bl)); }

int methodArity(obj m) {
  return vectorLength(methodParams(m)) - 1;
}

obj block(obj env, obj method) {
  return slotlessObject(oBlock, newVector(2, env, method));
}
obj blockEnv(obj b)    { return idx(hiddenEntity(b), 0); }
obj blockMethod(obj b) { return idx(hiddenEntity(b), 1); }
obj setBlockEnv(obj b, obj e) { return setIdx(hiddenEntity(b), 0, e); }

int isBlock(obj b) { // FIXME: This is not a perfect test.
  return vectorLength(hiddenEntity(b)) == 2 && isMethod(blockMethod(b));
}

vector vectorAppend(vector v1, vector v2) {
  int length1 = vectorLength(v1), length2 = vectorLength(v2);
  vector v3 = makeVector(length1 + length2);
  memcpy(vectorData(v3), vectorData(v1), length1 * sizeof(void *));
  memcpy((void *)vectorData(v3) + length1 * sizeof(void *), vectorData(v2), length2 * sizeof(void *));
  return v3;
}

void **shallowLookup(obj o, obj name, vector c) {
  for (int i = 0; i < slotCount(o); ++i)
    if (slotName(o, i) == name && isVisible(c, slotNamespace(o, i)))
      return slotValuePointer(o, i);
  return 0;
}
void **deepLookup(obj o, obj name, continuation c) {
  void **slot;
  if (slot = shallowLookup(o, name, c)) return slot;
  return o == oNull ? 0 : deepLookup(proto(o), name, c);
}

obj addCanonSlot(obj o, obj s, void *v) {
  return newSlot(o, s, v, oNamespaceCanon);
}
obj addSlot(obj o, obj s, void *v, continuation c) {
  void **slot = shallowLookup(o, s, c);
  return slot ? *slot = v : newSlot(o, s, v, currentNamespace(c));
}

void invokeDispatchMethod(void);

obj newEnvironment(obj oldEnv) {
  return typedObject(oldEnv, hiddenEntity(oldEnv));
}

vector methodContinuation(vector c, obj scope, vector args, obj method) {
  return subexpressionContinuation(origin(c),
                                   stackFrame(scope, methodParams(method), args, c),
                                   newEnvironment(dynamicEnv(c)),
                                   oMethodBody,
                                   sSubexpressions_,
                                   methodBody(method),
                                   oldContinuation(c));
}

void doNext(void);

void normalDispatchMethod() {
  vector c = threadContinuation(currentThread);
  obj r = receiver(c);
  void **slot = shallowLookup(r, selector(c), c);
  if (!slot) {
    setThreadReceiver(currentThread, proto(r));
    tailcall(invokeDispatchMethod);
  }
  obj contents = *slot;

  if (isPrimitive(contents)) callPrimitiveMethod(contents);
  if (isMethod(contents)) {
    vector params = methodParams(contents), args = evaluated(c);
    if (vectorLength(args) != vectorLength(params))
      prepareToRaise(eBadArity);
    else
      setContinuation(methodContinuation(c, continuationTarget(c), args, contents));
    tailcall(doNext);
  }
  staticMessageReturn(contents); // The slot contains a constant value, not code.
}

void invokeDispatchMethod() {
  continuation c = threadContinuation(currentThread);
  obj dm = dispatchMethod(receiver(c));

  // TODO: Find a nice solution to the bootstrapping problem of initializing all objects with a
  //       primitive method object containing normalDispatchMethod, and eliminate the first test.

  if (!dm) tailcall(normalDispatchMethod);
  if (isPrimitive(dm)) callPrimitiveMethod(dm);
  if (isMethod(dm)) {
    // Method is checked for correct arity when it's installed.
    setContinuation(methodContinuation(c, continuationTarget(c), evaluated(c), dm));
    tailcall(doNext);
  }
  staticMessageReturn(dm); // The dispatch method is actually just a constant value.
}

void dispatch() {
  continuation c = threadContinuation(currentThread);
  obj t = continuationTarget(c);
  if (isActor(t)) {
    vector evaled = evaluated(c);
    int n = vectorLength(evaled) - 1;
    vector args = makeVector(n);
    for (int i = 0; i < n; ++i) setIdx(args, i, idx(evaled, i + 1));
    staticMessageReturn(enqueueMessage(t, selector(c), args));
  }
  setThreadReceiver(currentThread, t);
  tailcall(invokeDispatchMethod);
}

// The heart of the interpreter, called at the end of each expression to evaluate the next one.
void doNext() {
  shelter(currentThread, 0); // Release temporary allocations from the last subexpression.
  continuation c = threadContinuation(currentThread);
  int evaluatedCount = vectorLength(evaluated(c)),
      unevaluatedCount = vectorLength(unevaluated(c));
  // Have we evaluated all the subexpressions?
  if (evaluatedCount >= unevaluatedCount) tailcall(dispatch);
  // If not, evaluate the next subexpression.
  vector subexpr = newVector(1, idx(unevaluated(c), evaluatedCount));
  setContinuation(newContinuation(c,
                                  sInterpret,
                                  subexpr,
                                  subexpr,
                                  env(c),
                                  dynamicEnv(c),
                                  oldContinuation(c)));
  tailcall(dispatch);
}

obj callWithState(obj scope, obj env, obj target, obj selector, vector args) {
  promise p = newPromise();
  setContinuation(subexpressionContinuation(p,
                                            scope,
                                            env,
                                            target,
                                            selector,
                                            args,
                                            newVector(2,
                                                      threadContinuation(currentThread),
                                                      shelteredValue(currentThread))));   
  doNext();
  vector oc = oldContinuation(threadContinuation(currentThread));
  shelter(currentThread, idx(oc, 1));
  setContinuation(idx(oc, 0));
  return p;
}
obj call(obj target, obj selector, obj args) {
  continuation c = threadContinuation(currentThread);
  return callWithState(env(c), dynamicEnv(c), target, selector, args);
}

vector cons(void *car, void *cdr) {
  return newVector(2, car, cdr);
}
void *car(vector pair) { return idx(pair, 0); }
void *cdr(vector pair) { return idx(pair, 1); }

vector setcar(vector pair, void *val) { return setIdx(pair, 0, val); }
vector setcdr(vector pair, void *val) { return setIdx(pair, 1, val); }

obj intern(obj symbol) {
  acquireSymbolTableLock();
  obj symbolTable = hiddenEntity(oInternals);
  char *string = stringData(symbol);
  for (pair st = symbolTable; st; st = cdr(st))
    if (!strcmp(string, stringData(car(st)))) {
      releaseSymbolTableLock();
      return car(st);
    }
  setHiddenData(oInternals, cons(symbol, symbolTable));
  releaseSymbolTableLock();
  return symbol;
}

obj quote(obj o) { return slotlessObject(oQuote, o); }
obj unquote(obj q) { return hiddenEntity(q); }
int isQuote(obj o) { return -1; } // FIXME: How do we test for this without adding a primitive type?

obj  codeTarget(obj c)   { return idx(hiddenEntity(c), 0); }
obj  codeSelector(obj c) { return idx(hiddenEntity(c), 1); }
pair codeArgs(obj c)     { return idx(hiddenEntity(c), 2); }
obj setCodeTarget(obj c, obj t) { return setIdx(hiddenEntity(c), 0, t); }
int isCode(obj c) { return vectorLength(hiddenEntity(c)) == 3; } // FIXME: Find a better test.

obj message(obj target, obj selector, vector args) {
  return slotlessObject(oCode, newVector(3, target, selector, args));
}
obj setMessageTarget(obj message, obj target) {
  setIdx(hiddenEntity(message), 0, target);
  return message;
}
obj expressionSequence(vector exprs) {
  return message(oMethodBody, sSubexpressions_, exprs);
}

obj threadTarget(vector td) { return continuationTarget(threadContinuation(td)); }

void *safeIdx(vector thread, vector v, int i) {
  return i < vectorLength(v) ? idx(v, i) : 0;
}
void *arg(vector thread, int i) {
  return safeIdx(thread, evaluated(threadContinuation(thread)), i + 1);
}
int arity() {
  return vectorLength(evaluated(threadContinuation(currentThread))) - 1;
}

obj integer(int v) {
  obj i = emptyBignum();
  mpz_init_set_si(bignumData(i), v);
  return i;
}
int integerValue(obj i) {
  return mpz_get_si(bignumData(i));
}

// This should never be called, as no primitive object should ever actually be sent a message.
void prototypePrimitiveHiddenValue() {
  die("The prototype primitive's code was executed.");
}

void *loadStream(FILE *, obj, obj);

#define invokeBlock(b) do { \
  continuation c = origin(threadContinuation(currentThread)); \
  setContinuation(subexpressionContinuation(c, \
                                            env(c), \
                                            dynamicEnv(c), \
                                            (b), \
                                            sAppliedTo_, \
                                            newVector(1, vectorObject(emptyVector)), \
                                            oldContinuation(c))); \
  gotoNext; \
} while(0)
#define checkWithoutWaiting(c_value, c_predicate) \
  ({ obj c_newValue = (c_value); \
     for (;;) { \
       if (c_newValue == oNull) { \
         obj e = slotlessObject(oBadTypeException, 0); \
         addCanonSlot(e, sSelector, selector(threadContinuation(currentThread))); \
         raise(e); \
       } \
       if ((c_predicate)(c_newValue)) break; \
       c_newValue = proto(c_newValue); \
     } \
     c_newValue; })
#define check(c_value, c_predicate) (checkWithoutWaiting(waitFor(c_value), (c_predicate)))
#define retarget(r_predicate) (target = checkWithoutWaiting(target, (r_predicate)))
#define safeIntegerValue(siv_i) (integerValue(check((siv_i), isInteger)))
#define safeStringValue(ssv_s) (stringData(check((ssv_s), isString)))
#define safeVector(sv_v) (vectorObjectVector(check((sv_v), isVectorObject)))
#define valueReturn(vr_v) messageReturn(vr_v)
#define normalReturn valueReturn(continuationTarget(threadContinuation(currentThread)))
#define arg(a_i) (arg(currentThread, (a_i)) ?: ({ raise(eMissingArgument); (void *)0; }))

#include "objects.c"

#undef arg
#undef normalReturn
#undef valueReturn
#undef safeVector
#undef safeStringValue
#undef safeIntegerValue
#undef retarget
#undef check

obj symbol(const char *s) {
  obj o = string(s);
  setProto(o, oSymbol);
  return intern(o);
}

void setupInterpreter() {
  initializeHeap();
  initializeMainThread();
  setCurrentThread(garbageCollectorRoot = createGarbageCollectorRoot(oObject));
  initializeObjects();
  initializePrototypeTags();
  intern(oSymbol);
}

void *loadStream(FILE *f, obj scope, obj env) {
  void *scanner = beginParsing(f);
  obj parserOutput, lastValue = oNull;
  while ((parserOutput = parse(scanner)) != oEndOfFile) {
    if (parserOutput == eSyntaxError) {
      lastValue = eSyntaxError;
      break;
    }
    lastValue = waitFor(callWithState(scope, env, parserOutput, sIdentity, emptyVector));
  }
  endParsing(scanner);
  fclose(f);
  return lastValue;
}

