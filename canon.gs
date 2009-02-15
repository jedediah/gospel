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

object serialized = "<object>"
true serialized = "<true>"
false serialized = "<false>"

object do { self }

## The exception-handling system that the core expects.
lobby raise: exception { dynamicContext applyHandlerTo: exception }
# Establish the target block as a handler in the current environment. Allows execution to return back
# through the \raise: expression if the block does not perform a nonlocal exit, which may yield strange
# behaviour for exceptions raised by primitives.
closure handleExceptions {
  handle: = \self
  # Install a handler that will apply us to the exceptions it catches.
  dynamicContext proto applyHandlerTo: exception {
    # Set up a new handler in place of ourself, to hand off to the next-innermost handler (if any)
    # should \handle: decide to raise an exception itself.
    self applyHandlerTo: exception { self proto applyHandlerTo: exception }
    handle: exception
  }
}
# Execute the target block. If it raises an exception, return the result of applying the argument block
# to the exception object.
closure except: \handle: {
  { e | ^ handle: e } handleExceptions
  self
}
# A default handler.
{ e | "\nUnhandled exception: " print
      e printLine
      exit
} handleExceptions

object include: fileName {
  fileEnvironment = dynamicContext new
  value = self include: fileName in: fileEnvironment
  dynamicContext proto setNamespaces: (dynamicContext proto namespaces ++ fileEnvironment namespaces) nub
  value
}

lobby namespace: name {
  (newNamespace = namespace new) serialized = "<" ++ name ++ "!>"
  dynamicContext proto setNamespaces: ([newNamespace] ++ dynamicContext proto namespaces) nub
  newNamespace
}

exception missingElement = "Missing collection element."
vector at: index {
  self at: index ifAbsent: { raise: exception missingElement }
}
vector at: index put: value {
  self at: index put: value ifAbsent: { raise: exception missingElement }
}
vector ofLength: n {
  self ofLength: n containing: null
}

object if: yes          { yes  }
false  if: yes          { self }
object if: yes else: no { yes  }
false  if: yes else: no { no   }
object         else: no { self }
false          else: no { no   }

object target: target with: arguments {
  \self target: target withArgumentVector: arguments asVector
}
object target: target {
  \self target: target withArgumentVector: []
}

vector asVector { self } 
vector each: \iterateOn: {
  index = 0
  { index < self length else: { ^^ self }
    iterateOn: self :at: index
    index := index + 1
    recurse
  } do
}
vector mapping: \valueFor: {
  result = vector ofLength: self length
  index = 0
  { index < self length else: { ^^ result }
    result at: index put: (valueFor: self :at: index)
    index := index + 1
    recurse
  } do
}

vector first { self at: 0 }
vector rest {
  rest = vector ofLength: self length - 1
  index = 1
  { index < self length else: { ^^ rest }
    rest at: index - 1 put: self :at: index
    index := index + 1
    recurse
  } do
}

vector injecting: value into: \valueOf:with: {
  accumulation = value
  self each: { x | accumulation := valueOf: accumulation with: x }
  accumulation 
}

vector selecting: \selects: {
  self injecting: [] into: { collection element |
    :selects: element if: { collection ++ [element] } else: collection
  }
}

vector occurrencesOf: object {
  self injecting: 0 into: { count element | element == object if: { count + 1 } else: count }
}

vector nub {
  self injecting: [] into: { nub element |
    nub :occurrencesOf: element == 0 if: { nub ++ [element] } else: nub
  }
}

vector serialized {
  self length == 0 if: { ^ "[]" }
  elements = self mapping: { x | x serialized }
  "[" ++ (elements rest injecting: elements first into: { x y | x ++ ", " ++ y }) ++ "]"
}

object print { self serialized print }
object printLine { self print. "\n" print }
vector print { self each: { element | element print } }

TCPSocket = object new
TCPSocket maximumBacklog = 128
TCPSocket serving: port with: \handle: {
  descriptor = (server = self new) POSIXFileDescriptor = POSIX TCPSocket
  POSIX bindInternetSocket: descriptor to: port
  POSIX listenOn: descriptor withMaximumBacklog: self maximumBacklog
  { (connection = self new) POSIXFileDescriptor = POSIX acceptOn: descriptor
    @handle: connection
    recurse
  } @except: { e | server close. end } # TODO: Rethrow exceptions that are not due to socket shutdown.
  server
}
TCPSocket read: byteCount {
  POSIX read: byteCount from: self POSIXFileDescriptor
}
TCPSocket write: string {
  POSIX write: string to: self POSIXFileDescriptor
}
TCPSocket shutdown {
  POSIX shutdown: self POSIXFileDescriptor
}
TCPSocket close {
  POSIX close: self POSIXFileDescriptor
}

