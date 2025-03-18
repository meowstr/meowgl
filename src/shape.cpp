#include "shape.hpp"

#include <math.h>

void rect_t::vertices( float * out_data ) const
{
    out_data[ 0 ] = x;
    out_data[ 1 ] = y;
    out_data[ 2 ] = 0.0f;

    out_data[ 3 ] = x + w;
    out_data[ 4 ] = y;
    out_data[ 5 ] = 0.0f;

    out_data[ 6 ] = x;
    out_data[ 7 ] = y + h;
    out_data[ 8 ] = 0.0f;

    out_data[ 9 ] = x + w;
    out_data[ 10 ] = y + h;
    out_data[ 11 ] = 0.0f;

    out_data[ 12 ] = x + w;
    out_data[ 13 ] = y;
    out_data[ 14 ] = 0.0f;

    out_data[ 15 ] = x;
    out_data[ 16 ] = y + h;
    out_data[ 17 ] = 0.0f;
}

void rect_t::vertices_2d( float * out_data ) const
{
    out_data[ 0 ] = x;
    out_data[ 1 ] = y;

    out_data[ 2 ] = x + w;
    out_data[ 3 ] = y;

    out_data[ 4 ] = x;
    out_data[ 5 ] = y + h;

    out_data[ 6 ] = x + w;
    out_data[ 7 ] = y + h;

    out_data[ 8 ] = x + w;
    out_data[ 9 ] = y;

    out_data[ 10 ] = x;
    out_data[ 11 ] = y + h;
}

void rect_t::margin( float amount )
{
    x += amount;
    w -= amount * 2;
    y += amount;
    h -= amount * 2;

    if ( w < 0 ) w = 0;
    if ( h < 0 ) h = 0;
}

float rect_t::center_x()
{
    return x + w * 0.5f;
}

float rect_t::center_y()
{
    return y + h * 0.5f;
}

int rect_t::contains( float ox, float oy ) const
{
    return ox >= x && ox <= x + w && oy >= y && oy <= y + h;
}

void ngon_vertices( float * out_data, int n )
{
    out_data[ 0 ] = 0.0f;
    out_data[ 1 ] = 0.0f;
    for ( int i = 0; i < n + 1; i++ ) {
        float theta = ( (float) ( i % n ) / n ) * 3.14159 * 2.0f;
        out_data[ ( i + 1 ) * 2 + 0 ] = cos( theta );
        out_data[ ( i + 1 ) * 2 + 1 ] = sin( theta );
    }
}

static int intersect_segments( vec2 out, vec2 a, vec2 b, vec2 c, vec2 d )
{
    return 0;
}

int intersect_line_and_rect( vec2 out1, vec2 out2, rect_t r, vec2 p1, vec2 p2 )
{
    return 0;
}
