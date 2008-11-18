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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "death.h"
#include "core.h"
#include "gc.h"
#include "objects.h"
#include "threadData.h"

#define NIL 0

int empty(pair l) {
  return l == emptyList;
}

pair list(vector *live, void *o) {
  return cons(live, o, emptyList);
}

pair nreverse(pair l) {
  pair next, last = emptyList;
  while (!empty(l)) {
    next = cdr(l);
    setcdr(l, last);
    last = l;
    l = next;
  }
  return last;
}

int length(pair l) {
  int i = 0;
  while (!empty(l)) {
    l = cdr(l);
    i++;
  }
  return i;
}

vector listToVector(vector *live, vector list) {
  vector living = newVector(live, 2, list, 0);
  vector v = setIdx(living, 1, makeVector(edenIdx(living, 1), length(list)));
  for (int i = 0; !empty(list); i++, list = cdr(list)) setIdx(v, i, car(list));
  return *live = v;
}

void *fold(void *op, pair args, void *identity) {
  return empty(args) ? identity
                     : ((void *(*)(void *, void *))op)(car(args), fold(op, cdr(args), identity));
}
pair map(life live, void *fn, pair list) {
  return empty(list) ? list
                     : cons(live, ((void *(*)(void *))fn)(car(list)), map(live, fn, cdr(list)));
}

vector suffix(vector *live, void *e, vector v) {
  int l = vectorLength(v);
  vector living = newVector(live, 3, e, v, 0),
         nv = makeVector(edenIdx(living, 2), l + 1);
  nv->data[l] = e;
  memcpy(nv->data, v->data, l * sizeof(void *));
  return nv;
}
vector prefix(vector *live, void *e, vector v) {
  int l = vectorLength(v);
  vector living = newVector(live, 3, e, v, 0),
         nv = makeVector(edenIdx(living, 2), l + 1);
  setIdx(nv, 0, e);
  memcpy(edenIdx(nv, 1), vectorData(v), l * sizeof(void *));
  return nv;
}

vector  class(obj o)        { return     idx(o, 0);     }
vector  instance(obj o)     { return     idx(o, 1);     }
vector  hiddenEntity(obj o) { return     idx(o, 2);     }
void   *hiddenAtom(obj o)   { return idx(idx(o, 2), 0); }

// Encapsulate the way slot names are stored within the class vector.
int slotCount(obj o) {
  return vectorLength(idx(o, 0)) - 1;
}
void *slotName(obj o, int i) {
  return idx(class(o), i + 1);
}
// Encapsulate the way slot values are stored within the instance vector.
void *setSlotByIndex(obj o, int i, obj v) {
  return setIdx(instance(o), i, v);
}

void setClass(obj o, vector c)      { setIdx(o, 0, c); }
void setInstance(obj o, vector i)   { setIdx(o, 1, i); }
void setHiddenData(obj o, vector h) { setIdx(o, 2, h); }

vector delegate(obj o) {
  return idx(class(o), 0);
}
void setDelegate(vector *live, obj o, obj d) {
  vector c = duplicateVector(live, class(o));
  setIdx(c, 0, d);
  setClass(o, c);
}

obj newObject(vector *live, vector proto, vector slotNames, vector slotValues, void *hidden) {
  vector eden = newVector(live, 4, proto, slotNames, slotValues, hidden);
  return newVector(live,
                   3,
                   prefix(edenIdx(eden, 0), proto, slotNames),
                   slotValues,
                   hidden);
}
obj slotlessObject(vector *live, vector proto, vector hidden) {
  return newObject(live, proto, emptyVector, emptyVector, hidden);
}

typedef vector continuation;

