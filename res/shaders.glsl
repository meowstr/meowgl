////////////////////////////////////////////////////////////////////////////////
#shader vertex_mesh
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
varying vec2 v_uv;

uniform mat4 u_sun_combined;
varying vec3 v_pos_sun_coords;

void main()
{
    vec4 pos = vec4( a_pos, 1.0 );
    vec4 world_pos = u_model * pos;
    v_normal = (u_model * vec4(a_normal, 0.0)).xyz;
    v_pos_sun_coords = (u_sun_combined * world_pos).xyz;
    v_uv = a_uv;
    gl_Position = u_proj * u_view * world_pos;
}

////////////////////////////////////////////////////////////////////////////////
#shader fragment_material
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;
uniform vec4 u_color;
varying vec3 v_normal;
varying vec2 v_uv;

uniform sampler2D u_depth_texture;
uniform sampler2D u_material_texture;
varying vec3 v_pos_sun_coords;

float sample_depth()
{
    vec2 pos_tex_coords = v_pos_sun_coords.xy * 0.5 + 0.5;
    float depth = v_pos_sun_coords.z * 0.5 + 0.5;

    float bias = 0.00005;

    float tex_depth = bias + texture2D( u_depth_texture, pos_tex_coords ).r;
    return depth < tex_depth ? 1.0 : 0.0;
}

void main()
{
    float light = 0.5 * dot( -normalize(v_normal), normalize(vec3( -1.0, -1.0, -1.0 )) );
    vec4 texture_color = texture2D( u_material_texture, v_uv );
    gl_FragColor = sample_depth() * light * ( u_color * texture_color );
    gl_FragColor.a = 1.0;
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
#shader fragment_postprocess
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;
uniform sampler2D u_texture;
uniform vec2 u_player_pos;
uniform vec2 u_player_dir;
varying vec2 v_uv;
void main()
{
    vec2 pos = vec2( gl_FragCoord.x, gl_FragCoord.y );
    vec2 vv = pos - u_player_pos;
    float dist = length( vv );
    vec2 v = vv / dist;

    float t = dot( v, u_player_dir );

    vec4 gray = vec4( 0.06, 0.06, 0.06, 1.0 );

    if ( t > 0.85 || dist < 50.0 ) {
        gl_FragColor = texture2D( u_texture, v_uv );
    } else if ( dist < 55.0 || t > 0.8 ) {
        float t1 = ( 55.0 - dist ) / 5.0;
        float t2 = ( t - 0.8 ) / 0.05;
        gl_FragColor = mix( gray, texture2D( u_texture, v_uv ), max( t1, t2 ) );
    } else {
        gl_FragColor = gray;
    }

    // gl_FragColor =
    //     mix( vec4( 0.10, 0.10, 0.10, 1.0 ),
    //          texture2D( u_texture, v_uv ),
    //          dot( vv, u_player_dir ) );
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
