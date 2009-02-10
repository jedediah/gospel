Literals

  : { 1 print } "1"
  : { 0090 print } "90"
  : -i ../lib/octal.gs { 0100 print } "64"
  
  % 0090  # => 90
  % 077   # => 77
  % -0    # => 0
  % -1    # => -1
  
  %=
  =%
    

=== Arithmatic

Basic stuff only currently

=== Program

"\n--- Subtraction\n" printLine
  (1 - 1) printLine
  (1 - 2) printLine
  (10 - 3) printLine
  (-2147483648 - 1) printLine
  { 1 - false } except: \printException
  { 1 - [0] } except: \printException
  { 1 - null } except: \printException

"\n--- Greater Than\n" printLine
  (1 > 1) printLine
  (2 > 1) printLine
  (10 > 1) printLine
  {2 > 3} printLine
  { 1 > false } except: \printException
  { 1 > [0] } except: \printException
  { 1 > null } except: \printException
  
"\n--- Less Than\n" printLine
  (1 < 1) printLine
  (2 < 1) printLine
  (10 < 11) printLine
  { 2 < 3 } printLine
  { 1 < false } except: \printException
  { 1 < [0] } except: \printException
  { 1 < null } except: \printException
  
=== Expected StdOut

--- Integer Literals

1
90
77

--- Addition

2
11
0
-2
2147483647
-2147483648
Received: Non-integer encountered where integer expected.
Received: Non-integer encountered where integer expected.
Received: Non-integer encountered where integer expected.

--- Subtraction

0
-1
7
2147483647
Received: Non-integer encountered where integer expected.
Received: Non-integer encountered where integer expected.
Received: Non-integer encountered where integer expected.

--- Greater Than

<false>
2
10
<false>
Received: Non-integer encountered where integer expected.
Received: Non-integer encountered where integer expected.
Received: Non-integer encountered where integer expected.

--- Less Than

<false>
<false>
10
2
Received: Non-integer encountered where integer expected.
Received: Non-integer encountered where integer expected.
Received: Non-integer encountered where integer expected.

=== Expected StdErr