continuation newContinuation(vector *live,
                             void *origin,
                             obj selector,
                             vector evaluated,
                             vector unevaluated,
                             vector staticEnv,
                             vector dynamicEnv) {
  return newVector(live, 6, origin, selector, evaluated, unevaluated, staticEnv, dynamicEnv);
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

obj continuationTarget(continuation c) {
  return waitFor(idx(evaluated(c), 0));
}

vector setSubexpressionContinuation(vector thread,
                                    continuation parentContinuation,
                                    obj staticEnv,
                                    obj dynamicEnv,
                                    obj target,
                                    obj selector,
                                    vector args) {
  vector eden = newVector(edenRoot(thread), 2, *edenRoot(thread), 0);
  return setContinuation(thread,
                         newContinuation(edenRoot(thread),
                                         parentContinuation,
                                         selector,
                                         emptyVector, // initially no subexpressions evaluated
                                         prefix(edenIdx(eden, 1), target, args),
                                         staticEnv,
                                         dynamicEnv));
}

// This needs to be a macro because otherwise the number of arguments would preclude tailcall optimization.
#define messageReturn(thread, value) do { \
  vector mr_t = (thread), \
         mr_v = (value); \
  continuation mr_c = origin(threadContinuation(mr_t)); \
  /* This logic can't easily be moved from the macro into a function call,
     because the return from keep() is what kills the thread. */ \
  if (isPromise(mr_c)) tailcall(keep, mr_t, mr_c, mr_v); \
  tailcall(doNext, setContinuation(mr_t, newContinuation(edenRoot(mr_t), \
                                                         origin(mr_c), \
                                                         selector(mr_c), \
                                                         suffix(edenRoot(mr_t), \
                                                                mr_v, \
                                                                evaluated(mr_c)), \
                                                         unevaluated(mr_c), \
                                                         env(mr_c), \
                                                         dynamicEnv(mr_c)))); \
} while (0)

vector setExceptionContinuation(vector thread, obj exception) {
  continuation c = threadContinuation(thread);
  return setSubexpressionContinuation(thread,
                                      origin(c),
                                      env(c),
                                      dynamicEnv(c),
                                      env(c),
                                      sRaise_,
                                      newVector(edenRoot(thread), 1, exception));
}

#define raise(thread, exception) \
  tailcall(doNext, setExceptionContinuation((thread), (exception)))

obj string(vector *live, const char *s) {
  // TODO: Write a macro to express the subtract-one-and-divide technique of finding size in cells.
  int length = (strlen(s) + 1 + sizeof(int) - 1) / sizeof(int);
  strcpy((char *)makeAtomVector(live, length)->data, s);
  return slotlessObject(live, oString, *live);
}
char *stringData(obj s) {
  return (char *)(hiddenEntity(s)->data);
}
obj appendStrings(vector *live, obj s1, obj s2) {
  vector living = newVector(live, 3, s1, s2, 0);
  int length1 = strlen(stringData(s1));
  vector s = makeAtomVector(edenIdx(living, 2), (length1 + strlen(stringData(s2)) + 1 + 3) / 4);
  strcpy((char *)vectorData(s), stringData(s1));
  strcpy((char *)vectorData(s) + length1, stringData(s2));
  return slotlessObject(live, oString, s);
}

// FIXME: This is not a good enough test, primitives and closures need to be specially tagged.
int isPrimitive(vector o) {
  return delegate(o) == oPrimitive;
}
int isClosure(obj o) {
  return delegate(o) == oClosure;
}

vector listToVector(life, vector);
obj block(vector *live, vector params, vector body) {
  return slotlessObject(live, oBlock, newVector(live, 2, params, body));
}
vector blockParams(obj b) { return idx(hiddenEntity(b), 0); }
vector blockBody(obj b)   { return idx(hiddenEntity(b), 1); }

obj newClosure(vector *live, obj env, vector params, vector body) {
  return slotlessObject(live, oClosure, newVector(live, 3, env, params, body));
}
vector closureEnv(obj c)    { return idx(hiddenEntity(c), 0); }
vector closureParams(obj c) { return idx(hiddenEntity(c), 1); }
vector closureBody(obj c)   { return idx(hiddenEntity(c), 2); }

void setSlotNames(vector *live, obj o, vector slotNames) {
  setClass(o, prefix(live, delegate(o), slotNames));
}
void setSlotValues(obj o, vector slotValues) {
  setInstance(o, slotValues);
}

