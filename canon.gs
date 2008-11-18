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

stackFrame return: value { self return: value atDepth: 0 }

# The exception-handling system that the core expects.
lobby raise: exception { dynamicContext raise: exception }
closure except: handle: {
  dynamicContext raise: exception {
    self raise: exception { self proto raise: exception }
    handle: exception
  }
  self
}

closure do { self }

object if: yieldTrue { yieldTrue }
false  if: yieldTrue { }
object if: yieldTrue else: yieldFalse { yieldTrue }
false  if: yieldTrue else: yieldFalse { yieldFalse }

integer times: yield {
  count = self
  { count == 0 if: { ^^ }
    yield
    count := count - 1
    recurse
  } do
}
