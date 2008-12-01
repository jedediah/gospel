#   Copyright 2008 Sam Chapin
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

# The exception-handling system that the core expects.
dynamicContext raise: exception {
  "\nUnhandled exception: " print. exception print
  abortToREPL
}
lobby raise: exception { dynamicContext applyHandlerTo: exception }
closure except: handle: {
  dynamicContext applyHandlerTo: exception {
    self applyHandlerTo: exception { self proto applyHandlerTo: exception } # For when $handle: reraises.
    handle: exception
  }
  self
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

# The preceding code must be present in order for the interpreter to function correctly.
# The following code creates core library functionality, things that the interpreter doesn't
#  require but which any real program likely will.

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
vector each: iterateOn: {
  index = 0
  { index < self length else: { ^^ self }
    iterateOn: self :at: index
    index := index + 1
    recurse
  } do
}
vector mapping: valueFor: {
  result = vector ofLength: self length
  index = 0
  { index < self length else: { ^^ result }
    result at: index put: (valueFor: self :at: index)
    index := index + 1
    recurse
  } do
}
vector injecting: valueOf:with: {
  accumulation = self at: 0
  index = 1
  { index < self length else: { ^^ accumulation }
    accumulation := valueOf: accumulation with: self :at: index
    index := index + 1
    recurse
  } do
}
vector serialized {
  self length == 0 if: { ^ "[]" }
  "[" ++ (self :mapping: { x | x serialized } :injecting: { x y | x ++ ", " ++ y } ++ "]")
}

object print { self serialized print }

vector print { self each: { x | x print } }

# In a later release it will be possible to load a file with an arbitrary object (the target of $include:)
# as the toplevel namespace. In the current release, files are always loaded into the lobby.
lobby include: filename { interpreter @include: filename -> end. filename }