vector vectorAppend(vector *live, vector v1, vector v2) {
  int length1 = vectorLength(v1), length2 = vectorLength(v2);
  vector living = newVector(live, 3, v1, v2),
         v3 = makeVector(edenIdx(living, 2), length1 + length2);
  memcpy(vectorData(v3), vectorData(v1), length1 * sizeof(void *));
  memcpy((void *)vectorData(v3) + length1, vectorData(v2), length2 * sizeof(void *));
  return v3;
}

void **shallowLookup(obj o, obj name) {
  vector c = class(o);
  for (int i = 1; i < vectorLength(c); i++)
    if (idx(c, i) == name) return &instance(o)->data[i - 1];
  return 0;
}
void **deepLookup(obj o, obj name) {
  void **slot;
  if (slot = shallowLookup(o, name)) return slot;
  return o == oNull ? 0 : deepLookup(delegate(o), name);
}

obj primitive(vector *live, void *code) {
  return slotlessObject(live, oPrimitive, newAtomVector(live, 1, code));
}

obj integer(vector *live, int value) {
  return slotlessObject(live, oInteger, newAtomVector(live, 1, value));
}
int integerValue(obj i) {
  return (int)hiddenAtom(i);
}

void doNext(vector);
vector newThread(vector *life,
                 void *cc,
                 obj staticEnv,
                 obj dynamicEnv,
                 obj target,
                 obj selector,
                 vector args) {
  vector living = newVector(life, 6, cc, staticEnv, dynamicEnv, target, selector, args),
         threadData = addThread(edenIdx(living, 1), garbageCollectorRoot),
         *newLife = edenIdx(living, 3);
  setContinuation(threadData,
                  newContinuation(newLife,
                                  cc,
                                  selector,
                                  emptyVector,
                                  prefix(newLife, target, args),
                                  staticEnv,
                                  dynamicEnv));
  spawn(topOfStack(threadData), doNext, threadData);
  return threadData;
}

promise originalInterpreterPromise;
void returnToREPL(vector thread) {
  keep(thread, originalInterpreterPromise, oNull);
  // The return from this function ends the current thread.
}
#define abort(thread, ...) do { \
  printf(__VA_ARGS__); \
  fflush(stdout); \
  tailcall(returnToREPL, (thread)); \
} while (0)

void dispatch(vector thread) {
  continuation c = threadContinuation(thread);
  obj t = continuationTarget(c);

  if (isChannel(t)) {
    vector e = makeVector(edenRoot(thread), 3);
    promise p = newPromise(edenIdx(e, 0));
    acquireChannelLock(t);
    vector behindChannel = duplicateVector(edenIdx(e, 1), evaluated(c));
    setIdx(behindChannel, 0, channelTarget(t));

    vector channelThread = addThread(edenIdx(e, 2), garbageCollectorRoot);
    setContinuation(channelThread,
                    newContinuation(edenIdx(e, 2),
                                    p,
                                    selector(c),
                                    behindChannel,
                                    behindChannel,
                                    env(c),
                                    dynamicEnv(c)));
    spawn(topOfStack(channelThread), dispatch, channelThread);

    obj r = waitFor(p);
    releaseChannelLock(t);
    messageReturn(thread, r);
  }

  void **slot = deepLookup(t, selector(c));
  if (!slot) {
    printf("Message $%s (%d arguments) not understood by %x.\nSlots in target:\n",
           stringData(selector(c)),
           vectorLength(evaluated(c)) - 1,
           t);
    for (int i = 1; i < vectorLength(class(t)); i++)
      printf("(%d) $%s ", i, stringData(idx(class(t), i)));
    raise(thread, eMessageNotUnderstood);
  }

  // TODO: Consider having messages sent to slots containing promises return promises.
  //       Unary messages could return the promise as-is, but if there are arguments then
  //       a new promise would need to be constructed to represent the message send.
  obj contents = waitFor(*slot);

  if (isPrimitive(contents))
    tailcall((void (*)(vector))hiddenAtom(contents), thread);
  if (isClosure(contents)) {
    vector params = closureParams(contents), args = evaluated(c);
    if (vectorLength(args) - 1 != vectorLength(params))
      raise(thread, string(edenRoot(thread), "Wrong number of arguments to closure."));

    // Discard the expression wherein the arguments were evaluated. Replace it with an expression
    // wherein the body of the closure is evaluated. The value of this expression is the value we
    // wanted for the original one.

    vector eden = makeVector(edenRoot(thread), 2);
    tailcall(doNext,
             setSubexpressionContinuation(thread,
                                          origin(c),
                                          newObject(edenIdx(eden, 0),
                                                    env(c),
                                                    prefix(edenIdx(eden, 0), sSelf, params),
                                                    args,
                                                    c),
                                          slotlessObject(edenIdx(eden, 1), dynamicEnv(c), NIL),
                                          oInterpreter,
                                          sMethodBody,
                                          closureBody(contents)));
  }
  messageReturn(thread, contents); // The slot just contains a value, not code.
}

