#pragma once

#include <cglm/types.h>
#include <string.h> // memcpy

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

    char ** avail_model_file_list;
    int avail_model_file_count;

    char ** model_file_list;
    int model_file_count;

    bool enable_pos_snapping;
    float pos_snapping_delta;

    bool enable_rot_snapping;
    float rot_snapping_delta;

    bool enable_pos_lock_x;
    bool enable_pos_lock_y;
    bool enable_pos_lock_z;

    float locked_x;
    float locked_y;
    float locked_z;

    int current_entity;
    int current_axis;
    int move_mode;
};

extern state_t state;
