////////////////////////////////////////////////////////////////////////////////
#shader vertex_deferred
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;
attribute vec3 a_pos;
attribute vec3 a_normal;
attribute vec2 a_uv;

uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;

varying vec3 v_normal; // in world coords
varying vec3 v_position; // in world coords
varying vec2 v_uv;

void main()
{
    vec4 pos = vec4( a_pos, 1.0 );
    vec4 world_pos = u_model * pos;

    v_normal = ( u_model * vec4( a_normal, 0.0 ) ).xyz;
    v_uv = a_uv;
    v_position = world_pos.xyz;
    gl_Position = u_proj * u_view * world_pos;
}

////////////////////////////////////////////////////////////////////////////////
#shader fragment_deferred
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;
uniform vec4 u_color;
varying vec3 v_normal; // in world coords
varying vec3 v_position; // in world coords
varying vec2 v_uv;

uniform sampler2D u_material_texture;
uniform float u_material_mix;
uniform vec3 u_emission;

#define O_COLOR    0
#define O_POSITION 1
#define O_NORMAL   2
#define O_EMISSION 3

// float light_from_scene()
// {
//     vec2 pos_tex_coords = v_pos_sun_coords.xy * 0.5 + 0.5;
//     float depth = v_pos_sun_coords.z * 0.5 + 0.5;
// 
//     float bias = 0.00005;
// 
//     float tex_depth = bias + texture2D( u_depth_texture, pos_tex_coords ).r;
//     //return depth < tex_depth ? 20.0 : 2.0;
//     return 5.0;
// }

void main()
{
    vec4 texture_color = texture2D( u_material_texture, v_uv );
    texture_color = mix( texture_color, vec4(1.0, 1.0, 1.0, 1.0), u_material_mix );

    gl_FragData[ O_COLOR ] =  u_color * texture_color;
    gl_FragData[ O_COLOR ].a = 1.0;
    gl_FragData[ O_POSITION ] = vec4( v_position, 1.0 );
    gl_FragData[ O_NORMAL ] = vec4( v_normal, 1.0 );
    gl_FragData[ O_EMISSION ] = vec4( u_emission, 1.0 );
}

////////////////////////////////////////////////////////////////////////////////
#shader vertex_shadow
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;
attribute vec3 a_pos;
attribute vec3 a_normal;
attribute vec2 a_uv;

uniform mat4 u_combined;
uniform mat4 u_model;

void main()
{
    gl_Position = u_combined * u_model * vec4( a_pos, 1.0 );
}

////////////////////////////////////////////////////////////////////////////////
#shader fragment_shadow
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;

void main()
{
    // output depth info
}

////////////////////////////////////////////////////////////////////////////////
#shader vertex_mesh_highlight
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;
attribute vec3 a_pos;
attribute vec3 a_normal;
attribute vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;

void main()
{
    gl_Position = u_proj * u_view * u_model * vec4( a_pos, 1.0 );
}

////////////////////////////////////////////////////////////////////////////////
#shader fragment_highlight
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;

void main()
{
    gl_FragColor = vec4( 1.0, 1.0, 1.0, 1.0 );
}

////////////////////////////////////////////////////////////////////////////////
#shader vertex_screen
////////////////////////////////////////////////////////////////////////////////

#version 100
attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main()
{
    v_uv = a_uv;
    gl_Position = vec4( a_pos, 0.0, 1.0 );
}

////////////////////////////////////////////////////////////////////////////////
#shader fragment_light
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;

uniform sampler2D u_position_texture;
uniform sampler2D u_depth_texture;
uniform vec4 u_depth_tile;
uniform mat4 u_light_matrix;
uniform vec3 u_light_pos;

varying vec2 v_uv;

bool do_shadow_test( vec3 pos )
{
    // transform
    vec4 pos_light_space = u_light_matrix * vec4( pos, 1.0 );
    // perspective divide
    pos_light_space = pos_light_space / pos_light_space.w;
    // convert to texture coords for sampling
    vec2 tex_coords = pos_light_space.xy * 0.5 + 0.5;
    // convert to tile
    vec2 tile_coords = u_depth_tile.xy + ( tex_coords * u_depth_tile.zw );
    vec2 tile_tex_coords = tile_coords / vec2( 8192, 8192 );
    // compute depth in light space
    float depth = pos_light_space.z * 0.5 + 0.5;
    // sample
    float bias = 0.00005;
    float sample_depth = bias + texture2D( u_depth_texture, tile_tex_coords ).r;
    // make sure its in our bounds and yea
    return tex_coords.x >= 0.0 && tex_coords.x <= 1.0 && 
           tex_coords.y >= 0.0 && tex_coords.y <= 1.0 && 
           depth >= 0.0 && depth <= 1.0 &&
           depth <= sample_depth;
}