// The heart of the interpreter, called at the end of each expression to evaluate the next one.
void doNext(vector thread) {
  setShelter(thread, 0); // Release temporary allocations from the last subexpression.
  continuation c = threadContinuation(thread);
  int evaluatedCount = vectorLength(evaluated(c)),
      unevaluatedCount = vectorLength(unevaluated(c));
  // Have we evaluated all the subexpressions?
  if (evaluatedCount == unevaluatedCount) tailcall(dispatch, thread);
  // If not, evaluate the next subexpression.
  vector *live = edenRoot(thread),
         subexpr = newVector(live, 1, idx(unevaluated(c), evaluatedCount));
  tailcall(dispatch,
           setContinuation(thread,
                           newContinuation(live,
                                           c,
                                           sInterpret,
                                           subexpr,
                                           subexpr,
                                           env(c),
                                           dynamicEnv(c))));
}

obj call(vector *live, obj dynamicEnv, obj target, obj selector, vector args) {
  promise p = newPromise(live);
  newThread(live, p, oStackFrame, dynamicEnv, target, selector, args);
  return waitFor(p);
}

vector cons(vector *live, void *car, void *cdr) {
  return newVector(live, 2, car, cdr);
}
void *car(vector pair) { return idx(pair, 0); }
void *cdr(vector pair) { return idx(pair, 1); }

vector setcar(vector pair, void *val) { setIdx(pair, 0, val); }
vector setcdr(vector pair, void *val) { setIdx(pair, 1, val); }

int isEmpty(vector v) {
  return vectorLength(v) == 0; 
}

void dispatch();

// TODO: Not threadsafe, currently must not be called outside of parser.
obj intern(vector *live, obj symbol) {
  obj symbolTable = *symbolTableShelter(garbageCollectorRoot);
  char *string = stringData(symbol);
  for (pair st = symbolTable; !empty(st); st = cdr(st))
    if (!strcmp(string, stringData(car(st))))
      return car(st);

  *symbolTableShelter(garbageCollectorRoot) = cons(live, symbol, symbolTable);

  return symbol;
}

obj  codeTarget(obj c)   { return idx(hiddenEntity(c), 0); }
obj  codeSelector(obj c) { return idx(hiddenEntity(c), 1); }
pair codeArgs(obj c)     { return idx(hiddenEntity(c), 2); }

obj message(vector *live, obj target, obj selector, vector args) {
  return slotlessObject(live, oCode, newVector(live, 3, target, selector, args));
}
obj expressionSequence(vector *live, vector exprs) {
  return message(live, oInterpreter, sMethodBody, exprs);
}
obj promiseCode(vector *live, obj message) {
  return slotlessObject(live, oPromiseCode, message);
}
obj promiseCodeValue(obj p) {
  return hiddenEntity(p);
}

obj arrowCode(vector *live, obj promiser, obj follower) {
  return slotlessObject(live, oArrowCode, newVector(live, 2, promiser, follower));
}
obj arrowPromiser(obj arrow) { return idx(hiddenEntity(arrow), 0); }
obj arrowFollower(obj arrow) { return idx(hiddenEntity(arrow), 1); }

void doNext();
YYSTYPE yyparse();

