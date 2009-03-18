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

# Used by the parser to construct cascades.
object cascading: target { "Syntax error in cascade - leftmost message had a target." raise. exit }
defaultMessageTarget cascading: target { target }

object serialized = "<object>"
true serialized = "<true>"
false serialized = "<false>"

object do: {
  appliedTo: args {
    args == [] if: { ^ self }
    exception badArity raise
  }
  value                   { appliedTo: [          ] }
}
block do: {
  value: arg              { appliedTo: [arg       ] }
  value: arg1 value: arg2 { appliedTo: [arg1, arg2] }
}

## The exception-handling system that the core expects.
object raise { dynamicContext handler value: self }
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

object do: {
  include: fileName {
    fileEnvironment = dynamicContext new
    value = self include: fileName in: fileEnvironment
    dynamicContext proto setNamespaces: (dynamicContext proto namespaces ++ fileEnvironment namespaces) nub
    value
  }
  # TODO: Is there any reason we can't just allow any object as a namespace token?
  namespace: name {
    (newNamespace = namespace new) serialized = "<" ++ name ++ "!>"
    dynamicContext proto setNamespaces: ([newNamespace] ++ dynamicContext proto namespaces) nub
    newNamespace
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

exception missingElement = "Missing collection element."

block do: {
  over: collection { collection mapping: self }
  over: collection1 and: collection2 {
    collection1 length == collection2 length else: { "Bad collection lengths." raise }
    index = 0
    result = collection1 ofLength: collection1 length
    { index < result length else: { ^^ result }
      result at: index put: (value: collection1 :at: index value: collection2 :at: index) 
      index := index + 1
      recurse
    } value
  }
}

vector do: {
  at: index {
    at: index ifAbsent: { exception missingElement raise }
  }
  at: index put: value {
    at: index put: value ifAbsent: { exception missingElement raise }
  }
  ofLength: n {
    ofLength: n containing: null
  }
  asVector { self }
  == object {
    length == object length else: { ^ false }
    { x y | x == y else: { ^^ false } } over: self and: object
    true
  }
  each: iteration {
    index = 0
    { index < self length else: { ^^ self }
      iteration value: self :at: index
      index := index + 1
      recurse
    } value
  }
  mapping: iteration {
    result = vector ofLength: length
    index = 0 
    { index < length else: { ^^ result }
      result at: index put: (iteration value: self :at: index)
      index := index + 1
      recurse
    } value
  }
  first { at: 0 }
  rest {
    rest = ofLength: length - 1
    index = 1
    { index < length else: { ^^ rest }
      rest at: index - 1 put: self :at: index
      index := index + 1
      recurse
    } value
  }
  injecting: value into: operator {
    accumulation = value
    each: { x | accumulation := operator value: accumulation value: x }
    accumulation 
  }
  selecting: selector {
    injecting: [] into: { collection element |
      selector :value: element if: { collection ++ [element] } else: collection
    }
  }
  occurrencesOf: object {
    injecting: 0 into: { count element | element == object if: { count + 1 } else: count }
  }
  nub {
    injecting: [] into: { nub element |
      nub :occurrencesOf: element == 0 if: { nub ++ [element] } else: nub
    }
  }
  serialized {
    length == 0 if: { ^ "[]" }
    elements = mapping: { x | x serialized }
    "[" ++ (elements rest injecting: elements first into: { x y | x ++ ", " ++ y }) ++ "]"
  }
  print { each: { element | element print } }
}

object do: {
  print { self serialized print }
  printLine { self print. "\n" print }
}

