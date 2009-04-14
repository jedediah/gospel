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

new { self instance }

# Used by the parser to construct cascades.
cascading: target { "Syntax error in cascade - leftmost message had a target." raise. exit }
defaultMessageTarget cascading: target { target }

serialized = "<object>"
true serialized = "<true>"
false serialized = "<false>"

appliedTo: args {
  args length == 0 if: { ^ self }
  exception badArity raise
}
value { appliedTo: [] }

block do: {
  value: arg              { appliedTo: [arg       ] }
  value: arg1 value: arg2 { appliedTo: [arg1, arg2] }
}

## The exception-handling system that the core expects.
raise { dynamicContext handler value: self }
block do: {
  # Establish the target block as a handler in the current environment. Allows execution to return back through the $raise: expression if the block does not perform a nonlocal exit, which may yield strange behaviour for exceptions raised by primitives.
  handleExceptions {
    # Install a handler into the enclosing dynamic context that will apply us to the exceptions it catches.
    (enclosingDynamicContext = dynamicContext proto) handler = { exception |
      # Prepare to hand off to the next-innermost handler if our handler raises an exception.
      dynamicContext handler = { exception | enclosingDynamicContext proto handler value: exception }
      # Apply our handler.
      value: exception
    }
  }
  # Execute the target block. If it raises an exception, return the result of applying the argument block to the exception object.
  except: handler {
    { e | ^ handler value: e } handleExceptions
    value
  }
}

# A default handler.
{ e | "\nUnhandled exception: " print. e print. "\n" print
      exit
} handleExceptions

exception do: {
  # The $selector method is added by the C code that creates these objects.
  badType serialized {
    "Message target or argument has incorrect primitive type: " ++ selector serialized
  }
  messageNotUnderstood serialized {
    "Message not understood: " ++ selector serialized
  }
}

inclusionPaths = ["lib/"]
inclusionFileExtensions = [".gs", ""]
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

if: yes          { yes  value }
if: yes else: no { yes  value }
        else: no { self       }

false do: {
  if: yes          { self       }
  if: yes else: no { no   value }
          else: no { no   value }
}

== anObject { self is: anObject }

and: aBlock { aBlock value }
false and: aBlock { self }
or: aBlock { self }
false or: aBlock { aBlock value }
not { false }
false not { true }

# TODO: Generalize or eliminate.
block do: {
  over: collection { collection mapping: self }
  over: collection1 and: collection2 {
    collection1 length == collection2 length else: { "Bad collection lengths." raise }
    index = 0
    result = collection1 ofLength: collection1 length
    { index < result length else: { ^^ result }
      result at: index put: value: (collection1 at: index) value: (collection2 at: index)
      index := index + 1
      recurse
    } value
  }
}

### The ordered collection protocol.
exception missingElement = "Missing collection element."
absoluteIndex: i {
  i < 0 if: { ^ length + i }
  i
}
as: anOrderedCollection {
  result = anOrderedCollection ofLength: length;
   eachIndex: { i | result at: i put: at: i }
}
at: index {
  at: index ifAbsent: { exception missingElement raise }
}
at: index put: value {
  at: index put: value ifAbsent: { exception missingElement raise }
}
ofLength: n {
  ofLength: n containing: null
}
eachIndex: aBlock {
  index = 0
  { index < length else: { ^^ self }
    aBlock value: index
    index := index + 1
    recurse
  } value
}
each: aBlock {
  eachIndex: { i | aBlock value: at: i }
}
map: aBlock {
  eachIndex: { i | at: i put: (aBlock value: at: i) }
}
mapping: aBlock {
  result = ofLength: length;
   eachIndex: { i | result at: i put: (aBlock value: at: i) }
}
first {
  at: 0
}
rest {
  result = ofLength: length - 1;
   eachIndex: { i | result at: i put: at: i + 1 }
}
injecting: accumulation into: operator {
  each: { x | accumulation := operator value: accumulation value: x }
  accumulation
}
selecting: selector {
  injecting: (ofLength: 0) into: { collection element |
    selector value: element; if: { collection ++ containing: element } else: collection
  }
}
occurrencesOf: anObject {
  injecting: 0 into: { count element | element == anObject if: { count + 1 } else: count }
}
nub {
  injecting: (ofLength: 0) into: { nub element |
    nub occurrencesOf: element; == 0 if: { nub ++ containing: element } else: nub
  }
}
containing: element {
  ofLength: 1 containing: element
}

# TODO: Reconsider this design, it seems like an abuse of polymorphism.
vector print { each: { element | element print } }

vector do: {
  == object {
    length == object length else: { ^ false }
    { x y | x == y else: { ^^ false } } over: self and: object
    true
  }
  serialized {
    length == 0 if: { ^ "[]" }
    elements = mapping: { x | x serialized }
    "[" ++ (elements rest injecting: elements first into: { x y | x ++ ", " ++ y }) ++ "]"
  }
}

string ofLength: length { ofLength: length containing: " " }

print { self serialized print }
printLine { self print. "\n" print }

file do: {
  path = ""
  named: newPath {
    self new do: { path = newPath }
  }
}

TCPSocket do: {
  maximumConnectionBacklog = 42
}

integer successor { + 1 }

object do: {
  < anObject { > anObject or: { == anObject }; not }
  > anObject { < anObject or: { == anObject }; not }
  <= anObject { > anObject; not }
  >= anObject { < anObject; not }
}

range do: {
  first = 0
  last = 0

  length {
    last - first
  }
  at: index ifAbsent: aBlock {
    index := absoluteIndex: index
    index < 0 or: { length - 1 < index }; if: { ^ aBlock value }
    first + index
  }
  from: first to: last {
    self new do: { first = first. last = last }
  }
  from: first through: last {
    from: first to: last + 1
  }
  each: aBlock {
    current = first
    { current < last else: { ^^ self }
      aBlock value: current
      current := current successor
      recurse
    } value
  }
  of: aCollection {
    result = aCollection ofLength: (aCollection absoluteIndex: last) - (aCollection absoluteIndex: first);
     eachIndex: { i | result at: i put: (aCollection at: first + i) }
  }
  serialized {
    first serialized ++ "..." ++ last serialized
  }
}

methods {
  object is: self; if: [] else: { self proto methods }; ++ self localMethods
}

