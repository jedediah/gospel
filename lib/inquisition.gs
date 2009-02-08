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

namespace: "inquisition"

exception failedAssertion = "Failed assertion in unit test."
object assert { self }
false  assert { raise: exception failedAssertion }

# Turn an object into a test suite. Should be used after all test slots are added but before
# any non-test slots (including \setup and/or \teardown) are added.
object declareTestSuite {
  tests = self localSelectors
  self setup {}
  self teardown {}
  self run {
    failures = exceptions = 0
    tests each: { selector |
      self setup
      { { self send: selector } except: { e |
          e == exception failedAssertion if: { ^^ failures := failures + 1 }
          ^ exceptions := exceptions + 1
        }
      } do
      self teardown
    }
    
    totalTests = tests length
    passes = totalTests - failures - exceptions
    
    accumilation = totalTests serialized ++ " tests. "
    accumilation := accumilation ++ passes serialized ++ " passes, "
    accumilation := accumilation ++ failures serialized ++ " failures, "
    accumilation := accumilation ++ exceptions serialized ++ " exceptions"
  }
}
