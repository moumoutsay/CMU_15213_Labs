/* 
 * CS:APP Data Lab 
 * 
 * Name: Wei-Lin Tsai - weilints@andrew.cmu.edu
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
/* 
 * evenBits - return word with all even-numbered bits set to 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 8
 *   Rating: 1
 */
int evenBits(void) {
  int firstMask;
  int secondMask;
  int finalResult;

  firstMask = 85; // binary 01010101
  secondMask = firstMask | ( firstMask << 8 );
  finalResult = secondMask | ( secondMask << 16 );

  return finalResult;
}
/* 
 * isEqual - return 1 if x == y, and 0 otherwise 
 *   Examples: isEqual(5,5) = 1, isEqual(4,5) = 0
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int isEqual(int x, int y) {
  return !(x^y);
}
/* 
 * byteSwap - swaps the nth byte and the mth byte
 *  Examples: byteSwap(0x12345678, 1, 3) = 0x56341278
 *            byteSwap(0xDEADBEEF, 0, 2) = 0xDEEFBEAD
 *  You may assume that 0 <= n <= 3, 0 <= m <= 3
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 25
 *  Rating: 2
 */
int byteSwap(int x, int n, int m) {
  int valM;
  int valN;
  int shiftCountN;
  int shiftCountM;
  int result;
  int valXOR;      // byte n XOR byte m
 
  shiftCountN = n << 3;
  shiftCountM = m << 3; 

  // get val of M, N byte 
  valM = ( x >> shiftCountM ) & 255;
  valN = ( x >> shiftCountN ) & 255;

  valXOR = valM ^ valN;

  // swpap 
  result = x      ^ ( valXOR << shiftCountM);
  result = result ^ ( valXOR << shiftCountN);

  return result;
}
/* 
 * rotateRight - Rotate x to the right by n
 *   Can assume that 0 <= n <= 31
 *   Examples: rotateRight(0x87654321,4) = 0x18765432
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 25
 *   Rating: 3 
 */
int rotateRight(int x, int  n) {
  // for easy remember, let n + m = 32
  int leftMask;            // 00000011111 to mask sign bit extention after shifting 
  int valLeft;             // value of left m bits  
  int valRight;            // value of right n bits
  int valM;                // n + m = 32;

  // get m
  valM = 32 + ( ~n + 1 ); 
  
  // get right 
  valRight = x << valM;

  // prepare mask 
  leftMask = ~((~0) << valM);

  // get left 
  valLeft = (x >> n) & leftMask;
 
  return (valRight | valLeft);

}
/* 
 * logicalNeg - implement the ! operator using any of 
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int logicalNeg(int x) {
  int valAddTMAX; // x + TMAX (7fffffff) 
  int valResult; 
  
  valAddTMAX = x + (~(1 << 31));
  valResult = valAddTMAX | x ;  // msb will show carry, or it's msb is 1 itself

  return ((valResult >> 31) + 1);
}
/* 
 * TMax - return maximum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmax(void) {
  return ~( 1 << 31);
}
/* 
 * sign - return 1 if positive, 0 if zero, and -1 if negative
 *  Examples: sign(130) = 1
 *            sign(-23) = -1
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 10
 *  Rating: 2
 */
int sign(int x) {
  int signMask = x >> 31; // all 1 if negtive, otherwise 0

  return  ((!(!x)) | signMask);
}
/* 
 * isGreater - if x > y  then return 1, else return 0 
 *   Example: isGreater(4,5) = 0, isGreater(5,4) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isGreater(int x, int y) {
  // we want to see if x > y, which is equal to see if x - y > 0
  // let x - y = z, so return 1 if z > 0, 0 if z <= 0
  int valZ;
  int signMaskZ;    //  ~(z >> 31), all 0 if neg, all 1 if 0/pos 
  int valResult;
  int canOverflow;
  int doOverflow;
  int overflowMask;

  valZ = x + ( ~y + 1);
  // however, we need care abour overflow
  canOverflow = x ^ y;  // if msb is 1, must be (pos - neg) or (neg - pos)
  doOverflow = x ^ valZ;// if msb is 1, overflow happend 
  overflowMask = ((canOverflow & doOverflow) >> 31) & 1;

//  printf("x: %x, y: %x\n", x, y);
//  printf("over flow mask: %x\n", overflowMask);

  signMaskZ = ~ (valZ >> 31);
  valResult = valZ & signMaskZ;  // 0 if neg or 0, Z if Z is pos

  return ((!(!valResult)) ^ overflowMask);
}
/* 
 * subOK - Determine if can compute x-y without overflow
 *   Example: subOK(0x80000000,0x80000000) = 1,
 *            subOK(0x80000000,0x70000000) = 0, 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 3
 */
