#include "threads/fixed_point_arithmetic.h"

// convert int to fixed_point
int convert_fixed( int value ){
    return value * SCALE_FACTOR;
}
// convert fixed_point to int
int convert_int( int value ){
    return value / SCALE_FACTOR;
}
// convert int to int
int convert_int_near( int value ){
    if ( value > 0 ) return (value + SCALE_FACTOR/2)/SCALE_FACTOR;
    else             return (value - SCALE_FACTOR/2)/SCALE_FACTOR;
}
// add fixed x and fixed y
int fixed_add( int x, int y ){
    return x + y;
}
// subtract fixed x and fixed y
int fixed_sub( int x, int y ){
    return x - y;    
}
// add fixed x and int n
int fixed_add_int( int x, int n ){
    return x + (n * SCALE_FACTOR);
}
// subtract int n from fixed x
int fixed_sub_from_x( int x, int n ){
    return x - (n * SCALE_FACTOR); 
}
// multiply fixed x by fixed y
int fixed_mul ( int x, int y ){
    return ((int64_t) x) * y / SCALE_FACTOR; 
}
// multiply fixed x by int x
int fixed_mul_by_int( int x, int n ){
    return x * n;  
}
// divide fixed x by fixed y
int fixed_div ( int x, int y ){
    return ((int64_t) x) * SCALE_FACTOR / y; 
}
// divide fixed x by int n 
int fixed_div_by_int( int x, int n ){
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