obj addSlot(vector *live, obj o, obj s, obj v) {
  vector living = newVector(live, 3, o, s, v);
  void **slot = shallowLookup(o, s);
  if (slot) return *slot = v;
  setClass(o, suffix(edenIdx(living, 1), s, class(o)));
  setInstance(o, suffix(edenIdx(living, 2), v, instance(o)));
  return v;
}

// Intended for use by the parser. Obviously not thread-safe.
vector *temp() {
  vector *s = edenRoot(garbageCollectorRoot);
  vector v = newVector(s, 2, 0, *s);
  return idxPointer(v, 0);
}
void invalidateTemporaryLife() {
  setShelter(garbageCollectorRoot, 0);
}

// Used by socket objects.
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

obj threadTarget(vector td) { return continuationTarget(threadContinuation(td)); }

void *safeIdx(vector thread, vector v, int i) {
  if (i >= vectorLength(v)) raise(thread, eOutOfBounds);
  return idx(v, i);
}

void *arg(vector thread, int i) {
  return safeIdx(thread, evaluated(threadContinuation(thread)), i + 1);
}

#define normalReturn     messageReturn(thread, continuationTarget(threadContinuation(thread)))
#define valueReturn(v)   messageReturn(thread, (v))
#define arg(i)           arg(thread, (i))
#define target           continuationTarget(threadContinuation(thread))
#include "objects.c"
#undef target
#undef arg
#undef valueReturn
#undef normalReturn

void (*primitiveCode(obj p))(continuation);

obj appendSymbols(vector *live, pair symbols) {
  vector living = newVector(live, 2, symbols, 0);
  obj s = symbol(edenIdx(living, 1), "");
  for (; !empty(symbols); symbols = cdr(symbols))
    s = appendStrings(edenIdx(living, 1), s, car(symbols));
  return intern(live, s);
}

obj symbol(vector *live, const char *s) {
  vector eden = makeVector(live, 2);
  obj o = string(edenIdx(eden, 0), s);
  setDelegate(edenIdx(eden, 1), o, oSymbol);
  return intern(live, o);
}

#include "y.tab.h"
int interactiveLexer() {
  yylex = mainLexer;
  return INTERACTIVE;
}
int batchLexer() {
  yylex = mainLexer;
  return BATCH;
}

void setupInterpreter() {
  initializeHeap();
  vector dummyEden;
  garbageCollectorRoot = createGarbageCollectorRoot(&dummyEden);
  initializeObjects();
  *lobbyShelter(garbageCollectorRoot) = oLobby;
}

void loadFile(const char *filename) {
  // TODO: Find a way to feed the parser from the file exactly as is done from stdin in the interactive
  //       version, rather than having to keep all the temporary allocations made by the parser for the
  //       entire file alive until the very end.
  invalidateTemporaryLife();
  yylex = batchLexer;
  FILE *f = fopen(filename, "r");
  if (!f) die("Could not open \"%s\" for input.", filename);
  void *b = openInputFile(f);
  yyparse();
  closeInputFile(b);
  for (pair code = parserOutput; !empty(code); code = cdr(code)) {
    originalInterpreterPromise = newPromise(temp());
    newThread(temp(),
              originalInterpreterPromise,
              oStackFrame,
              oDynamicEnvironment,
              car(code),
              sIdentity,
              emptyVector);
    waitFor(originalInterpreterPromise);
  }
}

void REPL() {
  openInputFile(stdin);
  for (;;) {
    invalidateTemporaryLife();
    fputs("\n> ", stdout);
    yylex = interactiveLexer;
    yyparse();
    originalInterpreterPromise = newPromise(temp());
    newThread(temp(),
              originalInterpreterPromise,
              oStackFrame,
              oDynamicEnvironment,
              parserOutput,
              sIdentity,
              emptyVector);
    fflush(stdout);
    fputs("\n=> ", stdout);
    vector dummyResult = newPromise(temp());
    newThread(temp(),
              dummyResult,
              oStackFrame,
              oDynamicEnvironment,
              waitFor(originalInterpreterPromise),
              sInspect,
              emptyVector);
    waitFor(dummyResult);
  }
}
