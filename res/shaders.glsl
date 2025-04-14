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

uniform mat4 u_shadow0_combined;
uniform mat4 u_shadow1_combined;
uniform mat4 u_shadow2_combined;
uniform mat4 u_shadow3_combined;

varying vec3 v_shadow0_pos;
varying vec3 v_shadow1_pos;
varying vec3 v_shadow2_pos;
varying vec3 v_shadow3_pos;

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

uniform sampler2D u_material_texture;
uniform float u_material_mix;
uniform vec3 u_emission;

uniform sampler2D u_depth_texture;
varying vec3 v_pos_sun_coords;

uniform sampler2D u_shadow0_depth;
uniform sampler2D u_shadow1_depth;
uniform sampler2D u_shadow2_depth;
uniform sampler2D u_shadow3_depth;

varying vec3 v_shadow0_pos;
varying vec3 v_shadow1_pos;
varying vec3 v_shadow2_pos;
varying vec3 v_shadow3_pos;

float light_from_scene()
{
    vec2 pos_tex_coords = v_pos_sun_coords.xy * 0.5 + 0.5;
    float depth = v_pos_sun_coords.z * 0.5 + 0.5;

    float bias = 0.00005;

    float tex_depth = bias + texture2D( u_depth_texture, pos_tex_coords ).r;
    //return depth < tex_depth ? 20.0 : 2.0;
    return 5.0;
}

void main()
{
    float light_diffuse = 0.5 * dot( -normalize(v_normal), normalize(vec3( -0.9, -0.8, -2.0 )) );
    light_diffuse = max(light_diffuse, 0.0);
    float light = min(1.0, light_diffuse + 0.1);
    vec4 texture_color = texture2D( u_material_texture, v_uv );
    texture_color = mix( texture_color, vec4(1.0, 1.0, 1.0, 1.0), u_material_mix );
    gl_FragData[0] = light_from_scene() * light * ( u_color * texture_color );
    gl_FragData[0].a = 1.0;

    gl_FragData[1] = vec4( u_emission, 1.0 );
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
#shader fragment_scene_compose
////////////////////////////////////////////////////////////////////////////////

#version 100
precision highp float;
uniform sampler2D u_scene_texture;
uniform sampler2D u_bloom_texture;
uniform vec2 u_size;
varying vec2 v_uv;
void main()
{
    float exposure = 0.1;

    vec3 scene = texture2D( u_scene_texture, v_uv ).rgb;

    vec3 emission = vec3(0.0, 0.0, 0.0);
    int n_theta = 50;
    int n_radius = 30;
    for (int i = 0; i < n_theta; i++) {
        for (int j = 0; j < n_radius; j++) {
            vec2 uv = v_uv;
            float theta = float(i) / float(n_theta) * 6.28318530718;
            uv.x += float(j) * cos(theta) * (1.0 / u_size.x);
            uv.y += float(j) * sin(theta) * (1.0 / u_size.y);
            emission += (1.0 / (float(n_theta) * float(n_radius))) * texture2D( u_bloom_texture, uv ).rgb;
        }
    }

    vec3 hdr = scene + 50.0 * emission;

    vec3 mapped = vec3(1.0) - exp(-hdr * exposure);

    //mapped = pow(mapped, vec3(1.0 / 2.2));

    gl_FragColor = vec4(mapped, 1.0);
    //gl_FragColor = vec4(color, 1.0);
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
