#pragma once

using loop_function_t = void ( * )();

enum event_t {
    EVENT_TOUCH,
    EVENT_JUMP,
    EVENT_HAMMER_CW,
    EVENT_HAMMER_CCW,
    EVENT_FAST_HAMMER_CW,
    EVENT_FAST_HAMMER_CCW,
    EVENT_F1,
    EVENT_F2,
    EVENT_F3,
    EVENT_ROTATE_CW,
    EVENT_ROTATE_CCW,
    EVENT_SELECT_X,
    EVENT_SELECT_Y,
    EVENT_SELECT_Z,
};

int hardware_init();
void hardware_destroy();
void hardware_set_loop( loop_function_t step );

int * hardware_events( int * out_count );

int hardware_width();
int hardware_height();

float hardware_time();

float hardware_x_axis();
float hardware_y_axis();
float hardware_z_axis();

float hardware_look_yaw();
float hardware_look_pitch();

float hardware_touch_x();
float hardware_touch_y();

int hardware_touch_is_down();
