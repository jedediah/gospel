# Literals

  # Basic

    eq    1   1  # one
    eq    0   0  # zero
    eq   10  10  # something other than one and zero
    eq   -1  -1  # negative one
    eq   -0   0  # negative zero
    eq  -10 -10  # capable of more than just negative one
    eq   01   1  # single leading zero one thing
    eq   05   5  # single leading zero another
    eq  010  10  # single leading zero with valid octal number that would appear different in base10
    eq   00   0  # double zero
     
    eq  00001  1 # multiple leading zeros
    eq  00090 90 # multiple leading zeros and following zeros
  
  # Maximal Integers

    eq  2147483647  2147483647 # positive
    eq -2147483648 -2147483648 # negative
  
  # Overflows

    eq  9999999999  2147483647 # positive
    eq -9999999999 -2147483648 # negative

# Addition

  # Basic
  
    eq  ' 1 +  1'     2 # both positive with one positive result
    eq  ' 1 +  2'     3 # both positive with another positive result
    eq  '-1 +  2'     1 # one positive, one negative with one positive result
    eq  ' 4 + -2'     2 # one negative, one positive with another positive result
    eq  '-2 + -3'    -5 # both negative with one negative result
    eq  '-2 + -4'    -6 # both negative with another negative result
    eq  ' 1 +  0'     1 # positive and zero with one result
    eq  ' 5 +  0'     5 # positive and zero with another result
    eq  '-1 +  0'    -1 # negative and zero with one result
    eq  '-5 +  0'    -5 # negative and zero with another result 
    eq  ' 0 +  0'     0 # both zero
  
    eq  ' 1 +  1 +  1'  3 # all positive chain of 3
    eq  '-1 + -1 + -1' -3 # all negative chain of 3
    eq  ' 1 +  0 + -1'  0 # one mixed chain
    eq  ' 5 +  5 + -3'  7 # another mixed chain
  
  # Overflows
  
    eq ' 2147483647 +  0'  2147483647
    eq '-2147483648 +  0' -2147483648
    eq ' 2147483647 +  1' -2147483648
    eq '-2147483648 + -1'  2147483647
    eq ' 2147483647 +  5' -2147483644
    eq '-2147483648 + -5'  2147483643

# Comparion Operators

  # Equality
  
    eq '1 == 1'      1         # same to same returning one thing
    eq '2 == 2'      2         # same to same returning another
    eq '1 == 2'      '<false>' # different of same type
    eq '1 == object' '<false>' # different of different type
    eq '1 == null'   '<false>' # with null
                           
  # Greater Than
  
    eq '1 > 1'       '<false>' # positive same to same
    eq '1 > 5'       '<false>' # positive smaller to bigger
    eq '2 > 1'       2         # positive bigger to smaller returning one thing
    eq '5 > 1'       5         # positive bigger to smaller returning another
  
    eq '-1 > -1'     '<false>' # negative same to same
    eq '-3 > -2'     '<false>' # negative smaller to bigger
    eq '-2 > -5'     -2        # negative bigger to smaller returning one thing
    eq '-3 > -5'     -3        # negative bigger to smaller returning another

    eq ' 1 > -1'     1         # mixed 
    eq '-1 >  1'     '<false>' # mixed
    
  # Less Than
  
    eq '1 < 1'       '<false>' # positive same to same
    eq '2 < 1'       '<false>' # positive bigger to smaller
    eq '2 < 5'       2         # positive smaller to bigger returning one thing
    eq '4 < 5'       4         # positive smaller to bigger returning another
         
    eq '-1 < -1'     '<false>' # negative same to same
    eq '-2 < -3'     '<false>' # negative bigger to smaller
    eq '-2 < -1'     -2        # negative smaller to bigger returning one thing
    eq '-3 < -1'     -3        # negative smaller to bigger returning another
         
    eq ' 1 < -1'     '<false>' # mixed 
    eq '-1 <  1'     -1        # mixed
    