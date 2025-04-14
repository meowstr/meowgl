#include "render.hpp"
#include "hardware.hpp"
#include "render_utils.hpp"
#include "shape.hpp"

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
        int emission;
    } material_shader;

    struct {
        int id;
        int proj;
        int view;
        int model;
    } highlight_shader;

    struct {
        int id;
        int texture;
        int size;
    } highlight_post_shader;

    struct {
        int id;
        int texture;
        int size;
    } bloom_post_shader;

    struct {
        int id;
        int scene_texture;
        int bloom_texture;
        int size;
    } scene_compose_shader;

    vbuffer_t model_pos_buffer;
    vbuffer_t model_normal_buffer;
    vbuffer_t model_uv_buffer;

    vbuffer_t fb_pos_buffer;
    vbuffer_t fb_uv_buffer;

    framebuffer_t depth_fb;
    framebuffer_t highlight_fb;
    framebuffer_t bloom_fb;
    framebuffer_t scene_fb;

    int scene_fb_texture;
    int scene_fb_emission_texture;
    int depth_fb_texture;
    int highlight_fb_texture;
    int bloom_fb_texture;

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
    intern.material_shader.emission = find_uniform( id, "u_emission" );

    glBindAttribLocation( id, 0, "a_pos" );
    glBindAttribLocation( id, 1, "a_normal" );
    glBindAttribLocation( id, 2, "a_uv" );
}

static void init_shader2()
{
    int id = build_shader(
        find_shader_string( "vertex_mesh_highlight" ),
        find_shader_string( "fragment_highlight" )
    );

    intern.highlight_shader.id = id;
    intern.highlight_shader.proj = find_uniform( id, "u_proj" );
    intern.highlight_shader.view = find_uniform( id, "u_view" );
    intern.highlight_shader.model = find_uniform( id, "u_model" );

    glBindAttribLocation( id, 0, "a_pos" );
    glBindAttribLocation( id, 1, "a_normal" );
    glBindAttribLocation( id, 2, "a_uv" );
}

static void init_shader3()
{
    int id = build_shader(
        find_shader_string( "vertex_screen" ),
        find_shader_string( "fragment_highlight_pos" )
    );

    intern.highlight_post_shader.id = id;
    intern.highlight_post_shader.texture = find_uniform( id, "u_texture" );
    intern.highlight_post_shader.size = find_uniform( id, "u_size" );

    glBindAttribLocation( id, 0, "a_pos" );
    glBindAttribLocation( id, 1, "a_uv" );
}

static void init_shader4()
{
    int id = build_shader(
        find_shader_string( "vertex_screen" ),
        find_shader_string( "fragment_scene_compose" )
    );

    intern.scene_compose_shader.id = id;
    intern.scene_compose_shader.scene_texture =
        find_uniform( id, "u_scene_texture" );
    intern.scene_compose_shader.bloom_texture =
        find_uniform( id, "u_bloom_texture" );
    intern.scene_compose_shader.size = find_uniform( id, "u_size" );

    glBindAttribLocation( id, 0, "a_pos" );
    glBindAttribLocation( id, 1, "a_uv" );
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

    glm_mat4_mul( intern.proj, intern.view, rstate.combined );
}

static void setup_light_camera()
{
    glm_ortho( -10, 10, -10, 10, 0.0f, 1000.0f, intern.proj );

    glm_lookat(
        vec3{ 10.0f, 10.0f, 10.0f },
        vec3{ 0.0f, 0.0f, 0.0f },
        vec3{ 0.0f, 1.0f, 0.0f },
        intern.view
    );
}

static void render_model( int model_id )
{
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
        int model_id = rstate.entity_list[ i ].model;
        set_uniform(
            intern.material_shader.emission,
            rstate.model_emission_list[ model_id ]
        );
        if ( rstate.model_texture_list[ model_id ] == -1 ) {
            glActiveTexture( GL_TEXTURE1 );
            glBindTexture( GL_TEXTURE_2D, 0 );
            set_uniform( intern.material_shader.material_mix, 1.0f );
        } else {
            glActiveTexture( GL_TEXTURE1 );
            glBindTexture(
                GL_TEXTURE_2D,
                rstate.model_texture_list[ model_id ]
            );
            set_uniform( intern.material_shader.material_mix, 0.0f );
        }
        render_model( rstate.entity_list[ i ].model );
    }
}

void update_camera()
{
    // TODO: do less
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
    glm_vec3_zero( rstate.model_emission_list[ id ] );
    rstate.model_count++;

    return id;
}

