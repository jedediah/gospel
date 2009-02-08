include: "lib/inquisition.gs"

integerTest = object new
returnException: e { e }

integerTest additionWithNonIntegralTypes {
  (({ 1 + false } except: \returnException:) == "Expected an integer") assert
  (({ 1 + true } except: \returnException:) == "Expected an integer") assert
  (({ 1 + [] } except: \returnException:) == "Expected an integer") assert
  (({ 1 + object } except: \returnException:) == "Expected an integer") assert
  (({ 1 + "" } except: \returnException:) == "Expected an integer") assert 
}

integerTest greaterThanWithNonIntegralTypes {
  (({ 1 > object } except: \returnException:) == "Expected an integer") assert
  (({ 1 > null } except: \returnException:) == "Expected an integer") assert
}

integerTest lessThanWithNonIntegralTypes {
  (({ 1 < object } except: \returnException:) == "Expected an integer") assert
  (({ 1 < null } except: \returnException:) == "Expected an integer") assert
}

integerTest declareTestSuite
integerTest run printLine
exit
