#pragma once

#include "wavefront.hpp"

#include <cglm/types.h>

struct transform_t {
    vec3 pos;
    vec3 rot;
    vec3 scale;
    mat4 m;

    void update();
    void identity();
};

struct entity_t {
    transform_t transform;
    int model;
    int parent;
};

struct camera_t {
    vec3 pos;
    float yaw;
    float pitch;
    float roll;
};

struct renderstate_t {
    entity_t * entity_list;
    int entity_count;

    wavefront_t * model_list;
    int * model_texture_list;
    vec3 * model_emission_list;
    int model_count;

    int hi_entity;

    camera_t camera;

    mat4 view_inverse;
    mat4 combined;
};

extern renderstate_t rstate;

void render_init();

void render();

void update_camera();

int add_entity( int model );
int add_model( const char * filename );

int add_entity( int model, float x, float y, float z );
