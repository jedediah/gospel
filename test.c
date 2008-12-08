# 1 "./objects.c"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "./objects.c"
void mPrimitiveTarget_withArgumentVector_(vector thread) {
  obj code = target, argsObject = waitFor(arg(1));
  if (!isPrimitive(code)) raise(thread, ePrimitiveExpected);
  if (!isVectorObject(argsObject)) raise(thread, eVectorExpected);
  continuation c = threadContinuation(thread);
  life e = edenRoot(thread);
  tailcall(primitiveCode(code),
           setContinuation(thread,
                           newContinuation(e,
                                           origin(c),
                                           oNull,
                                           prefix(e, waitFor(arg(0)), vectorObjectVector(argsObject)),
                                           NIL,
                                           env(c),
                                           dynamicEnv(c))));
}

void mObjectSetSlot_to_(vector thread) {
  void **slot = deepLookup(target, waitFor(arg(0)));
  if (!slot) raise(thread, eSettingNonexistantSlot);
  valueReturn(*slot = arg(1));
}
void mObjectProto(vector thread) {
  valueReturn(proto(target));
}
void mObjectAddSlot_as_(vector thread) {
  valueReturn(addSlot(edenRoot(thread), target, waitFor(arg(0)), arg(1)));
}
void mObjectSend_(vector thread) {
  continuation c = origin(threadContinuation(thread));
  tailcall(doNext,
           setSubexpressionContinuation(thread,
                                        c,
                                        env(c),
                                        dynamicEnv(c),
                                        target,
                                        waitFor(arg(0)),
                                        emptyVector));
}
void mObjectChannel(vector thread) {
  valueReturn(newChannel(edenRoot(thread), target));
}
void mObjectEquals(vector thread) {
  if (target == waitFor(arg(0))) valueReturn(oTrue);
  valueReturn(oFalse);
}
void mObjectSelectors(vector thread) {
  valueReturn(vectorObject(edenRoot(thread), slotNameVector(edenRoot(thread), target)));
}
void mObjectContentsOfSlot_(vector thread) {
  void **slot = deepLookup(target, waitFor(arg(0)));
  if (!slot) raise(thread, eGettingNonexistantSlot);
  valueReturn(*slot);
}
void mObjectIdentity(vector thread) {
  normalReturn;
}
void mObjectInclude_(vector thread) {
  obj a = waitFor(arg(0));
  if (!isString(a)) raise(thread, eStringExpected);
  valueReturn(loadFile(stringData(a)));
}
void mObjectNew(vector thread) {
  obj o = slotlessObject(edenRoot(thread), target, hiddenEntity(target));
  setVectorType(o, vectorType(target));
  valueReturn(o);
}
void mObjectSetProto_(vector thread) {
  obj p = arg(0);
  setProto(edenRoot(thread), target, p);
  valueReturn(p);
}
void mObjectRespondsTo_(vector thread) {
  obj a = waitFor(arg(0)), t = target;
  for (int i = 0; i < slotCount(t); i++) if (slotName(t, i) == a) valueReturn(oTrue);
  valueReturn(oFalse);
}



void mFileReadBytes_(vector thread) {
  vector *live = edenRoot(thread), living = makeVector(live, 3);
  int length = safeIntegerValue(waitFor(arg(0)));
  vector buffer = makeAtomVector(idxPointer(living, 0), (length + sizeof(int) - 1) / sizeof(int));
  int count = read(safeIntegerValue(target), buffer->data, length);
  vector result = makeAtomVector(idxPointer(living, 2), (count + sizeof(int) - 1) / sizeof(int));
  memcpy(result->data, buffer->data, count);
  valueReturn(slotlessObject(live, oByteVector, result));
}
void mFileClose(vector thread) {
  if (close(safeIntegerValue(target))) raise(thread, eClosingFile);
  normalReturn;
}


void mArrowCodeFrom_(vector thread) {
  vector eden = makeVector(edenRoot(thread), 2);
  promise p = newPromise(edenIdx(eden, 0));
  continuation c = threadContinuation(thread);
  newThread(edenIdx(eden, 1),
            p,
            env(c),
            dynamicEnv(c),
            oInternals,
            sMethodBody,
            newVector(edenIdx(eden, 1),
                      2,
                      message(edenIdx(eden, 1), arg(0), sIdentity, emptyVector),
                      hiddenEntity(target)));
  valueReturn(p);
}

