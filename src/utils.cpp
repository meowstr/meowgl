#include "utils.hpp"

int tick_timer( float * timer, float step )
{
    *timer -= step;
    if ( *timer < 0.0f ) *timer = 0.0f;

    return *timer > 0.0f;
}
