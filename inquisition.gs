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

exception failedAssertion = "Failed assertion in unit test."
object assert { self }
false  assert { raise: exception failedAssertion }

# Turn an object into a test suite. Should be used after all test slots are added but before
# any non-test slots (including $setup and/or $teardown) are added.
object declareTestSuite {
  tests = self selectors
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
    inflect: string with: suffix for: number {
      number serialized ++ (number == 1 if: string else: { string ++ suffix })
    }
    total = tests length
    passes = total - failures - exceptions
    (inflect: " test" with: "s" for: total) ++ " run: " ++
     (inflect: " pass" with: "es" for: passes) ++ ", " ++
      (inflect: " failure" with: "s" for: failures) ++ ", " ++
       (inflect: " exception" with: "s" for: exceptions) ++ "."
  }
}
