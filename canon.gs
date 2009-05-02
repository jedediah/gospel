#   Copyright © 2008, 2009 Sam Chapin
#
#   This file is part of Gospel.
#
#   Gospel is free software: you can redistribute it and/or modify
#   it under the terms of version 3 of the GNU General Public License
#   as published by the Free Software Foundation.
#
#   Gospel is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with Gospel.  If not, see <http://www.gnu.org/licenses/>.

object new { self instance }

# Used by the parser to construct cascades.
object cascading: target { "Syntax error in cascade - leftmost message had a target." raise. exit }
defaultMessageTarget cascading: target { target }

object serialized = "<object>"
true serialized = "<true>"
false serialized = "<false>"

object do: {
  appliedTo: args {
    args length == 0 if: { ^ self }
    exception badArity raise
  }
  value { self appliedTo: [] }
}

block do: {
  value: arg              { self appliedTo: [arg       ] }
  value: arg1 value: arg2 { self appliedTo: [arg1, arg2] }
}

## The exception-handling system that the core expects.
block do: {
  # Establish the target block as a handler in the current environment. Allows execution to return back through the $raise: expression if the block does not perform a nonlocal exit, which may yield strange behaviour for exceptions raised by primitives.
  handleExceptions {
    handler = self
    # Install a handler into the enclosing dynamic context that will apply us to the exceptions it catches.
    (enclosingEnvironment = dynamicContext proto) handle: exception {
      # Prepare to hand off to the next-innermost handler if our handler raises an exception.
      dynamicContext handle: exception { enclosingEnvironment proto handle: exception }
      # Apply our handler.
      handler value: exception
    }
  }
  # Execute the target block. If it raises an exception, return the result of applying the argument block to the exception object.
  except: handler {
    { e | ^ handler value: e } handleExceptions
    self value
  }
}

# A default handler.
{ e | "\nUnhandled exception: " print. e print. "\n" print
      exit
} handleExceptions

exception do: {
  # The $selector method is added by the C code that creates these objects.
  badType serialized {
    "Message target or argument has incorrect primitive type: " ++ self selector serialized
  }
  messageNotUnderstood serialized {
    "Message not understood: " ++ self selector serialized
  }
}

inclusionPaths = ["lib/"]
inclusionFileExtensions = [".gs", ""]
object do: {
  include: filename in: environment {
    inclusionPaths each: { path |
      inclusionFileExtensions each: { extension |
        { ^^^ self includeExactPath: path ++ filename ++ extension in: environment } except:
         { e | e is: exception inclusion; else: { e raise } }
      }
    }
    "File to be included was not found: " ++ filename; raise
  }
  include: filename {
    fileEnvironment = dynamicContext new
    lastValue = self include: filename in: fileEnvironment
    dynamicContext proto setNamespaces: (dynamicContext proto namespaces ++ fileEnvironment namespaces) nub
    lastValue
  }
  namespace {
    dynamicContext proto setNamespaces: ([self] ++ dynamicContext proto namespaces) nub
    self
  }
  tap: aBlock {
    aBlock value: self
    self
  }
}

object do: {
  if: yes          { yes  value }
  if: yes else: no { yes  value }
          else: no { self       }
}

false do: {
  if: yes          { self       }
  if: yes else: no { no   value }
          else: no { no   value }
}

object == anObject { self is: anObject }

object and: aBlock { aBlock value }
false  and: aBlock { self }
object or: aBlock { self }
false  or: aBlock { aBlock value }
object not { false }
false  not { true }

# TODO: Generalize or eliminate.
block do: {
  over: collection { collection mapping: self }
  over: collection1 and: collection2 {
    collection1 length == collection2 length else: { "Bad collection lengths." raise }
    index = 0
    result = collection1 ofLength: collection1 length
    { index < result length else: { ^^ result }
      result at: index put: (self value: collection1 :at: index value: collection2 :at: index)
      index := index + 1
      recurse
    } value
  }
}

