#include "my_utility.h"

#include <deque>
#include <string>

//                           +------------------+                            //
// --------------------------+ GLOBAL VARIABLES +--------------------------- //
//                           +------------------+                            //

std::ofstream debug_output, test_output;     // file pointers for debugging and testing

//                              +------------+                               //
// -----------------------------+ FUNCTIONS  +------------------------------ //
//                              +------------+                               //

void InitializeTracing() {
    if (MY_TRACE)
        debug_output.open( FILE_NAME_DEBUG );
}

void FinalizeTracing() {
    if (MY_TRACE)
        debug_output.close();
}

void myPanic( std::string functionName, std::string errorMsg, int exitVal ) {
    if (MY_TRACE) {
        debug_output << "CRITICAL ERROR in function " << functionName << " --> " << errorMsg << std::endl;
        debug_output << "                           " <<                 "     " << "exiting with status: " << exitVal << std::endl;
    }
    exit( exitVal );
}

void myTrace( std::string functionName, std::string msg ) {
    if (MY_TRACE) debug_output << "function: " << functionName << " --> " << msg << std::endl;
}

// Index, range and pointer checking functions (guards)

// index must be in interval [ minVal, maxVal >
// Note that maxval is excluded (!) as valid value for index.
void CheckIndex( std::string fName, int index, int minVal, int maxVal ) {
    if (index < minVal)
        std::cout << "ERROR: " << fName << " --> index out of range: "    << std::to_string( index  )
                                        << ", should be between (min.): " << std::to_string( minVal )
                                        << " and (max.): "                << std::to_string( maxVal ) << std::endl;
    if (index >= maxVal)
        std::cout << "ERROR: " << fName << " --> index out of range: "    << std::to_string( index  )
                                        << ", should be between (min.): " << std::to_string( minVal )
                                        << " and (max.): "                << std::to_string( maxVal ) << std::endl;
}

// val must be in interval [ minVal, maxVal ]
// Note that maxval is included as valid value for val.
void CheckRange( std::string fName, int val, int minVal, int maxVal ) {
    if (val < minVal)
        std::cout << "ERROR: " << fName << " --> value out of range: "    << std::to_string( val    )
                                        << ", should be between (min.): " << std::to_string( minVal )
                                        << " and (max.): "                << std::to_string( maxVal ) << std::endl;
    if (val > maxVal)
        std::cout << "ERROR: " << fName << " --> value out of range: "    << std::to_string( val    )
                                        << ", should be between (min.): " << std::to_string( minVal )
                                        << " and (max.): "                << std::to_string( maxVal ) << std::endl;
}

void CheckRange( std::string fName, float val, float minVal, float maxVal ) {
    if (val < minVal)
        std::cout << "ERROR: " << fName << " --> value out of range: "    << std::to_string( val    )
                                        << ", should be between (min.): " << std::to_string( minVal )
                                        << " and (max.): "                << std::to_string( maxVal ) << std::endl;
    if (val > maxVal)
        std::cout << "ERROR: " << fName << " --> value out of range: "    << std::to_string( val    )
                                        << ", should be between (min.): " << std::to_string( minVal )
                                        << " and (max.): "                << std::to_string( maxVal ) << std::endl;
}

void CheckPointer( std::string fName, void *ptr ) {
    if (ptr == nullptr)
        std::cout << "ERROR: " << fName << " --> nullptr passed as input argument" << std::endl;
}

// String alignment functions

std::string StringAlignedR( std::string strArg, int nrChars ) {
    std::string s;
    int len = strArg.length();
    if (len >= nrChars)
        s.append( strArg );
    else {
        for (int i = 0; i < nrChars - len; i++)
            s.append( " " );
        s.append( strArg );
    }
    return s;
}

std::string StringAlignedL( std::string strArg, int nrChars ) {
    std::string s;
    int len = strArg.length();
    if (len >= nrChars)
        s.append( strArg );
    else {
        s.append( strArg );
        for (int i = 0; i < nrChars - len; i++)
            s.append( " " );
    }
    return s;
}

std::string StringAlignedC( std::string strArg, int nrChars ) {
    std::string s;
    int len = strArg.length();
    if (len >= nrChars)
        s.append( strArg );
    else {
        int lSpaces =           (nrChars - len) / 2;
        int rSpaces = lSpaces + (nrChars - len) % 2;   // if odd number of spaces: append an extra space
        for (int i = 0; i < lSpaces; i++)
            s.append( " " );
        s.append( strArg );
        for (int i = 0; i < rSpaces; i++)
            s.append( " " );
    }
    return s;
}

// overloaded versions for int and float arguments
std::string StringAlignedR( int   nArg, int nrChars ) { return StringAlignedR( std::to_string( nArg ), nrChars ); }
std::string StringAlignedR( float fArg, int nrChars ) { return StringAlignedR( std::to_string( fArg ), nrChars ); }

std::string StringAlignedL( int   nArg, int nrChars ) { return StringAlignedL( std::to_string( nArg ), nrChars ); }
std::string StringAlignedL( float fArg, int nrChars ) { return StringAlignedL( std::to_string( fArg ), nrChars ); }

std::string StringAlignedC( int   nArg, int nrChars ) { return StringAlignedC( std::to_string( nArg ), nrChars ); }
std::string StringAlignedC( float fArg, int nrChars ) { return StringAlignedC( std::to_string( fArg ), nrChars ); }

std::string PrintBoolToString( bool var ) {
    std::string s;
    if (var)
        s.append("TRUE");
    else
        s.append("FALSE");
    return s;
}

// ==============================/   utility functions /========================================

