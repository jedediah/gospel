=== Object

=== Program

"\nlogic\n" printLine
  true if: { "a" print }
  false if: { "b" print }
  true else: { "c" print }
  false else: { "d" print }
  true if: { "e" print } else: { "f" print }
  false if: { "g" print } else: { "h" print }

=== Expected StdOut

logic

adeh

=== Expected StdErr