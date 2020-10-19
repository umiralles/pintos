#ifndef THREADS_FIXED_POINT
#define THREADS_FIXED_POINT

/* Type used to represent fixed point numbers, integer with 32 bits*/
typedef int32_t fixed_point_number;

/* Number of bits in a fixed point number after the decimal point */
#define FRACTIONAL_BITS (14)

/* Number of bits in a fixed point number before the decimal point
   does not include the sign bit */
#define INTEGER_BITS (31 - FRACTIONAL_BITS)

/* The integer that represents the value 1 as a fixed point number*/
#define FIXED_POINT_ONE (1 << FRACTIONAL_BITS)

/* Multiplies a fixed point number x by the value FIXED_POINT_ONE
   uses shifts for efficiency */
#define FP_ONE_MUL(x) \
  (((fixed_point_number) x) << FRACTIONAL_BITS)

/* Divides a fixed point number by the value FIXED_POINT_ONE
   uses shifts for efficiency */
#define FP_ONE_DIV(x) \
  (((fixed_point_number) x) >> FRACTIONAL_BITS)

/* Wrapper for FP_ONE_MUL that gives a clear interface to convert an 
   int to a fixed point number */
#define INT_TO_FP(n) \
  (fixed_point_number) (FP_ONE_MUL(n))

/* Wrapper for FP_ONE_DIV that gives a clear interface to convert a
   fixed point number to an int
   always rounds towards 0 */
#define FP_TO_INT(x) \
  (int) (FP_ONE_DIV(x))

/* Converts a fixed point number to an int by rounding to the nearest int */
#define FP_TO_NEAREST_INT(x) \
  (int) (x >= 0 ? x + (FIXED_POINT_ONE / 2) : x - (FIXED_POINT_ONE / 2))

/* Adds two fixed point numbers */
#define FP_ADD(x, y) \
  ((fixed_point_number) x + (fixed_point_number) y)

/* Subtracts a fixed point number from another fixed  point number */
#define FP_SUB(x, y) \
  ((fixed_point_number) x - (fixed_point_number) y)

/* Adds a fixed point number and an integer by converting the integer to a 
   fixed point number */
#define FP_ADD_INT(x, n) \
  FP_ADD(x, INT_TO_FP(n))

/* Subtracts an integer froma fixed point number by conberting the integer
   to a fixed point number */
#define FP_SUB_INT(x, n) \
  FP_SUB(x, INT_TO_FP(n))

/* Multiplies two fixed point numbers by using a 64 bit int to store the result
   and dividing by the value 1 to get back to 32 bits */
#define FP_MUL(x, y) \
  (fixed_point_number) \
  FP_ONE_DIV(((int64_t) x) * ((int64_t) y))

/* Multiplies a fixed point number by an int */
#define FP_MUL_INT(x, n) \
  (fixed_point_number) ((fixed_point_number) x * (int) n)

/* Divides a fixed point number by another fixed point number by multplying x
   by the fixed point value for 1 and then dividing by the second number */
#define FP_DIV(x, y) \
  (fixed_point_number) \
  (FP_ONE_MUL((int64_t) x) / (fixed_point_number) y)

/* Divides a fixed point number by an int */
#define FP_DIV_INT(x, n) \
  (fixed_point_number) ((fixed_point_number) x / (int) n)
#endif