void mClosureEnvironment(vector thread) {
  valueReturn(closureEnv(target));
}
void mClosureTarget_withArgumentVector_(vector thread) {
  obj code = target, argsObject = waitFor(arg(1));
  if (!isClosure(code)) raise(thread, eClosureExpected);
  if (!isVectorObject(argsObject)) raise(thread, eVectorExpected);
  vector args = vectorObjectVector(argsObject),
         params = closureParams(code),
         e = makeVector(edenRoot(thread), 2);
  if (vectorLength(args) + 1 != vectorLength(params)) raise(thread, eBadArity);
  continuation c = threadContinuation(thread);
  tailcall(doNext,
           setSubexpressionContinuation(thread,
                                        origin(c),
                                        stackFrame(edenIdx(e, 0),
                                                   closureEnv(code),
                                                   params,
                                                   prefix(edenIdx(e, 0), waitFor(arg(0)), args),
                                                   c),
                                        slotlessObject(edenIdx(e, 1), dynamicEnv(c), NIL),
                                        oInternals,
                                        sMethodBody,
                                        closureBody(code)));
}

void mStringAppending(vector thread) {
  obj a = waitFor(arg(0));
  if (!isString(target) || !isString(a)) raise(thread, eStringExpected);
  valueReturn(appendStrings(edenRoot(thread), target, waitFor(arg(0))));
}
void mStringSerialized(vector thread) {

  vector eden = makeVector(edenRoot(thread), 2);
  obj quote = string(edenIdx(eden, 0), "\"");
  valueReturn(appendStrings(edenIdx(eden, 1), quote, appendStrings(edenIdx(eden, 1), target, quote)));
}
void mStringPrint(vector thread) {
  if (!isString(target)) raise(thread, eStringExpected);
  fputs(stringData(target), stdout);
  fflush(stdout);
  normalReturn;
}

void mIntegerPlus(vector thread) {
  valueReturn(integer(edenRoot(thread),
                      safeIntegerValue(target) + safeIntegerValue(waitFor(arg(0)))));
}
void mIntegerIsLessThan(vector thread) {
  if (safeIntegerValue(target) < safeIntegerValue(waitFor(arg(0)))) normalReturn;
  valueReturn(oFalse);
}
void mIntegerIsGreaterThan(vector thread) {
  if (safeIntegerValue(target) > safeIntegerValue(waitFor(arg(0)))) normalReturn;
  valueReturn(oFalse);
}
void mIntegerSecondsDelay(vector thread) {
  int i = safeIntegerValue(target);
  while (i) i = sleep(i);
  valueReturn(target);
}
void mIntegerEquals(vector thread) {
  if (safeIntegerValue(target) == safeIntegerValue(waitFor(arg(0)))) normalReturn;
  valueReturn(oFalse);
}
void mIntegerMinus(vector thread) {
  valueReturn(integer(edenRoot(thread),
              safeIntegerValue(target) - safeIntegerValue(waitFor(arg(0)))));
}
void *bIntegerSerialized(vector) __attribute__ ((noinline));
void mIntegerSerialized(vector thread) { gotoNext(bIntegerSerialized(thread)); }


void mCodeInterpret(vector thread) {
  continuation c = threadContinuation(thread);
  obj o = continuationTarget(c);
  vector *live = edenRoot(thread);
  tailcall(doNext,
           setContinuation(thread,
                           newContinuation(live,
                                           origin(c),
                                           codeSelector(o),
                                           emptyVector,
                                           prefix(live, codeTarget(o) ?: env(c), codeArgs(o)),
                                           env(c),
                                           dynamicEnv(c))));
}