int subOK(int x, int y) {
  // let x - y = z,
  int valZ;
  int canOverflow;
  int doOverflow;

  valZ = x + ( ~y + 1);
  // check if overflow 
  canOverflow = x ^ y;  // if msb is 1, must be (pos - neg) or (neg - pos)
  doOverflow = x ^ valZ;// if msb is 1, overflow happend 
  return (!((canOverflow & doOverflow) >> 31));
}
/*
 * satAdd - adds two numbers but when positive overflow occurs, returns
 *          maximum possible value, and when negative overflow occurs,
 *          it returns minimum possible value.
 *   Examples: satAdd(0x40000000,0x40000000) = 0x7fffffff
 *             satAdd(0x80000000,0xffffffff) = 0x80000000
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 30
 *   Rating: 4
 */
int satAdd(int x, int y) {
  int valZ;
  int canOverflow;
  int doOverflow;
  int overflowMask;
  int valMinMax;
  int valResult;

  // x + y = z
  valZ = x + y;

  // check if overflow 
  canOverflow = ~(x ^ y);  // if msb is 1, must be (pos + pos) or (neg + neg)
  doOverflow = x ^ valZ;   // if msb is 1, overflow happend 

  // prepare overflow mask
  overflowMask = ( canOverflow & doOverflow) >> 31 ;  // should be all-1 or  0

  // prepare min_max part 
  valMinMax = (valZ >> 31) ^ (1 << 31);
  // prepare final result
  valResult = (valMinMax & overflowMask) + (valZ & (~overflowMask));

  return valResult;
}
/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12) = 5
 *            howManyBits(298) = 10
 *            howManyBits(-5) = 4
 *            howManyBits(0)  = 1
 *            howManyBits(-1) = 1
 *            howManyBits(0x80000000) = 32
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 90
 *  Rating: 4
 */