void main()
{
    vec3 pos = texture2D( u_position_texture, v_uv ).xyz;

    if ( distance( pos, u_light_pos ) < 4.0 && do_shadow_test( pos ) ) {
        // lit
        gl_FragColor = vec4( 1.0, 1.0, 1.0, 1.0 );
    } else {
        // not lit
        gl_FragColor = vec4( 0.0, 0.0, 0.0, 0.0 );
    }
}

////////////////////////////////////////////////////////////////////////////////
#shader fragment_scene_compose
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;

uniform sampler2D u_color_texture;
uniform sampler2D u_light_mask_texture;
uniform sampler2D u_bloom_texture;

uniform vec2 u_size;
varying vec2 v_uv;
void main()
{
    //float exposure = 0.1;

    float light = texture2D(u_light_mask_texture, v_uv).r;
    light = clamp(light, 0.1, 0.8); // ambient
    vec3 color = texture2D( u_color_texture, v_uv ).rgb;

    gl_FragColor = vec4(light * color, 1.0);

    //vec3 emission = vec3(0.0, 0.0, 0.0);
    //int n_theta = 50;
    //int n_radius = 30;
    //for (int i = 0; i < n_theta; i++) {
    //    for (int j = 0; j < n_radius; j++) {
    //        vec2 uv = v_uv;
    //        float theta = float(i) / float(n_theta) * 6.28318530718;
    //        uv.x += float(j) * cos(theta) * (1.0 / u_size.x);
    //        uv.y += float(j) * sin(theta) * (1.0 / u_size.y);
    //        emission += (1.0 / (float(n_theta) * float(n_radius))) * texture2D( u_bloom_texture, uv ).rgb;
    //    }
    //}

    //vec3 hdr = scene + 50.0 * emission;

    //vec3 mapped = vec3(1.0) - exp(-hdr * exposure);

    ////mapped = pow(mapped, vec3(1.0 / 2.2));

    //gl_FragColor = vec4(mapped, 1.0);
    ////gl_FragColor = vec4(color, 1.0);
}

////////////////////////////////////////////////////////////////////////////////
#shader fragment_highlight_post
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;
uniform sampler2D u_texture;
uniform vec2 u_size;
varying vec2 v_uv;
void main()
{
    int kernel_size = 5;

    // 1  1  1
    // 1 -9  1
    // 1  1  1


    float total_alpha = 0.0;

    for (int i = 0; i < kernel_size; i++) {
        for (int j = 0; j < kernel_size; j++) {
            int dx = i - kernel_size / 2;
            int dy = j - kernel_size / 2;

            float x = v_uv.x + ( float( dx ) / u_size.x );
            float y = v_uv.y + ( float( dy ) / u_size.y );

            total_alpha += texture2D( u_texture, vec2( x, y ) ).a;
        }
    }

    float center_sample = texture2D( u_texture, v_uv ).a;

    if (total_alpha > 0.0 && center_sample == 0.0) {
        gl_FragColor = vec4( 1.0, 1.0, 0.0, 1.0 );
    } else {
        gl_FragColor = vec4( 0.0, 0.0, 0.0, 0.0 );
    }

}

////////////////////////////////////////////////////////////////////////////////
#shader shader2_vertex
////////////////////////////////////////////////////////////////////////////////

#version 100
attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main()
{
    v_uv = a_uv;
    gl_Position = vec4( a_pos.x, a_pos.y, 0.0, 1.0 );
}

////////////////////////////////////////////////////////////////////////////////
#shader shader2_fragment
////////////////////////////////////////////////////////////////////////////////

#version 100
precision lowp float;
uniform sampler2D u_texture;
uniform float u_amount;
varying vec2 v_uv;
void main()
{
    vec2 d = vec2( 0.003, 0.003 ) * u_amount;
    vec3 c;
    c.r = texture2D( u_texture, v_uv + d ).r;
    c.g = texture2D( u_texture, v_uv ).g;
    c.b = texture2D( u_texture, v_uv - d ).b;
    gl_FragColor = vec4( c, 1.0 );
}

////////////////////////////////////////////////////////////////////////////////
#shader shader3_vertex
////////////////////////////////////////////////////////////////////////////////

#version 100
attribute vec2 a_pos;
attribute vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_model;
varying vec2 v_uv;
void main()
{
    v_uv = a_uv;
    vec4 pos = vec4( a_pos, 0.0, 1.0 );
    gl_Position = u_proj * u_model * pos;
}

////////////////////////////////////////////////////////////////////////////////
#shader shader3_fragment
////////////////////////////////////////////////////////////////////////////////

#version 100
precision lowp float;
uniform sampler2D u_texture;
uniform vec4 u_color;
varying vec2 v_uv;
void main()
{
    gl_FragColor = u_color * texture2D( u_texture, v_uv );
}