### The ordered collection protocol.
exception missingElement = "Missing collection element."
object do: {
  absoluteIndex: i {
    i < 0 if: { ^ self length + i }
    i
  }
  as: anOrderedCollection {
    result = anOrderedCollection ofLength: self length;
     eachIndex: { i | result at: i put: self :at: i }
  }
  at: index {
    self at: index ifAbsent: { exception missingElement raise }
  }
  at: index put: value {
    self at: index put: value ifAbsent: { exception missingElement raise }
  }
  ofLength: n {
    self ofLength: n containing: null
  }
  eachIndex: aBlock {
    index = 0
    { index < self length else: { ^^ self }
      aBlock value: index
      index := index + 1
      recurse
    } value
  }
  each: aBlock {
    self eachIndex: { i | aBlock value: self :at: i }
  }
  map: aBlock {
    self eachIndex: { i | at: i put: (aBlock value: self :at: i) }
  }
  mapping: aBlock {
    result = self ofLength: self length;
     eachIndex: { i | result at: i put: (aBlock value: self :at: i) }
  }
  first {
    self at: 0
  }
  rest {
    result = self ofLength: self length - 1;
     eachIndex: { i | result at: i put: (self at: i + 1) }
  }
  injecting: accumulation into: operator {
    self each: { x | accumulation := operator value: accumulation value: x }
    accumulation
  }
  selecting: selector {
    self injecting: self :ofLength: 0 into: { collection element |
      selector value: element; if: { collection ++ self containing: element } else: collection
    }
  }
  occurrencesOf: anObject {
    self injecting: 0 into: { count element | element == anObject if: { count + 1 } else: count }
  }
  nub {
    self injecting: self :ofLength: 0 into: { nub element |
      nub occurrencesOf: element; == 0 if: { nub ++ (self containing: element) } else: nub
    }
  }
  containing: element {
    self ofLength: 1 containing: element
  }
}

# TODO: Reconsider this design, it seems like an abuse of polymorphism.
vector print { self each: { element | element print } }

vector do: {
  == anObject {
    self length == anObject length else: { ^ false }
    { x y | x == y else: { ^^ false } } over: self and: anObject
    true
  }
  serialized {
    self length == 0 if: { ^ "[]" }
    elements = self mapping: { x | x serialized }
    "[" ++ (elements rest injecting: elements first into: { x y | x ++ ", " ++ y }) ++ "]"
  }
}

string ofLength: length { self ofLength: self length containing: " " }

object do: {
  print { self serialized print }
  printLine { self print. "\n" print }
}

file do: {
  path = ""
  named: newPath {
    self new do: { path = newPath }
  }
}

TCPSocket do: {
  maximumConnectionBacklog = 42
}

integer successor { self + 1 }

object do: {
  < anObject { self > anObject or: { self == anObject }; not }
  > anObject { self < anObject or: { self == anObject }; not }
  <= anObject { self > anObject; not }
  >= anObject { self < anObject; not }
}

range do: {
  first = 0
  last = 0

  length {
    self last - self first
  }
  at: index ifAbsent: aBlock {
    index := self absoluteIndex: index
    index < 0 or: { self length - 1 < index }; if: { ^ aBlock value }
    self first + index
  }
  from: first to: last {
    self new do: { first = first. last = last }
  }
  from: first through: last {
    from: first to: last + 1
  }
  each: aBlock {
    current = self first
    { current < self last else: { ^^ self }
      aBlock value: current
      current := current successor
      recurse
    } value
  }
  of: aCollection {
    result = aCollection ofLength:
              (aCollection absoluteIndex: self last) - (aCollection absoluteIndex: self first);
     eachIndex: { i | result at: i put: (aCollection at: first + i) }
  }
  serialized {
    self first serialized ++ "..." ++ self last serialized
  }
}

object methods {
  object is: self; if: [] else: { self proto methods }; ++ self localMethods
}

