// #include "threads/fixed_point_arithmetic.h"
#include "../include/threads/fixed_point_arithmetic.h"
// #include <debug.h>
// #include <stddef.h>
// #include <stdio.h>
// #include <string.h>

// convert int to fixed_point
fixed_point convert_fixed( int value ){
    return value * SCALE_FACTOR;
}
// convert fixed_point to int
int convert_int( fixed_point value ){
    return value / SCALE_FACTOR;
}
// convert fixed_point to int
int convert_int_near( fixed_point value ){
    if ( value > 0 ) return (value + SCALE_FACTOR/2)/SCALE_FACTOR;
    else return (value - SCALE_FACTOR/2)/SCALE_FACTOR;
}
// add fixed x and fixed y
fixed_point fixed_add( fixed_point x, fixed_point y ){
    return x + y;
}
// subtract fixed x and fixed y
fixed_point fixed_sub( fixed_point x, fixed_point y ){
    return x - y;    
}
// add fixed x and int n
fixed_point fixed_add_int( fixed_point x, int n ){
    return x + (n * SCALE_FACTOR);
}
// subtract int n from fixed x
fixed_point fixed_sub_from_x( fixed_point x, int n ){
    return x - (n * SCALE_FACTOR); 
}
// multiply fixed x by fixed y
fixed_point fixed_mul ( fixed_point x, fixed_point y ){
    return ((int64_t) x) * y / SCALE_FACTOR; 
}
// multiply fixed x by int x
fixed_point fixed_mul_by_int( fixed_point x, int n ){
    return x * n;  
}
// divide fixed x by fixed y
fixed_point fixed_div ( fixed_point x, fixed_point y ){
    return ((int64_t) x) * SCALE_FACTOR / y; 
}
// divide fixed x by int n 
fixed_point fixed_div_by_int( fixed_point x, int n ){
    return x / n;
}

// int main(int argc, char const *argv[])
// {
//     fixed_point a = convert_fixed(1.5);
//     fixed_point b = convert_fixed(2.5);

//     fixed_point sum = fixed_add(a, b);
//     fixed_point difference = fixed_sub(a, b);
//     fixed_point product = fixed_mul(a, b);
//     fixed_point quotient = fixed_div(a, b);

//     printf("Sum: %d\n", convert_int(sum));
//     printf("Difference: %d\n", convert_int(difference));
//     printf("Product: %d\n", convert_int(product));
//     printf("Quotient: %d\n", convert_int(quotient));
//     return 0;
// }
