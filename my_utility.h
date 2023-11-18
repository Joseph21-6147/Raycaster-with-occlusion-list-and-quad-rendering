#ifndef MY_UTILITY_H
#define MY_UTILITY_H

#include <iostream>
#include  <fstream>

//                          +--------------------+                           //
// -------------------------+ MODULE DESCRIPTION +-------------------------- //
//                          +--------------------+                           //

//                               +-----------+                               //
// ------------------------------+ CONSTANTS +------------------------------ //
//                               +-----------+                               //

#define PI  3.1415926535f

#define ALIGN_STRLEN     14  // default length for string alignment

#define FILE_NAME_TEST     "test_output.txt"
#define FILE_NAME_DEBUG   "debug_output.txt"

// levels of debug output. DEBUG_FLAG true = minimal debug output, VERBOSE_FLAG true = additional detailed debug output
#define DEBUG_FLAG    true
#define VERBOSE_FLAG  true
#define DEBUG( x )    if (  DEBUG_FLAG) std::cout << x
#define VERBOSE( x )  if (VERBOSE_FLAG) std::cout << x

#define MY_TRACE      true  // set to false before creating a new release

//                              +------------+                               //
// -----------------------------+ PROTOTYPES +------------------------------ //
//                              +------------+                               //

// opens and close file for trace output
void InitializeTracing();
void FinalizeTracing();

// prints an error message to debug_output
// NOTE: the message is only displayed if MY_TRACE == true!
void myTrace( std::string functionName, std::string msg );
// prints an error message to debug_output and exits with status exitVal
// NOTE: the message is only displayed if MY_TRACE == true!
void myPanic( std::string functionName, std::string errorMsg, int exitVal );

// ========== Argument checking ==========

// index must be in interval [ minVal, maxVal >
// Note that maxval is excluded (!) as valid value for index.
// sFuncName is the name of the function where the guard is built into.
void   CheckIndex( std::string sFuncName, int index, int minVal, int maxVal );

// val must be in interval [ minVal, maxVal ]
// Note that maxval is included as valid value for val.
// sFuncName is the name of the function where the guard is built into.
void   CheckRange( std::string sFuncName, int   val, int   minVal, int   maxVal );
void   CheckRange( std::string sFuncName, float val, float minVal, float maxVal );

// Checks on the pointer not to be nullptr
// sFuncName is the name of the function where the guard is built into.
void CheckPointer( std::string sFuncName, void *ptr );

// ========== Aligned printing ==========

// aligns the argument right in a string of length nrChars
std::string StringAlignedR( std::string sArg, int nrChars = ALIGN_STRLEN );
std::string StringAlignedR( int         nArg, int nrChars = ALIGN_STRLEN );
std::string StringAlignedR( float       fArg, int nrChars = ALIGN_STRLEN );

// aligns the argument left in a string of length nrChars
std::string StringAlignedL( std::string sArg, int nrChars = ALIGN_STRLEN );
std::string StringAlignedL( int         nArg, int nrChars = ALIGN_STRLEN );
std::string StringAlignedL( float       fArg, int nrChars = ALIGN_STRLEN );

// centers the argument in a string of length nrChars
std::string StringAlignedC( std::string sArg, int nrChars = ALIGN_STRLEN );
std::string StringAlignedC( int         nArg, int nrChars = ALIGN_STRLEN );
std::string StringAlignedC( float       fArg, int nrChars = ALIGN_STRLEN );

std::string PrintBoolToString( bool var );

// ==============================/   utility functions /========================================

// returns an integer random number in the range [ minValue, maxValue ]
int RandomNumberBetween( int minValue, int maxValue );
// returns a floating point random number in the range [ minValue, maxValue ]
// with four digit significance
float RandomNumberBetween( float minValue, float maxValue );
// returns true if a change of x to y occurs. Example: Chance_x2y( 1, 10 ) gives 0.1f chance.
bool Chance_x2y( int x, int y );
// utility function to translate a radian angle into degrees
// the result is modulated to be in [0, 2 * PI )
float radiansToDegrees( float angleInRadians );
// utility function to translate a degree angle into radians
// the result is modulated to be in [0, 360)
float degreesToRadians( float angleInDegrees );

// ========== ADDED FROM SGE_utilities... ==============================

// returns random float in [0.0f, 1.0f]
float RandChance();
// returns true if chance of n out of m occurs. Note that n <= m!!
bool ChanceInt( int n, int m );
// returns true if chance of fNormdPerc occurs. The argument must be in [0.0f, 1.0f]
bool ChanceFloat( float fNormdPerc );

// returns a random integer n in [ a, b ]
int   RandIntBetween(   int   a, int   b );
// returns a random float f in [ a, b ]
float RandFloatBetween( float a, float b );

// translate val into it's corresponding hexadecimal representation.
// e.g. 0 translates to '0' and 15 translates to 'F'.
// val must be in [0, 15]
char        to_hex_position( uint8_t val );
// translates val into its corresponding hexadecimal representation.
// since val is 32 bits, it translates into a string of length 8.
std::string to_hex( uint32_t val );

// functions produce an aligned string of length positions containing nValue
std::string right_align( int nValue, int positions );
std::string  left_align( int nValue, int positions );
std::string right_align( std::string &s, int positions );
std::string  left_align( std::string &s, int positions );
// fValue resp. s contains a floating point number which may (or may not) have a
// dot in it. Align the dot of this number on dotPosition, creating a string of
// totalPositions
std::string dot_align( float fValue  , int dotPosition, int totalPositions );
std::string dot_align( std::string &s, int dotPosition, int totalPositions );

// return the clamped value of a in [ a_start, a_end ]
int   Clamp( int   a, int   a_start, int   a_end );
float Clamp( float a, float a_start, float a_end );

//                           +------------------+                            //
// --------------------------+ GLOBAL VARIABLES +--------------------------- //
//                           +------------------+                            //

extern std::ofstream debug_output, test_output;   // file pointers for testing, debugging

//                                                                           //
// ------------------------------------------------------------------------- //
//                                                                           //

#endif // MY_UTILITY_H
