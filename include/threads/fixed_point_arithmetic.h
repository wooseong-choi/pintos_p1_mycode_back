#include <stdint.h>

typedef int32_t fixed_point;

#define FRACTIONAL_BITS 14
#define SCALE_FACTOR (1 << FRACTIONAL_BITS)

// convert int to fixed_point
fixed_point convert_fixed( int value );
// convert fixed_point to int
int convert_int( fixed_point value );
// convert fixed_point to int
int convert_int_near( fixed_point value );
// add fixed x and fixed y
fixed_point fixed_add( fixed_point x, fixed_point y );
// subtract fixed x and fixed y
fixed_point fixed_sub( fixed_point x, fixed_point y );
// add fixed x and int n
fixed_point fixed_add_int( fixed_point x, int n );
// subtract int n from fixed x
fixed_point fixed_sub_from_x( fixed_point x, int n );
// multiply fixed x by fixed y
fixed_point fixed_mul ( fixed_point x, fixed_point y );
// multiply fixed x by int x
fixed_point fixed_mul_by_int( fixed_point x, int n );
// divide fixed x by fixed y
fixed_point fixed_div ( fixed_point x, fixed_point y );
// divide fixed x by int n 
fixed_point fixed_div_by_int( fixed_point x, int n );