// returns an integer random number in the range [ minValue, maxValue ]
int RandomNumberBetween( int minValue, int maxValue ) {
    return (rand() % (maxValue + 1 - minValue)) + minValue;
}

// returns a floating point random number in the range [ minValue, maxValue ]
float RandomNumberBetween( float minValue, float maxValue ) {
    return ((float)rand() / (float)RAND_MAX) * (maxValue - minValue) + minValue;
}

// returns true if a change of x to y occurs. Example: Chance_x2y( 1, 10 ) returns
// a chance distribution corresponding to 0.1f.
bool Chance_x2y( int x, int y ) {
    return (rand() % y) < x;
}

// utility function to translate a radian angle into degrees
// the result is modulated to be in [0, 2 * PI )
float radiansToDegrees( float angleInRadians ) {
    float result = angleInRadians * (180.0f / PI);
    while (result <    0.0f) result += 360.0f;
    while (result >= 360.0f) result -= 360.0f;
    return result;
}

// utility function to translate a degree angle into radians
// the result is modulated to be in [0, 360)
float degreesToRadians( float angleInDegrees ) {
    float result = angleInDegrees * (PI / 180.0f);
    while (result <  0.0f     ) result += 2.0f * PI;
    while (result >= 2.0f * PI) result -= 2.0f * PI;
    return result;
}

// ========== ADDED FROM SGE_utilities... ==============================

char to_hex_position( uint8_t val ) {
    char result = '?';
    if (val >= 16) {
        std::cout << "ERROR: to_hex_position() --> argument > F: " << val << std::endl;
    } else {
        result = (val < 10) ? '0' + val : 'A' + (val - 10);
    }
    return result;
}

std::string to_hex( uint32_t val ) {
    // Break the 32 bit int up into 8 groups of 4 bits and push these to the front of the
    // auxiliary queue. Least significant will be at the back, most significant at the front.
    std::deque<char> hexQueue;
    uint32_t input = val;
    std::string output;
    for (int i = 0; i < 8; i++) {
        uint8_t byteVal = (input % 16);
        hexQueue.push_front( byteVal );
        input = input / 16;
    }
    // Build the result string by walking the auxiliary queue front to back and convering the
    // 4 bit values into their char hexadecmial representations.
    output.append( "0x" );
    for (auto e : hexQueue) {
        output.append( std::string( 1, to_hex_position( e )));
    }
    return output;
}

// returns random number in [0.0f, 1.0f]
float RandChance() {
    return float( rand() ) / float( RAND_MAX );
}

// returns true if chance of n out of m occurs. Note that n <= m!!
bool ChanceInt( int n, int m ) {
    return (rand() % m) < n;
}

// returns true if chance of fNormdPerc occurs. The argument must be in [0.0f, 1.0f]
bool ChanceFloat( float fNormdPerc ) {
    return (RandChance() < fNormdPerc);
}

// return a random integer value in the range [a, b]
int RandIntBetween( int a, int b ) {
    return (rand() % (b - a + 1)) + a;
}

// return a random float value in the range [a, b]
float RandFloatBetween( float a, float b ) {
    return RandChance() * (b - a) + a;
}

// returns a clamp of the first parameter between the second and third
uint8_t Clamp( uint8_t a, uint8_t a_start, uint8_t a_end ) {
    uint8_t retVal = a;
    if (retVal < a_start) retVal = a_start;
    if (retVal > a_end  ) retVal = a_end;
    return retVal;
}

// returns a clamp of the first parameter between the second and third
int Clamp( int a, int a_start, int a_end ) {
    int retVal = a;
    if (retVal < a_start) retVal = a_start;
    if (retVal > a_end  ) retVal = a_end;
    return retVal;
}

float Clamp( float a, float a_start, float a_end ) {
    float retVal = a;
    if (retVal < a_start) retVal = a_start;
    if (retVal > a_end  ) retVal = a_end;
    return retVal;
}

// function produces a right aligned string of length positions containing value
std::string right_align( int value, int positions ) {
    std::string aux = std::to_string( value );
    return right_align( aux, positions );
}

std::string right_align( std::string &s, int positions ) {
    std::string result;

    for (int i = 0; i < (positions - (int)s.length()); i++)
        result.append( " " );
    result.append( s );

    return result;
}

// function produces a left aligned string of length positions containing value
std::string left_align( int value, int positions ) {
    std::string aux = std::to_string( value );
    return left_align( aux, positions );
}

std::string left_align( std::string &s, int positions ) {
    std::string result;

    result.append( s );
    for (int i = 0; i < (positions - (int)s.length()); i++)
        result.append( " " );

    return result;
}

// float f resp. string s contains a floating point number which may (or may not) have a dot in it.
// align the dot of this number on dotPosition, creating a string of totalPositions
std::string dot_align( float f, int dotPosition, int totalPositions ) {
    std::string aux = std::to_string( f );
    return dot_align( aux, dotPosition, totalPositions );
}

std::string dot_align( std::string &s, int dotPosition, int totalPositions ) {
    std::string result;
    size_t dotIndex = s.find( "." );
    std::string sBeforeDot, sAfterDot, sDot;
    if (dotIndex == std::string::npos) {     // string contained no '.'
        sBeforeDot = s;
        sDot = "";
        sAfterDot = "";
    } else {
        sBeforeDot = s.substr( 0, dotIndex );
        sDot = ".";
        sAfterDot  = s.substr( dotIndex + 1 );
    }
    result.append( right_align( sBeforeDot, dotPosition - 1 ));
    result.append( sDot );
    result.append( left_align( sAfterDot, totalPositions - dotPosition ));
    return result;
}

//                                                                           //
// ------------------------------------------------------------------------- //
//
