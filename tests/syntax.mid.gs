=== Gospel Syntax Tests

Grossly incomplete.

=== Program

# comment1

#comment2
##comment3

 # comment4
null
# comment5
foo { #comment6 }
  "uncommented\n" print
  ("parens\n"#)
  ) print
  ["array literal\n",#]1,
  2] print
  [
  ] print
  [#3]
  ] print
} do

=== Expected StdOut

uncommented
parens
array literal
2

=== Expected StdErr