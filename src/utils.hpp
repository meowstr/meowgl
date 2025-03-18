#pragma once

int tick_timer( float * timer, float step );

inline int grid( int w, int h, int x, int y )
{
    if ( x < 0 ) return -1;
    if ( y < 0 ) return -1;
    if ( x >= w ) return -1;
    if ( y >= h ) return -1;
    return y * w + x;
}