void mVectorAt_put_ifAbsent_(vector thread) {
  int i = safeIntegerValue(waitFor(arg(0)));
  obj v = hiddenEntity(target);
  if (i < 0) i = vectorLength(v) + i;
  if (i < 0 || i >= vectorLength(v)) {
    continuation c = origin(threadContinuation(thread));
    tailcall(doNext,
             setSubexpressionContinuation(thread, c, env(c), dynamicEnv(c), arg(2), sDo, emptyVector));
  }
  valueReturn(setIdx(v, i, arg(1)));
}
void mVectorLength(vector thread) {
  valueReturn(integer(edenRoot(thread), vectorLength(hiddenEntity(target))));
}
void mVectorAt_ifAbsent_(vector thread) {
  int i = safeIntegerValue(waitFor(arg(0)));
  obj v = hiddenEntity(target);
  if (i < 0) i = vectorLength(v) + i;
  if (i < 0 || i >= vectorLength(v)) {
    continuation c = origin(threadContinuation(thread));
    tailcall(doNext,
             setSubexpressionContinuation(thread, c, env(c), dynamicEnv(c), arg(1), sDo, emptyVector));
  }
  valueReturn(idx(v, i));
}
void mVectorOfLength_containing_(vector thread) {
  int l = safeIntegerValue(waitFor(arg(0)));
  if (l < 0) raise(thread, eNegativeVectorLength);
  obj o = arg(1);
  vector v = makeVector(edenRoot(thread), l);
  while (l--) setIdx(v, l, o);
  valueReturn(vectorObject(edenRoot(thread), v));
}

void mLobbyDynamicContext(vector thread) {
  valueReturn(dynamicEnv(threadContinuation(thread)));
}
void mLobbyAbort(vector thread) {
  tailcall(returnToREPL, thread);
}
void mLobbyReturn_atDepth_(vector thread) {
  obj frame = target;
  for (int i = safeIntegerValue(waitFor(arg(1)));;) {
    if (frame == oLobby) raise(thread, eBadReturnDepth);
    if (!i--) break;
    frame = proto(frame);
  }
  obj v = *edenRoot(thread) = arg(0);
  setContinuation(thread, safeStackFrameContinuation(frame));
  valueReturn(v);
}
void mLobbyExit(vector thread) {
  exit(0);
}
void mLobbyEnd(vector thread) {
  killThreadData(thread);

}
void mLobbyReturn(vector thread) {
  if (target == oLobby) raise(thread, eBadReturnDepth);
  setContinuation(thread, safeStackFrameContinuation(target));
  valueReturn(oNull);
}
void mLobbyAbortToREPL(vector thread) {
  tailcall(returnToREPL, thread);
}
void mLobbyRecurse(vector thread) {
  if (target == oLobby) raise(thread, eTopLevelRecursion);
  tailcall(doNext, setContinuation(thread, safeStackFrameContinuation(target)));
}
void mLobbyReturn_(vector thread) {
  if (target == oLobby) raise(thread, eBadReturnDepth);
  obj v = *edenRoot(thread) = arg(0);
  setContinuation(thread, safeStackFrameContinuation(target));
  valueReturn(v);
}
void mLobbyThisContext(vector thread) {
  normalReturn;
}
void mLobbyCollectGarbage(vector thread) {
  forbidGC();
  collectGarbage();
  permitGC();
  normalReturn;
}

void mSymbolSerialized(vector thread) {

  valueReturn(appendStrings(edenRoot(thread), string(edenRoot(thread), "$"), target));
}

void mPromiseCodeInterpret(vector thread) {
  vector *life = edenRoot(thread);
  promise p = newPromise(life);
  continuation c = threadContinuation(thread);
  newThread(life,
            p,
            env(c),
            dynamicEnv(c),
            hiddenEntity(continuationTarget(c)),
            sIdentity,
            emptyVector);
  valueReturn(p);
}

void mNullIdentity(vector thread) {
  normalReturn;
}
void mNullInterpret(vector thread) {
  normalReturn;
}
void mNullPrint(vector thread) {
  fputs("<null>", stdout);
  fflush(stdout);
  normalReturn;
}