void render_init()
{
    float pos_buffer[ 6 * 2 ];
    float uv_buffer[ 6 * 2 ];

    rect_t{ -1, -1, 2, 2 }.vertices_2d( pos_buffer );
    rect_t{ 0, 0, 1, 1 }.vertices_2d( uv_buffer );

    rstate.entity_list = new entity_t[ 1024 ];
    rstate.model_list = new wavefront_t[ 32 ];
    rstate.model_texture_list = new int[ 32 ];
    rstate.model_emission_list = new vec3[ 32 ];
    rstate.entity_count = 0;
    rstate.model_count = 0;

    glm_mat4_identity( intern.model );
    glm_mat4_identity( intern.view );
    glm_mat4_identity( intern.proj );

    intern.model_pos_buffer.init( 3 );
    intern.model_normal_buffer.init( 3 );
    intern.model_uv_buffer.init( 2 );

    intern.fb_pos_buffer.init( 2 );
    intern.fb_uv_buffer.init( 2 );

    intern.fb_pos_buffer.set( pos_buffer, 6 );
    intern.fb_uv_buffer.set( uv_buffer, 6 );

    init_shader1();
    init_shader2();
    init_shader3();
    init_shader4();

    intern.depth_fb.init( 4096, 4096 );
    intern.highlight_fb.init( hardware_width(), hardware_height() );
    intern.bloom_fb.init( hardware_width(), hardware_height() );
    intern.scene_fb.init( hardware_width(), hardware_height() );

    intern.depth_fb_texture = intern.depth_fb.init_depth_texture();
    intern.highlight_fb_texture = intern.highlight_fb.init_color_texture( 0 );
    intern.bloom_fb_texture = intern.bloom_fb.init_color_texture( 0 );

    intern.scene_fb.init_depth_texture();
    intern.scene_fb_texture = intern.scene_fb.init_hdr_texture( 0 );
    intern.scene_fb_emission_texture = intern.scene_fb.init_color_texture( 1 );

    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ); // blend alpha
}

static void render_fb()
{
    intern.fb_pos_buffer.enable( 0 );
    intern.fb_uv_buffer.enable( 1 );

    glDrawArrays( GL_TRIANGLES, 0, 6 );
}

static void render_highlight()
{
    if ( rstate.hi_entity == -1 ) return;

    glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );
    glBindFramebuffer( GL_FRAMEBUFFER, intern.highlight_fb.id );
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glUseProgram( intern.highlight_shader.id );

    int e = rstate.hi_entity;
    set_uniform( intern.highlight_shader.proj, intern.proj );
    set_uniform( intern.highlight_shader.view, intern.view );
    set_uniform(
        intern.highlight_shader.model,
        rstate.entity_list[ e ].transform.m
    );
    render_model( rstate.entity_list[ e ].model );

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );

    glUseProgram( intern.highlight_post_shader.id );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, intern.highlight_fb_texture );

    vec2 size;
    size[ 0 ] = hardware_width();
    size[ 1 ] = hardware_height();

    set_uniform( intern.highlight_post_shader.texture, 0 );
    set_uniform( intern.highlight_post_shader.size, size );

    render_fb();
    glEnable( GL_CULL_FACE );
    glEnable( GL_DEPTH_TEST );
}

static void render_shadow_map()
{
    glCullFace( GL_FRONT );
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
}

static void enable_attachment0()
{
    GLenum buffers[]{ GL_COLOR_ATTACHMENT0 };
    glDrawBuffers( 1, buffers );
}

static void enable_attachment1()
{
    GLenum buffers[]{ GL_COLOR_ATTACHMENT1 };
    glDrawBuffers( 1, buffers );
}

static void enable_attachment0_attachment1()
{
    GLenum buffers[]{ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers( 2, buffers );
}

void render()
{
    vec2 size;
    size[ 0 ] = hardware_width();
    size[ 1 ] = hardware_height();

    render_shadow_map();

    setup_camera();

    glBindFramebuffer( GL_FRAMEBUFFER, intern.scene_fb.id );

    enable_attachment0();
    glClearColor( 1.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    enable_attachment1();
    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    enable_attachment0_attachment1();
    glViewport( 0, 0, hardware_width(), hardware_height() );
    glCullFace( GL_BACK );

    glUseProgram( intern.material_shader.id );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, intern.depth_fb_texture );

    set_uniform( intern.material_shader.view, intern.view );
    set_uniform( intern.material_shader.proj, intern.proj );
    set_uniform( intern.material_shader.sun_combined, intern.sun_combined );
    set_uniform( intern.material_shader.depth_texture, 0 );

    render_scene();

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    enable_attachment0();

    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glUseProgram( intern.scene_compose_shader.id );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, intern.scene_fb_texture );
    glActiveTexture( GL_TEXTURE1 );
    glBindTexture( GL_TEXTURE_2D, intern.scene_fb_emission_texture );
    set_uniform( intern.scene_compose_shader.scene_texture, 0 );
    set_uniform( intern.scene_compose_shader.bloom_texture, 1 );
    set_uniform( intern.scene_compose_shader.size, size );

    glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );

    render_fb();

    glEnable( GL_CULL_FACE );
    glEnable( GL_DEPTH_TEST );

    render_highlight();
}
