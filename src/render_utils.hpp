#pragma once

#include "res.hpp"

#include <cglm/types.h>

struct vbuffer_t {
    int buffer;
    int element_count;
    int element_size;

    void init( int new_element_size );
    void set( const float * new_data, int new_element_count );
    void enable( int attrib_index );
};

struct framebuffer_t {
    int id;
    int width;
    int height;

    void init( int width, int height );
    int init_color_texture( int attachment_index );
    int init_hdr_texture( int attachment_index );
    int init_depth_texture();
};

int load_texture( res_t res );

const char * find_shader_string( const char * name );

int build_shader( const char * vertex_string, const char * fragment_string );

int find_uniform( int shader, const char * uniform_name );

void set_uniform( int uniform, int v );
void set_uniform( int uniform, float v );
void set_uniform( int uniform, float ( &v )[ 2 ] );
void set_uniform( int uniform, float ( &v )[ 3 ] );
void set_uniform( int uniform, float ( &v )[ 4 ] );
void set_uniform( int uniform, vec4 ( &m )[ 4 ] );