void mSocketPosixFileDescriptor(vector thread) {
  valueReturn(integer(edenRoot(thread), (int)hiddenAtom(target)));
}
void mSocketNew(vector thread) {
  int s = socket(PF_INET, SOCK_STREAM, 0);
  if (s == -1) raise(thread, eOpeningSocket);
  valueReturn(setProto(edenRoot(thread), integer(edenRoot(thread), s), oSocket));
}
void mSocketRead_(vector thread) {
  int length = safeIntegerValue(waitFor(arg(0)));
  vector *live = edenRoot(thread);
  obj string = slotlessObject(live, oString, makeAtomVector(live, length));
  if (recv(safeIntegerValue(target), vectorData(hiddenEntity(string)), length, 0) == -1)
    raise(thread, eReadingFromSocket);
  valueReturn(string);
}
void mSocketListenWithBacklog_(vector thread) {
  if (listen((int)hiddenAtom(target), safeIntegerValue(waitFor(arg(0)))))
    raise(thread, eListeningOnSocket);
  normalReturn;
}
void *bSocketAccept(vector) __attribute__ ((noinline));
void mSocketAccept(vector thread) { gotoNext(bSocketAccept(thread)); }
void *bSocketBind_(vector) __attribute__ ((noinline));
void mSocketBind_(vector thread) { gotoNext(bSocketBind_(thread)); }


void mInternalsMethodBody(vector thread) {
  int l = vectorLength(evaluated(threadContinuation(thread)));
  valueReturn(l == 1 ? oNull : arg(l - 2));
}
void mInternalsVectorLiteral(vector thread) {
  int c = vectorLength(evaluated(threadContinuation(thread))) - 1;
  vector v = makeVector(edenRoot(thread), c);
  while (c--) setIdx(v, c, arg(c));
  valueReturn(vectorObject(edenRoot(thread), v));
}

void mByteVectorLength(vector thread) {

  valueReturn(integer(edenRoot(thread), vectorLength(hiddenEntity(target)) / sizeof(int)));
}

