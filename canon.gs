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
lobby raise: exception { dynamicContext raise: exception }
closure except: handle: {
  dynamicContext raise: exception {
    self raise: exception { self proto raise: exception } # For when $handle: reraises.
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

vector mapping: valueFor: {
  domain = self
  range = vector ofLength: domain length
  index = 0
  { index < domain length else: { ^^ range }
    range at: index put: (valueFor: domain :at: index)
    index := index + 1
    recurse
  } do
}

vector injecting: using:with: {
  vector = self
  accumulation = vector at: 0
  index = 1
  { index < vector length else: { ^^ accumulation }
    accumulation := using: accumulation with: vector :at: index
    index := index + 1
    recurse
  } do
}
vector injecting: using:with: with: accumulation {
  vector = self
  index = 0
  { index < vector length else: { ^^ accumulation }
    accumulation := using: accumulation with: vector :at: index
    index := index + 1
    recurse
  } do
}

vector serialized {
  self length == 0 if: { ^ "[]" }
  "[" ++ (self :mapping: { x | x serialized } injecting: { x y | x ++ ", " ++ y }) ++ "]"
}

integer times: iterate {
  count = self
  { count == 0 if: { ^^ }
    iterate
    count := count - 1
    recurse
  } do
}

# In a later release it will be possible to load a file with an arbitrary object (the target of $include:)
# as the toplevel namespace. In the current release, files are always loaded into the lobby.
lobby include: filename { interpreter @include: filename -> end. filename }
