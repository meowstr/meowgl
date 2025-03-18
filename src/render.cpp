#include "render.hpp"
#include "hardware.hpp"
#include "render_utils.hpp"

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include <cglm/affine.h>
#include <cglm/cam.h>
#include <cglm/mat4.h>

#include <math.h>
#include <string.h>

renderstate_t rstate;

void transform_t::update()
{
    glm_mat4_identity( m );
    glm_translate_x( m, pos[ 0 ] );
    glm_translate_y( m, pos[ 1 ] );
    glm_translate_z( m, pos[ 2 ] );
    glm_rotate_z( m, glm_rad( rot[ 2 ] ), m );
    glm_rotate_y( m, glm_rad( rot[ 1 ] ), m );
    glm_rotate_x( m, glm_rad( rot[ 0 ] ), m );
    glm_scale( m, scale );
}

void transform_t::identity()
{
    pos[ 0 ] = 0.0f;
    pos[ 1 ] = 0.0f;
    pos[ 2 ] = 0.0f;
    rot[ 0 ] = 0.0f;
    rot[ 1 ] = 0.0f;
    rot[ 2 ] = 0.0f;
    scale[ 0 ] = 1.0f;
    scale[ 1 ] = 1.0f;
    scale[ 2 ] = 1.0f;
    update();
}

// render state
struct {
    mat4 model;
    mat4 view;
    mat4 proj;

    mat4 sun_combined;

    struct {
        int id;
        int proj;
        int view;
        int model;
        int color;
        int sun_combined;
        int depth_texture;
        int material_texture;
        int material_mix;
    } material_shader;

    vbuffer_t model_pos_buffer;
    vbuffer_t model_normal_buffer;
    vbuffer_t model_uv_buffer;

    vbuffer_t fb_pos_buffer;

    framebuffer_t depth_fb;

} intern;

static void init_shader1()
{
    int id = build_shader(
        find_shader_string( "vertex_mesh" ),
        find_shader_string( "fragment_material" )
    );
    intern.material_shader.id = id;

    intern.material_shader.proj = find_uniform( id, "u_proj" );
    intern.material_shader.view = find_uniform( id, "u_view" );
    intern.material_shader.model = find_uniform( id, "u_model" );
    intern.material_shader.color = find_uniform( id, "u_color" );
    intern.material_shader.sun_combined = find_uniform( id, "u_sun_combined" );
    intern.material_shader.depth_texture =
        find_uniform( id, "u_depth_texture" );
    intern.material_shader.material_texture =
        find_uniform( id, "u_material_texture" );
    intern.material_shader.material_mix = find_uniform( id, "u_material_mix" );

    glBindAttribLocation( id, 0, "a_pos" );
    glBindAttribLocation( id, 1, "a_normal" );
    glBindAttribLocation( id, 2, "a_uv" );
}

static void setup_camera()
{
    glm_perspective(
        glm_rad( 45.0f ),
        (float) hardware_width() / hardware_height(),
        0.01f,
        10000.0f,
        intern.proj
    );

    glm_mat4_identity( rstate.view_inverse );

    glm_translate_x( rstate.view_inverse, rstate.camera.pos[ 0 ] );
    glm_translate_y( rstate.view_inverse, rstate.camera.pos[ 1 ] );
    glm_translate_z( rstate.view_inverse, rstate.camera.pos[ 2 ] );

    glm_rotate_y( rstate.view_inverse, rstate.camera.yaw, rstate.view_inverse );
    glm_rotate_x(
        rstate.view_inverse,
        rstate.camera.pitch,
        rstate.view_inverse
    );

    // TODO: solve with transpose instead
    glm_mat4_inv( rstate.view_inverse, intern.view );
}

static void setup_light_camera()
{
    glm_ortho( -5, 5, -5, 5, 0.0f, 1000.0f, intern.proj );

    glm_lookat(
        vec3{ 10.0f, 10.0f, 10.0f },
        vec3{ 0.0f, 0.0f, 0.0f },
        vec3{ 0.0f, 1.0f, 0.0f },
        intern.view
    );
}

