#pragma once

#include "wavefront.hpp"

#include <cglm/types.h>

#define MEOWGL_MAX_MODEL_COUNT  32
#define MEOWGL_MAX_ENTITY_COUNT 1024

struct transform_t {
    vec3 pos;
    vec3 rot;
    vec3 scale;
    mat4 m;

    void update();
    void identity();
};

struct camera_t {
    vec3 pos;
    float yaw;
    float pitch;
    float roll;
};

struct renderstate_t {
    // clang-format off
    float *        vertex_pos_list;            // VERTEX TABLE
    float *        vertex_normal_list;
    float *        vertex_uv_list;
    int            vertex_count;
    int            vertex_cap;

    int *          model_size_list;            // MODEL TABLE
    int *          model_offset_list;          //
    int *          model_texture_list;         //
    vec3 *         model_emission_list;        //
    int            model_count;                //

    transform_t *  entity_transform_list;      // ENTITY TABLE
    int *          entity_model_list;          //
    int            entity_count;               //

    int *          e_model_entity_list;        // MODEL ENTITY TABLE
    int            e_model_count;              //

    int *          e_light_entity_list;        // LIGHT ENTITY TABLE
    int            e_light_count;              //

    int *          e_nocast_light_entity_list; // NON-CASTING LIGHT ENTITY TABLE
    int            e_nocast_light_count;       //
    // clang-format on

    int light_model; // model used for visualizing lights

    int hi_entity; // entity to highlight/outline

    camera_t camera; // for moving the camera

    mat4 view_inverse; // for grabbing things

    mat4 combined; // for raycasting to things

    float shadow_bias;
};

extern renderstate_t rstate;

void render_init();

void render();

void compute_all_shadow_maps();

void compute_camera_matrices();

void update_vertex_buffers();
