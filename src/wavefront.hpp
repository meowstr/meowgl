#pragma once

#include "res.hpp"

struct wavefront_t {
    const char * filename;

    float * pos_list;
    float * normal_list;
    float * uv_list;

    int vertex_count;

    int * obj_offset_list;
    const char ** obj_name_list;
    int obj_count;

    const char * material_lib_filename;

    int * material_group_offset_list;
    const char ** material_group_material_list;
    int material_group_count;

    void compute_bounds( float * min_vec3, float * max_vec3 );
};

struct material_t {
    float ns;            // specular exponent
    float ka[ 3 ];       // ambient
    float kd[ 3 ];       // diffuse
    float ks[ 3 ];       // specular
    float ke[ 3 ];       // emmision
    float ni;            // refraction index
    float d;             // dissolve
    int illum;           // illum model
    const char * map_kd; // diffuse texture map
};

struct material_lib_t {
    const char ** material_name_list;
    material_t * material_list;
    int material_count;
};

/// @threadsafe
int load_wavefront( wavefront_t * out_mesh, res_t res );

/// @threadsafe
int load_material_lib( material_lib_t * out_lib, res_t res );

