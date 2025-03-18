#pragma once

#include <cglm/types.h>

template < typename T > void array_swap_last( T * arr, int count, int index )
{
    memcpy( arr + index, arr + count - 1, sizeof( T ) );
}


struct state_t {
    float tick_time;
    float render_time;
    float tick_step;
    float render_step;
    int tick;

    const char ** model_file_list;
    int model_file_count;

    bool enable_pos_snapping;
    float pos_snapping_delta;

    bool enable_rot_snapping;
    float rot_snapping_delta;

    int current_entity;
    int current_axis;
};

extern state_t state;
