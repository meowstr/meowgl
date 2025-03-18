#pragma once

#include <cglm/types.h>

struct rect_t {
    float x;
    float y;
    float w;
    float h;

    /// 6 vec3 vertices
    /// out_data should be at least 18 floats long
    void vertices( float * out_data ) const;

    /// 6 vec2 vertices
    /// out_data should be at least 12 floats long
    void vertices_2d( float * out_data ) const;

    void margin( float amount );

    float center_x();
    float center_y();

    rect_t & centerize()
    {
        x -= w * 0.5f;
        y -= h * 0.5f;
        return *this;
    }

    int contains( float x, float y ) const;
};

int intersect_line_and_rect( vec2 out1, vec2 out2, rect_t r, vec2 p1, vec2 p2 );

/// out_data :: float[n * 2]
void ngon_vertices( float * out_data, int n );