static void render_model( int model_id )
{
    if ( rstate.model_texture_list[ model_id ] == -1 ) {
        glActiveTexture( GL_TEXTURE1 );
        glBindTexture( GL_TEXTURE_2D, 0 );
        set_uniform( intern.material_shader.material_mix, 1.0f );
    } else {
        glActiveTexture( GL_TEXTURE1 );
        glBindTexture( GL_TEXTURE_2D, rstate.model_texture_list[ model_id ] );
        set_uniform( intern.material_shader.material_mix, 0.0f );
    }

    wavefront_t & model = rstate.model_list[ model_id ];

    intern.model_pos_buffer.set( model.pos_list, model.vertex_count );
    intern.model_normal_buffer.set( model.normal_list, model.vertex_count );
    intern.model_uv_buffer.set( model.uv_list, model.vertex_count );

    intern.model_pos_buffer.enable( 0 );
    intern.model_normal_buffer.enable( 1 );
    intern.model_uv_buffer.enable( 2 );

    glDrawArrays( GL_TRIANGLES, 0, model.vertex_count );
}

static void render_scene()
{
    vec4 white{ 1.0f, 1.0f, 1.0f, 1.0f };

    for ( int i = 0; i < rstate.entity_count; i++ ) {
        set_uniform(
            intern.material_shader.model,
            rstate.entity_list[ i ].transform.m
        );
        set_uniform( intern.material_shader.material_texture, 1 );
        set_uniform( intern.material_shader.color, white );
        render_model( rstate.entity_list[ i ].model );
    }
}

void update_camera()
{
    setup_camera();
}

int add_entity( int model, float x, float y, float z )
{
    int id = add_entity( model );

    rstate.entity_list[ id ].transform.pos[ 0 ] = x;
    rstate.entity_list[ id ].transform.pos[ 1 ] = y;
    rstate.entity_list[ id ].transform.pos[ 2 ] = z;
    rstate.entity_list[ id ].transform.update();

    return id;
}

int add_entity( int model )
{
    int id = rstate.entity_count;
    rstate.entity_count++;

    rstate.entity_list[ id ].model = model;

    rstate.entity_list[ id ].transform.identity();

    return id;
}

int add_model( const char * filename )
{
    for ( int i = 0; i < rstate.model_count; i++ ) {
        if ( strcmp( filename, rstate.model_list[ i ].filename ) == 0 ) {
            return i;
        }
    }

    int id = rstate.model_count;
    load_wavefront( rstate.model_list + id, find_res( filename ) );
    rstate.model_list[ id ].filename = strdup( filename );
    rstate.model_texture_list[ id ] = -1;
    rstate.model_count++;

    return id;
}

void render_init()
{
    rstate.entity_list = new entity_t[ 1024 ];
    rstate.model_list = new wavefront_t[ 32 ];
    rstate.model_texture_list = new int[ 32 ];
    rstate.entity_count = 0;
    rstate.model_count = 0;

    glm_mat4_identity( intern.model );
    glm_mat4_identity( intern.view );
    glm_mat4_identity( intern.proj );

    intern.model_pos_buffer.init( 3 );
    intern.model_normal_buffer.init( 3 );
    intern.model_uv_buffer.init( 2 );

    init_shader1();

    intern.depth_fb.init_depth( 1024, 1024 );

    glEnable( GL_DEPTH_TEST );
    //  glEnable( GL_BLEND );
    //  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ); // blend alpha
}

static void render_fb( framebuffer_t fb )
{
}

static void render_shadow_map()
{
    glBindFramebuffer( GL_FRAMEBUFFER, intern.depth_fb.id );
    glViewport( 0, 0, intern.depth_fb.width, intern.depth_fb.height );
    glClear( GL_DEPTH_BUFFER_BIT );

    setup_light_camera();

    glm_mat4_mul( intern.proj, intern.view, intern.sun_combined );

    glUseProgram( intern.material_shader.id );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, 0 );

    set_uniform( intern.material_shader.view, intern.view );
    set_uniform( intern.material_shader.proj, intern.proj );
    set_uniform( intern.material_shader.depth_texture, 0 );

    render_scene();

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    glViewport( 0, 0, hardware_width(), hardware_height() );
}

void render()
{
    render_shadow_map();

    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    setup_camera();

    glUseProgram( intern.material_shader.id );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, intern.depth_fb.texture );

    set_uniform( intern.material_shader.view, intern.view );
    set_uniform( intern.material_shader.proj, intern.proj );
    set_uniform( intern.material_shader.sun_combined, intern.sun_combined );
    set_uniform( intern.material_shader.depth_texture, 0 );

    render_scene();
}
