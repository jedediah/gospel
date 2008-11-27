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
vector slotNameVector(vector *eden, obj o) {
  vector c = class(o);
  int l = vectorLength(c) - 1;
  vector s = makeVector(edenIdx(newVector(eden, 2, c, 0), 1), l);
  memcpy(vectorData(s), idxPointer(c, 1), l * sizeof(vector));
  return s;
}

// Encapsulate the way slot values are stored within the instance vector.
void *setSlotByIndex(obj o, int i, obj v) {
  return setIdx(instance(o), i, v);
}

void setClass(obj o, vector c)      { setIdx(o, 0, c); }
void setInstance(obj o, vector i)   { setIdx(o, 1, i); }
void setHiddenData(obj o, vector h) { setIdx(o, 2, h); }

vector proto(obj o) {
  return idx(class(o), 0);
}
void setProto(vector *live, obj o, obj d) {
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

vector setMessageReturnContinuation(vector thread, continuation c, obj value) {
  return setContinuation(thread,
                         newContinuation(edenRoot(thread),
                                         origin(c),
                                         selector(c),
                                         suffix(edenRoot(thread), value, evaluated(c)),
                                         unevaluated(c),
                                         env(c),
                                         dynamicEnv(c)));
}

#define messageReturn(thread, value) do { \
  vector mr_t = (thread), mr_v = (value); \
  continuation mr_c = origin(threadContinuation(mr_t)); \
  /* This logic can't easily be moved from the macro into the function call,
     because the return from keep() is what kills the thread. */ \
  if (isPromise(mr_c)) tailcall(keep, mr_t, mr_c, mr_v); \
  tailcall(doNext, setMessageReturnContinuation(mr_t, mr_c, mr_v)); \
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
  return proto(o) == oPrimitive;
}
int isClosure(obj o) {
  return proto(o) == oClosure;
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
  setClass(o, prefix(live, proto(o), slotNames));
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
  return o == oNull ? 0 : deepLookup(proto(o), name);
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

obj vectorObject(vector *eden, vector v) {
  return slotlessObject(eden, oVector, v);
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

promise REPLPromise;
void returnToREPL(vector thread) {
  keep(thread, REPLPromise, oNull);
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
                                                    closureEnv(contents),
                                                    prefix(edenIdx(eden, 0), sSelf, params),
                                                    args,
                                                    c),
                                          slotlessObject(edenIdx(eden, 1), dynamicEnv(c), NIL),
                                          oInternals,
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
  newThread(live, p, oLobby, dynamicEnv, target, selector, args);
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
  return message(live, oInternals, sMethodBody, exprs);
}
obj promiseCode(vector *live, obj message) {
  return slotlessObject(live, oPromiseCode, message);
}
obj promiseCodeValue(obj p) {
  return hiddenEntity(p);
}

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
  return i < vectorLength(v) ? idx(v, i) : 0;
}
void *arg(vector thread, int i) {
  return safeIdx(thread, evaluated(threadContinuation(thread)), i + 1);
}

obj *filenameToInclude;
promise *promiseOfInclusion;

#define normalReturn     messageReturn(thread, continuationTarget(threadContinuation(thread)))
#define valueReturn(v)   messageReturn(thread, (v))
#define arg(i)           (arg(thread, (i)) ?: ({ raise(thread, eMissingArgument); (void *)0; }))
#define target           continuationTarget(threadContinuation(thread))
#include "objects.c"
#undef target
#undef arg
#undef valueReturn
#undef normalReturn

void (*primitiveCode(obj p))(continuation);

obj appendSymbols(vector *live, pair symbols) {
  vector e = newVector(live, 2, symbols, 0);
  obj s = string(edenIdx(e, 0), "");
  for (; !empty(symbols); symbols = cdr(symbols))
    s = appendStrings(edenIdx(e, 0), s, car(symbols));
  setProto(edenIdx(e, 1), s, oSymbol);
  return intern(live, s);
}

obj symbol(vector *live, const char *s) {
  vector eden = makeVector(live, 2);
  obj o = string(edenIdx(eden, 0), s);
  setProto(edenIdx(eden, 1), o, oSymbol);
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

void resetLineNumber() { currentLine = 1; }

void loadFile(const char *filename) {
  // TODO: Find a way to feed the parser from the file exactly as is done from stdin in the interactive
  //       version, rather than having to keep all the temporary allocations made by the parser for the
  //       entire file alive until the very end.
  invalidateTemporaryLife();
  yylex = batchLexer;
  FILE *f = fopen(filename, "r");
  if (!f) die("Could not open \"%s\" for input.", filename);
  void *b = createLexerBuffer(f);
  resetLineNumber();
  yyparse();
  deleteLexerBuffer(b);
  fclose(f);
  for (pair code = parserOutput; !empty(code); code = cdr(code)) {
    REPLPromise = newPromise(temp());
    newThread(temp(),
              REPLPromise,
              oLobby,
              oDynamicEnvironment,
              car(code),
              sIdentity,
              emptyVector);
    waitFor(REPLPromise);
  }
}

int debuggingInclude = 0;
void REPL() {
  void *lexerBuffer = createLexerBuffer(stdin);
  for (;;) {
    invalidateTemporaryLife();
    
    vector inclusionEden = makeVector(temp(), 2);
    filenameToInclude = edenIdx(inclusionEden, 0);
    promiseOfInclusion = edenIdx(inclusionEden, 1);
    fflush(stdout);
    fputs("\n> ", stdout);
    yylex = interactiveLexer;
    resetLineNumber();
    yyparse();
    REPLPromise = newPromise(temp());
    newThread(temp(), REPLPromise, oLobby, oDynamicEnvironment, parserOutput, sIdentity, emptyVector);
    obj result = waitFor(REPLPromise);

    if (*filenameToInclude) {
      loadFile(stringData(*filenameToInclude));
      fulfillPromise(*promiseOfInclusion, oNull);
      setLexerBuffer(lexerBuffer);
    }

    REPLPromise = newPromise(temp());
    newThread(temp(), REPLPromise, oLobby, oDynamicEnvironment, result, sSerialized, emptyVector);
    obj serialization = waitFor(REPLPromise);
    fflush(stdout);
    fputs("\n=> ", stdout);
    REPLPromise = newPromise(temp());
    newThread(temp(), REPLPromise, oLobby, oDynamicEnvironment, serialization, sPrint, emptyVector);
    waitFor(REPLPromise);
  }
}
