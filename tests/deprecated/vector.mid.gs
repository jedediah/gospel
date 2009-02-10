=== Vector

=== Program

[$adam, $eve] printLine

"\n--- ==\n" printLine
  ([] == []) printLine
  ([] == [1]) printLine
  ([$a] == [$a]) printLine
  ([1] == [1]) printLine
  ([9, 1] == [1, 9]) printLine

=== Expected StdOut

$adam
$eve

--- ==

<true>
<false>
<true>
<true>
<false>

=== Expected StdErr