void mBlockInterpret(vector thread) {
  continuation c = threadContinuation(thread);
  obj o = continuationTarget(c);
  valueReturn(newClosure(edenRoot(thread), env(c), blockParams(o), blockBody(o)));
}
# 380 "./objects.c"
void *bIntegerSerialized(vector thread) {
  char buffer[12];
  sprintf(buffer, "%d", safeIntegerValue(target));
  valueReturn(string(edenRoot(thread), buffer));
}
# 393 "./objects.c"
void *bSocketAccept(vector thread) {
  struct sockaddr socketParameters;
  socklen_t socketParametersSize = sizeof(struct sockaddr);
  int newSocket = accept(safeIntegerValue(target), &socketParameters, &socketParametersSize);
  if (newSocket == -1) raise(thread, eAcceptingSocketConnection);
  life e = edenRoot(thread);
  valueReturn(setProto(e, integer(e, newSocket), oSocket));
}
void *bSocketBind_(vector thread) {
  struct sockaddr_in s;
  memset(&s, 0, sizeof(struct sockaddr_in));
  s.sin_family = AF_INET;
  s.sin_port = htons(safeIntegerValue(waitFor(arg(0))));
  s.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(safeIntegerValue(target), (struct sockaddr *)&s, (socklen_t)sizeof(struct sockaddr_in)))
    raise(thread, eBindingSocket);
  normalReturn;
}
# 420 "./objects.c"
void initializeObjects() {
  oPrimitive = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oObject = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oFalse = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oInterpreter = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oFile = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oTrue = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oArrowCode = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oClosure = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oString = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oInteger = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oDynamicEnvironment = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oCode = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oVector = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oLobby = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oSymbol = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oPromiseCode = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oNull = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oSocket = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oException = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oInternals = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oByteVector = newObject(temp(), NIL, emptyVector, emptyVector, NIL);
  oBlock = newObject(temp(), NIL, emptyVector, emptyVector, NIL);

  eStackFrameExpected = string(temp(), "Something other than a stack frame encountered where stack frame expected.");
  eReadingFromSocket = string(temp(), "Error while reading from a socket.");
  eBindingSocket = string(temp(), "Error while binding a socket.");
  eClosureExpected = string(temp(), "Non-closure encountered where closure expected.");
  eIntegerExpected = string(temp(), "Non-integer encountered where integer expected.");
  eMessageNotUnderstood = string(temp(), "Message not understood.");
  ePrimitiveExpected = string(temp(), "Non-primitive encountered where primitive expected.");
  eOutOfBounds = string(temp(), "Out-of-bounds array access.");
  eNegativeVectorLength = string(temp(), "Attempted to create a vector with negative length.");
  eAcceptingSocketConnection = string(temp(), "Error while accepting a connection on a socket.");
  eGettingNonexistantSlot = string(temp(), "Attempted to access a nonexistant slot.");
  eTopLevelRecursion = string(temp(), "Attempted to recurse at the top level.");
  eBadReturnDepth = string(temp(), "Too many carets in return statement.");
  eMissingArgument = string(temp(), "Too few arguments to primitive function.");
  eBadArity = string(temp(), "Wrong number of arguments to closure.");
  eOpeningSocket = string(temp(), "Error while opening a socket.");
  eSettingNonexistantSlot = string(temp(), "Attempted to set a nonexistant slot.");
  eVectorExpected = string(temp(), "Non-vector encountered where vector expected.");
  eListeningOnSocket = string(temp(), "Error while beginning to listen on a socket.");
  eClosingFile = string(temp(), "Error while closing a file.");
  eStringExpected = string(temp(), "Non-string encountered where string expected.");
  sOfLength_containing_ = symbol(temp(), "ofLength:containing:");
  sAddSlot_as_ = symbol(temp(), "addSlot:as:");
  sSend_ = symbol(temp(), "send:");
  sExit = symbol(temp(), "exit");
  sInterpret = symbol(temp(), "interpret");
  sEquals = symbol(temp(), "==");
  sPrint = symbol(temp(), "print");
  sSecondsDelay = symbol(temp(), "secondsDelay");
  sNew = symbol(temp(), "new");
  sSetProto_ = symbol(temp(), "setProto:");
  sProto = symbol(temp(), "proto");
  sSetSlot_to_ = symbol(temp(), "setSlot:to:");
  sAbort = symbol(temp(), "abort");
  sMethodBody = symbol(temp(), "methodBody");
  sRead_ = symbol(temp(), "read:");
  sEnd = symbol(temp(), "end");
  sSelectors = symbol(temp(), "selectors");
  sReadBytes_ = symbol(temp(), "readBytes:");
  sFrom_ = symbol(temp(), "from:");
  sInclude_ = symbol(temp(), "include:");
  sRecurse = symbol(temp(), "recurse");
  sVectorLiteral = symbol(temp(), "vectorLiteral");
  sReturn_ = symbol(temp(), "return:");
  sListenWithBacklog_ = symbol(temp(), "listenWithBacklog:");
  sIsGreaterThan = symbol(temp(), ">");
  sChannel = symbol(temp(), "channel");
  sAbortToREPL = symbol(temp(), "abortToREPL");
  sPosixFileDescriptor = symbol(temp(), "posixFileDescriptor");
  sClose = symbol(temp(), "close");
  sAt_ifAbsent_ = symbol(temp(), "at:ifAbsent:");
  sThisContext = symbol(temp(), "thisContext");
  sSerialized = symbol(temp(), "serialized");
  sDynamicContext = symbol(temp(), "dynamicContext");
  sIsLessThan = symbol(temp(), "<");
  sAt_put_ifAbsent_ = symbol(temp(), "at:put:ifAbsent:");
  sReturn_atDepth_ = symbol(temp(), "return:atDepth:");
  sMinus = symbol(temp(), "-");
  sReturn = symbol(temp(), "return");
  sEnvironment = symbol(temp(), "environment");
  sPlus = symbol(temp(), "+");
  sLength = symbol(temp(), "length");
  sTarget_withArgumentVector_ = symbol(temp(), "target:withArgumentVector:");
  sAppending = symbol(temp(), "++");
  sContentsOfSlot_ = symbol(temp(), "contentsOfSlot:");
  sIdentity = symbol(temp(), "identity");
  sCollectGarbage = symbol(temp(), "collectGarbage");
  sRespondsTo_ = symbol(temp(), "respondsTo:");
  sAccept = symbol(temp(), "accept");
  sBind_ = symbol(temp(), "bind:");
  sSerialized = symbol(temp(), "serialized");
  sDo = symbol(temp(), "do");
  sSelf = symbol(temp(), "self");
  sRaise_ = symbol(temp(), "raise:");
  sCurrentMessageTarget = symbol(temp(), "currentMessageTarget");
  sTrue = symbol(temp(), "true");
  sFalse = symbol(temp(), "false");
  sObject = symbol(temp(), "object");
  sClosure = symbol(temp(), "closure");
  sInteger = symbol(temp(), "integer");
  sNull = symbol(temp(), "null");
  sLobby = symbol(temp(), "lobby");
  sFile = symbol(temp(), "file");
  sSocket = symbol(temp(), "socket");
  sByteVector = symbol(temp(), "byteVector");
  sCode = symbol(temp(), "code");
  sPromiseCode = symbol(temp(), "promiseCode");
  sArrowCode = symbol(temp(), "arrowCode");
  sBlock = symbol(temp(), "block");
  sPrimitive = symbol(temp(), "primitive");
  sString = symbol(temp(), "string");
  sSymbol = symbol(temp(), "symbol");
  sInterpreter = symbol(temp(), "interpreter");
  sException = symbol(temp(), "exception");
  sVector = symbol(temp(), "vector");
  sSerialized = symbol(temp(), "serialized");

  setHiddenData(oPrimitive, newAtomVector(temp(), 1, prototypePrimitiveHiddenValue));
  setProto(temp(), oPrimitive, oObject);
  setSlotNames(temp(), oPrimitive, newVector(temp(), 1
  , sTarget_withArgumentVector_));
  setSlotValues(oPrimitive, newVector(temp(), 1
  , primitive(temp(), mPrimitiveTarget_withArgumentVector_)));

  setProto(temp(), oObject, oNull);
  setSlotNames(temp(), oObject, newVector(temp(), 13
  , sSetSlot_to_
  , sProto
  , sAddSlot_as_
  , sSend_
  , sChannel
  , sEquals
  , sSelectors
  , sContentsOfSlot_
  , sIdentity
  , sInclude_
  , sNew
  , sSetProto_
  , sRespondsTo_));
  setSlotValues(oObject, newVector(temp(), 13
  , primitive(temp(), mObjectSetSlot_to_)
  , primitive(temp(), mObjectProto)
  , primitive(temp(), mObjectAddSlot_as_)
  , primitive(temp(), mObjectSend_)
  , primitive(temp(), mObjectChannel)
  , primitive(temp(), mObjectEquals)
  , primitive(temp(), mObjectSelectors)
  , primitive(temp(), mObjectContentsOfSlot_)
  , primitive(temp(), mObjectIdentity)
  , primitive(temp(), mObjectInclude_)
  , primitive(temp(), mObjectNew)
  , primitive(temp(), mObjectSetProto_)
  , primitive(temp(), mObjectRespondsTo_)));

  setProto(temp(), oFalse, oObject);
  setSlotNames(temp(), oFalse, newVector(temp(), 0));
  setSlotValues(oFalse, newVector(temp(), 0));

  setProto(temp(), oInterpreter, oObject);
  setSlotNames(temp(), oInterpreter, newVector(temp(), 0));
  setSlotValues(oInterpreter, newVector(temp(), 0));

  setProto(temp(), oFile, oObject);
  setSlotNames(temp(), oFile, newVector(temp(), 2
  , sReadBytes_
  , sClose));
  setSlotValues(oFile, newVector(temp(), 2
  , primitive(temp(), mFileReadBytes_)
  , primitive(temp(), mFileClose)));

  setProto(temp(), oTrue, oObject);
  setSlotNames(temp(), oTrue, newVector(temp(), 0));
  setSlotValues(oTrue, newVector(temp(), 0));

  setProto(temp(), oArrowCode, oObject);
  setSlotNames(temp(), oArrowCode, newVector(temp(), 1
  , sFrom_));
  setSlotValues(oArrowCode, newVector(temp(), 1
  , primitive(temp(), mArrowCodeFrom_)));

  setHiddenData(oClosure, newVector(temp(), 3, oNull, newVector(temp(), 1, oNull), newVector(temp(), 1, oClosure)));
  setProto(temp(), oClosure, oObject);
  setSlotNames(temp(), oClosure, newVector(temp(), 2
  , sEnvironment
  , sTarget_withArgumentVector_));
  setSlotValues(oClosure, newVector(temp(), 2
  , primitive(temp(), mClosureEnvironment)
  , primitive(temp(), mClosureTarget_withArgumentVector_)));

  setHiddenData(oString, newAtomVector(temp(), 1, 0));
  setProto(temp(), oString, oObject);
  setSlotNames(temp(), oString, newVector(temp(), 3
  , sAppending
  , sSerialized
  , sPrint));
  setSlotValues(oString, newVector(temp(), 3
  , primitive(temp(), mStringAppending)
  , primitive(temp(), mStringSerialized)
  , primitive(temp(), mStringPrint)));

  setHiddenData(oInteger, newAtomVector(temp(), 1, 0));
  setProto(temp(), oInteger, oObject);
  setSlotNames(temp(), oInteger, newVector(temp(), 7
  , sPlus
  , sIsLessThan
  , sIsGreaterThan
  , sSecondsDelay
  , sEquals
  , sMinus
  , sSerialized));
  setSlotValues(oInteger, newVector(temp(), 7
  , primitive(temp(), mIntegerPlus)
  , primitive(temp(), mIntegerIsLessThan)
  , primitive(temp(), mIntegerIsGreaterThan)
  , primitive(temp(), mIntegerSecondsDelay)
  , primitive(temp(), mIntegerEquals)
  , primitive(temp(), mIntegerMinus)
  , primitive(temp(), mIntegerSerialized)));

  setProto(temp(), oDynamicEnvironment, oObject);
  setSlotNames(temp(), oDynamicEnvironment, newVector(temp(), 0));
  setSlotValues(oDynamicEnvironment, newVector(temp(), 0));

  setProto(temp(), oCode, oObject);
  setSlotNames(temp(), oCode, newVector(temp(), 1
  , sInterpret));
  setSlotValues(oCode, newVector(temp(), 1
  , primitive(temp(), mCodeInterpret)));

  setProto(temp(), oVector, oObject);
  setSlotNames(temp(), oVector, newVector(temp(), 4
  , sAt_put_ifAbsent_
  , sLength
  , sAt_ifAbsent_
  , sOfLength_containing_));
  setSlotValues(oVector, newVector(temp(), 4
  , primitive(temp(), mVectorAt_put_ifAbsent_)
  , primitive(temp(), mVectorLength)
  , primitive(temp(), mVectorAt_ifAbsent_)
  , primitive(temp(), mVectorOfLength_containing_)));

  setHiddenData(oLobby, oInternals);
  setProto(temp(), oLobby, oObject);
  setSlotNames(temp(), oLobby, newVector(temp(), 31
  , sDynamicContext
  , sAbort
  , sReturn_atDepth_
  , sExit
  , sEnd
  , sReturn
  , sAbortToREPL
  , sRecurse
  , sReturn_
  , sThisContext
  , sCollectGarbage
  , sTrue
  , sFalse
  , sObject
  , sClosure
  , sInteger
  , sNull
  , sLobby
  , sFile
  , sSocket
  , sByteVector
  , sCode
  , sPromiseCode
  , sArrowCode
  , sBlock
  , sPrimitive
  , sString
  , sSymbol
  , sInterpreter
  , sException
  , sVector));
  setSlotValues(oLobby, newVector(temp(), 31
  , primitive(temp(), mLobbyDynamicContext)
  , primitive(temp(), mLobbyAbort)
  , primitive(temp(), mLobbyReturn_atDepth_)
  , primitive(temp(), mLobbyExit)
  , primitive(temp(), mLobbyEnd)
  , primitive(temp(), mLobbyReturn)
  , primitive(temp(), mLobbyAbortToREPL)
  , primitive(temp(), mLobbyRecurse)
  , primitive(temp(), mLobbyReturn_)
  , primitive(temp(), mLobbyThisContext)
  , primitive(temp(), mLobbyCollectGarbage)
  , oTrue
  , oFalse
  , oObject
  , oClosure
  , oInteger
  , oNull
  , oLobby
  , oFile
  , oSocket
  , oByteVector
  , oCode
  , oPromiseCode
  , oArrowCode
  , oBlock
  , oPrimitive
  , oString
  , oSymbol
  , newChannel(temp(), oInterpreter)
  , oException
  , oVector));

  setHiddenData(oSymbol, newAtomVector(temp(), 1, 0));
  setProto(temp(), oSymbol, oObject);
  setSlotNames(temp(), oSymbol, newVector(temp(), 1
  , sSerialized));
  setSlotValues(oSymbol, newVector(temp(), 1
  , primitive(temp(), mSymbolSerialized)));

  setProto(temp(), oPromiseCode, oObject);
  setSlotNames(temp(), oPromiseCode, newVector(temp(), 1
  , sInterpret));
  setSlotValues(oPromiseCode, newVector(temp(), 1
  , primitive(temp(), mPromiseCodeInterpret)));

  setProto(temp(), oNull, oNull);
  setSlotNames(temp(), oNull, newVector(temp(), 4
  , sIdentity
  , sInterpret
  , sPrint
  , sSerialized));
  setSlotValues(oNull, newVector(temp(), 4
  , primitive(temp(), mNullIdentity)
  , primitive(temp(), mNullInterpret)
  , primitive(temp(), mNullPrint)
  , string(temp(), "<null>")));

  setProto(temp(), oSocket, oFile);
  setSlotNames(temp(), oSocket, newVector(temp(), 6
  , sPosixFileDescriptor
  , sNew
  , sRead_
  , sListenWithBacklog_
  , sAccept
  , sBind_));
  setSlotValues(oSocket, newVector(temp(), 6
  , primitive(temp(), mSocketPosixFileDescriptor)
  , primitive(temp(), mSocketNew)
  , primitive(temp(), mSocketRead_)
  , primitive(temp(), mSocketListenWithBacklog_)
  , primitive(temp(), mSocketAccept)
  , primitive(temp(), mSocketBind_)));

  setProto(temp(), oException, oObject);
  setSlotNames(temp(), oException, newVector(temp(), 21
  , symbol(temp(), "stackFrameExpected")
  , symbol(temp(), "readingFromSocket")
  , symbol(temp(), "bindingSocket")
  , symbol(temp(), "closureExpected")
  , symbol(temp(), "integerExpected")
  , symbol(temp(), "messageNotUnderstood")
  , symbol(temp(), "primitiveExpected")
  , symbol(temp(), "outOfBounds")
  , symbol(temp(), "negativeVectorLength")
  , symbol(temp(), "acceptingSocketConnection")
  , symbol(temp(), "gettingNonexistantSlot")
  , symbol(temp(), "topLevelRecursion")
  , symbol(temp(), "badReturnDepth")
  , symbol(temp(), "missingArgument")
  , symbol(temp(), "badArity")
  , symbol(temp(), "openingSocket")
  , symbol(temp(), "settingNonexistantSlot")
  , symbol(temp(), "vectorExpected")
  , symbol(temp(), "listeningOnSocket")
  , symbol(temp(), "closingFile")
  , symbol(temp(), "stringExpected")));
  setSlotValues(oException, newVector(temp(), 21
  , eStackFrameExpected
  , eReadingFromSocket
  , eBindingSocket
  , eClosureExpected
  , eIntegerExpected
  , eMessageNotUnderstood
  , ePrimitiveExpected
  , eOutOfBounds
  , eNegativeVectorLength
  , eAcceptingSocketConnection
  , eGettingNonexistantSlot
  , eTopLevelRecursion
  , eBadReturnDepth
  , eMissingArgument
  , eBadArity
  , eOpeningSocket
  , eSettingNonexistantSlot
  , eVectorExpected
  , eListeningOnSocket
  , eClosingFile
  , eStringExpected));

  setProto(temp(), oInternals, oNull);
  setSlotNames(temp(), oInternals, newVector(temp(), 2
  , sMethodBody
  , sVectorLiteral));
  setSlotValues(oInternals, newVector(temp(), 2
  , primitive(temp(), mInternalsMethodBody)
  , primitive(temp(), mInternalsVectorLiteral)));

  setProto(temp(), oByteVector, oObject);
  setSlotNames(temp(), oByteVector, newVector(temp(), 1
  , sLength));
  setSlotValues(oByteVector, newVector(temp(), 1
  , primitive(temp(), mByteVectorLength)));

  setProto(temp(), oBlock, oObject);
  setSlotNames(temp(), oBlock, newVector(temp(), 1
  , sInterpret));
  setSlotValues(oBlock, newVector(temp(), 1
  , primitive(temp(), mBlockInterpret)));
}
