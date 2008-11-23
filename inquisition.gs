declareTestSuite {
  self run {
    (suite = self) :respondsTo: $setup else: { suite setup {} }
     suite         :respondsTo: $teardown else: { suite teardown {} }
    failures = 0
    suite selectors each: { selector |
      suite setup
      { { suite send: selector } except:
         { e | e == exception failedAssertion if: { ^^ failures := failures + 1 } else: { raise: e } }
      } do
      suite teardown
    }
  }
}
