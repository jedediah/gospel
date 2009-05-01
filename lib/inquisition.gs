#   Copyright © 2008 Sam Chapin
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

"inquisition" namespace

assert { self }
false assert { suite failedAssertion raise }

suite = new do: {
  new {
    suite instance do: { tests = object new }
  }

  tests = object new
  failedAssertion = "Failed assertion in unit test."
 
  setup {}
  teardown {}
  run {
    failures = exceptions = 0
    "Tests: " ++ (self tests localMethods each: { selector |
                    self setup
                    { self tests send: selector } except: { e |
                      e is: failedAssertion; if: { ^ failures := failures + 1 }
                      exceptions := exceptions + 1
                    }
                    self teardown
                  }; length serialized) ++
     "\nFailures: " ++ failures serialized ++
      "\nExceptions: " ++ exceptions serialized
  }
}