int howManyBits(int x) {
  int signMask;        // all 1 or 0
  int valMinusOne;
  int psuedoAbsX;
  int sixteenMask; 
  int eightMask;
  int fourMask;
  int twoMask;
  int oneMask;
           
  int isSixteenSet;
  int isEightSet;  
  int isFourSet;
  int isTwoSet;
  int isOneSet;

  // prepare mask 
  signMask = x >> 31;
  valMinusOne = ~0;
  sixteenMask = valMinusOne << 16;  // 1...1(16)_0...0(16)
  eightMask   = valMinusOne <<  8;  // 1...1(24)_0...0(8)
//  fourMask    = valMinusOne <<  4;  // 1...1(28)_0...0(4) // 240
//  twoMask     = valMinusOne <<  2;  // 1...1(30)_0...0(2) // 12
//  oneMask     = valMinusOne <<  1;  // 1...1(31)_0        // 2
  fourMask    = 240; 
  twoMask     = 12;
  oneMask     = 2;


   
  // covert to all unsigned, which is, do x = -x -1 when x is neg
  // in fact we want to do x = ~x if x is neg and x = x if x is pos
  psuedoAbsX = x ^ signMask;

  // get results
  isSixteenSet = (!!(psuedoAbsX & sixteenMask)) << 4; // 16 or 0 
  psuedoAbsX = psuedoAbsX >> isSixteenSet;            // shift 16 or not 

  isEightSet = (!!(psuedoAbsX & eightMask)) << 3;     // 8 or 0 
  psuedoAbsX = psuedoAbsX >> isEightSet;              // shift 8 or not 

  isFourSet = (!!(psuedoAbsX & fourMask)) << 2;       // 4 or 0 
  psuedoAbsX = psuedoAbsX >> isFourSet;               // shift 4 or not 

  isTwoSet = (!!(psuedoAbsX & twoMask)) << 1;         // 2 or 0 
  psuedoAbsX = psuedoAbsX >> isTwoSet;                // shift 2 or not 

  isOneSet = (psuedoAbsX & oneMask) >> 1;             // 1 or 0
  psuedoAbsX = psuedoAbsX >> isOneSet;             
 
//  printf("input: %d, isOneSet: %d, isLastSet: %d\n", x, isOneSet, isLastSet);

  return ( isOneSet + isTwoSet + isFourSet + isEightSet + isSixteenSet  + psuedoAbsX + 1/*for sign bit */ );
}
/* 
 * float_half - Return bit-level equivalent of expression 0.5*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_half(unsigned uf) {
  // TODO, can use any constant and unsigned
  int valTMIN  = 0x80000000;// 1 << 31
  int valTMAX  = 0x7fffffff;// ~valTMIN;
  int maskFrac = 0x7fffff; // valTMAX >> 8;  // mask for fraction
  int valAbsUF = uf & valTMAX;  // remove msb
  int valMSBUF = uf & valTMIN;  // store msb
  int valExp = valAbsUF >> 23; 
  int valFrac = uf & maskFrac; 
  int valResultZeroExp;         // actually, for exp == 0 or exp == 1
  int valExpMinusOne = valExp - 1;
  int valNeedRoundUp;

  /* val preparation */
  // prepare val when exp = 0;
  valNeedRoundUp = ((uf & 3) == 3);
  valResultZeroExp = (( valAbsUF >> 1) + valNeedRoundUp) | valMSBUF;

  // Val table 
  // ESP        0    1   255   2~254
  // ESP-1   f..f    0   254   1~254

  /* return control */
  if (valExp) { // exp > 0
    if (valExpMinusOne) { // exp > 1 
      if (valExp == 255) return uf;          // 255
      else return ( valMSBUF | (valExpMinusOne << 23) | valFrac ); // 2 ~ 254
    } else { // exp = 1
      return valResultZeroExp;
    }
  } else {
    return valResultZeroExp;
  }
  

// old code  // if exp == 255
// old code  if (valExp == 255) return uf;
// old code  
// old code  // if exp == 1
// old code  if (!(valExpMinusOne)) return valResultZeroExp;
// old code
// old code  // if exp != 0 
// old code  if ( valExp ) return ( valMSBUF | (valExpMinusOne << 23) | valFrac );
// old code
// old code  // now exp must be 0
// old code  return valResultZeroExp;
}
/* 
 * float_f2i - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
int float_f2i(unsigned uf) {
  int valTMIN = 0x80000000;
  int valTMAX = 0x7fffffff;
  int valFrac = (uf & 0x7fffff)| 0x800000; 
  int valAbsUF = uf & valTMAX;  // remove msb
  int valMSBUF = uf & valTMIN;  // store msb
  int valExp = valAbsUF >> 23; 
  int valResult;
  int valNoRight;               // number of shift write  
//  int maskSignShift = valTMIN;
//  unsigned maskUnsignShift =  0x80000000u;
    
  // if valAbsUF < 0.5, return 0 
  if ( valAbsUF < 0x3f000001) return 0;
 
  // if valAbsUF > max positive int, return valTMIN
  if ( valAbsUF > 0x4f000000) return valTMIN;

  // if exp == 255
//  if (!(valExp ^ 255)) return valTMIN;
 
//  printf("input %x, valexp: %d\n", uf, valExp); 
  
  if ( valExp > 149/*126 + 23*/ ){
     valResult = valFrac << (valExp - 150);
  }
  else {
     valNoRight = 150 - valExp;
     valResult = valFrac >> (valNoRight);
     // need handle rounding here
//     valNoRight = 32 - valNoRight; 
//     maskSignShift = ~(maskSignShift >> valNoRight);
//     maskUnsignShift = maskUnsignShift >> valNoRight;
//     if ( (valFrac & maskSignShift)> maskUnsignShift ) valResult = valResult + 1;  
  }

  // handle negative 
  if (valMSBUF) valResult = 0 - valResult;

  return valResult;
}
