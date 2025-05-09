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
        int material_texture;
        int material_mix;
        int emission;
    } deferred_shader;

    struct {
        int id;
        int combined;
        int model;
    } shadow_shader;

    struct {
        int id;
        int position_texture;
        int normal_texture;
        int depth_texture;
        int depth_tile;
        int light_matrix;
        int light_pos;
        int shadow_bias;
    } light_shader;

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
        int color_texture;
        int light_mask_texture;
        int bloom_texture;
        int size;
    } scene_compose_shader;

    vbuffer_t vertex_pos_buffer;
    vbuffer_t vertex_normal_buffer;
    vbuffer_t vertex_uv_buffer;

    vbuffer_t fb_pos_buffer;
    vbuffer_t fb_uv_buffer;

    framebuffer_t deferred_fb;
    int deferred_position_texture;
    int deferred_normal_texture;
    int deferred_color_texture;
    int deferred_emission_texture;
    int deferred_depth_texture;

    framebuffer_t light_fb;
    int light_mask_texture;

    framebuffer_t depth_fb;
    int shadowmap_texture;

    framebuffer_t highlight_fb;
    framebuffer_t bloom_fb;
    framebuffer_t scene_fb;

    int scene_fb_texture;
    int scene_fb_emission_texture;
    int highlight_fb_texture;
    int bloom_fb_texture;

} intern;

static void init_shader1()
{
    int id = build_shader(
        find_shader_string( "vertex_deferred" ),
        find_shader_string( "fragment_deferred" )
    );
    intern.deferred_shader.id = id;

    intern.deferred_shader.proj = find_uniform( id, "u_proj" );
    intern.deferred_shader.view = find_uniform( id, "u_view" );
    intern.deferred_shader.model = find_uniform( id, "u_model" );
    intern.deferred_shader.color = find_uniform( id, "u_color" );
    intern.deferred_shader.material_texture =
        find_uniform( id, "u_material_texture" );
    intern.deferred_shader.material_mix = find_uniform( id, "u_material_mix" );
    intern.deferred_shader.emission = find_uniform( id, "u_emission" );

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
    intern.scene_compose_shader.color_texture =
        find_uniform( id, "u_color_texture" );
    intern.scene_compose_shader.light_mask_texture =
        find_uniform( id, "u_light_mask_texture" );
    intern.scene_compose_shader.bloom_texture =
        find_uniform( id, "u_bloom_texture" );
    intern.scene_compose_shader.size = find_uniform( id, "u_size" );

    glBindAttribLocation( id, 0, "a_pos" );
    glBindAttribLocation( id, 1, "a_uv" );
}

static void init_shader5()
{
    int id = build_shader(
        find_shader_string( "vertex_screen" ),
        find_shader_string( "fragment_light" )
    );

    intern.light_shader.id = id;
    intern.light_shader.position_texture =
        find_uniform( id, "u_position_texture" );
    intern.light_shader.normal_texture = find_uniform( id, "u_normal_texture" );
    intern.light_shader.depth_texture = find_uniform( id, "u_depth_texture" );
    intern.light_shader.depth_tile = find_uniform( id, "u_depth_tile" );
    intern.light_shader.light_matrix = find_uniform( id, "u_light_matrix" );
    intern.light_shader.light_pos = find_uniform( id, "u_light_pos" );
    intern.light_shader.shadow_bias = find_uniform( id, "u_shadow_bias" );

    glBindAttribLocation( id, 0, "a_pos" );
    glBindAttribLocation( id, 1, "a_uv" );
}

static void init_shader6()
{
    int id = build_shader(
        find_shader_string( "vertex_shadow" ),
        find_shader_string( "fragment_shadow" )
    );

    intern.shadow_shader.id = id;
    intern.shadow_shader.combined = find_uniform( id, "u_combined" );
    intern.shadow_shader.model = find_uniform( id, "u_model" );

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

void compute_camera_matrices()
{
    setup_camera();
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
    intern.vertex_pos_buffer.enable( 0 );
    intern.vertex_normal_buffer.enable( 1 );
    intern.vertex_uv_buffer.enable( 2 );

    glDrawArrays(
        GL_TRIANGLES,
        rstate.model_offset_list[ model_id ],
        rstate.model_size_list[ model_id ]
    );
}

static void render_scene()
{
    vec4 white{ 1.0f, 1.0f, 1.0f, 1.0f };
    vec3 white_emission{ 1.0f, 1.0f, 1.0f };
    vec3 black_emission{ 0.0f, 0.0f, 0.0f };

    for ( int i = 0; i < rstate.e_model_count; i++ ) {
        int e = rstate.e_model_entity_list[ i ];
        int model_id = rstate.entity_model_list[ e ];
        int texture = rstate.model_texture_list[ model_id ];

        set_uniform(
            intern.deferred_shader.model,
            rstate.entity_transform_list[ e ].m
        );
        set_uniform( intern.deferred_shader.material_texture, 1 );
        set_uniform( intern.deferred_shader.color, white );
        set_uniform(
            intern.deferred_shader.emission,
            rstate.model_emission_list[ model_id ]
        );
        if ( texture == -1 ) {
            glActiveTexture( GL_TEXTURE1 );
            glBindTexture( GL_TEXTURE_2D, 0 );
            set_uniform( intern.deferred_shader.material_mix, 1.0f );
        } else {
            glActiveTexture( GL_TEXTURE1 );
            glBindTexture( GL_TEXTURE_2D, texture );
            set_uniform( intern.deferred_shader.material_mix, 0.0f );
        }

        render_model( model_id );
    }

    // TODO: move outside of deferred pipeline
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    for ( int i = 0; i < rstate.e_light_count; i++ ) {
        int e = rstate.e_light_entity_list[ i ];
        int model_id = rstate.entity_model_list[ e ];
        set_uniform(
            intern.deferred_shader.model,
            rstate.entity_transform_list[ e ].m
        );
        set_uniform( intern.deferred_shader.color, white );
        set_uniform( intern.deferred_shader.emission, black_emission );
        set_uniform( intern.deferred_shader.material_mix, 1.0f );
        render_model( model_id );
    }
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

static void setup_tables()
{
    rstate.vertex_count = 0;
    rstate.vertex_cap = 1024;
    rstate.vertex_pos_list = new float[ 1024 * 3 ];
    rstate.vertex_normal_list = new float[ 1024 * 3 ];
    rstate.vertex_uv_list = new float[ 1024 * 2 ];

    rstate.model_count = 0;
    rstate.model_size_list = new int[ MEOWGL_MAX_MODEL_COUNT ];
    rstate.model_offset_list = new int[ MEOWGL_MAX_MODEL_COUNT ];
    rstate.model_emission_list = new vec3[ MEOWGL_MAX_MODEL_COUNT ];
    rstate.model_texture_list = new int[ MEOWGL_MAX_MODEL_COUNT ];

    rstate.entity_count = 0;
    rstate.entity_model_list = new int[ MEOWGL_MAX_ENTITY_COUNT ];
    rstate.entity_transform_list = new transform_t[ MEOWGL_MAX_ENTITY_COUNT ];

    rstate.e_model_count = 0;
    rstate.e_model_entity_list = new int[ MEOWGL_MAX_ENTITY_COUNT ];

    rstate.e_light_count = 0;
    rstate.e_light_entity_list = new int[ MEOWGL_MAX_ENTITY_COUNT ];

    rstate.e_nocast_light_count = 0;
    rstate.e_nocast_light_entity_list = new int[ MEOWGL_MAX_ENTITY_COUNT ];
}

void render_init()
{
    setup_tables();

    rstate.shadow_bias = 0.01;

    float pos_buffer[ 6 * 2 ];
    float uv_buffer[ 6 * 2 ];

    rect_t{ -1, -1, 2, 2 }.vertices_2d( pos_buffer );
    rect_t{ 0, 0, 1, 1 }.vertices_2d( uv_buffer );

    glm_mat4_identity( intern.model );
    glm_mat4_identity( intern.view );
    glm_mat4_identity( intern.proj );

    intern.vertex_pos_buffer.init( 3 );
    intern.vertex_normal_buffer.init( 3 );
    intern.vertex_uv_buffer.init( 2 );

    intern.fb_pos_buffer.init( 2 );
    intern.fb_uv_buffer.init( 2 );

    intern.fb_pos_buffer.set( pos_buffer, 6 );
    intern.fb_uv_buffer.set( uv_buffer, 6 );

    init_shader1();
    init_shader2();
    init_shader3();
    init_shader4();
    init_shader5();
    init_shader6();

    intern.deferred_fb.init( hardware_width(), hardware_height() );

    // clang-format off
    intern.deferred_color_texture = intern.deferred_fb.init_color_texture( 0 );
    intern.deferred_position_texture = intern.deferred_fb.init_hdr_texture( 1 );
    intern.deferred_normal_texture = intern.deferred_fb.init_hdr_texture( 2 );
    intern.deferred_emission_texture = intern.deferred_fb.init_color_texture( 3 );
    intern.deferred_depth_texture = intern.deferred_fb.init_depth_texture();
    // clang-format on

    intern.light_fb.init( hardware_width(), hardware_height() );
    intern.light_mask_texture = intern.light_fb.init_color_texture( 0 );

    intern.depth_fb.init( 8192, 8192 );
    intern.highlight_fb.init( hardware_width(), hardware_height() );
    intern.bloom_fb.init( hardware_width(), hardware_height() );
    intern.scene_fb.init( hardware_width(), hardware_height() );

    intern.shadowmap_texture = intern.depth_fb.init_depth_texture();
    intern.highlight_fb_texture = intern.highlight_fb.init_color_texture( 0 );
    intern.bloom_fb_texture = intern.bloom_fb.init_color_texture( 0 );

    intern.scene_fb.init_depth_texture();
    intern.scene_fb_texture = intern.scene_fb.init_hdr_texture( 0 );
    intern.scene_fb_emission_texture = intern.scene_fb.init_color_texture( 1 );

    glEnable( GL_MULTISAMPLE );
    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ); // blend alpha
}

void update_vertex_buffers()
{
    intern.vertex_pos_buffer.set( rstate.vertex_pos_list, rstate.vertex_count );
    intern.vertex_normal_buffer.set(
        rstate.vertex_normal_list,
        rstate.vertex_count
    );
    intern.vertex_uv_buffer.set( rstate.vertex_uv_list, rstate.vertex_count );
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
        rstate.entity_transform_list[ e ].m
    );
    render_model( rstate.entity_model_list[ e ] );

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

static void enable_n_attachments( int n )
{
    static GLenum buffers[ 32 ];

    for ( int i = 0; i < n; i++ ) {
        buffers[ i ] = GL_COLOR_ATTACHMENT0 + i;
    }

    glDrawBuffers( n, buffers );
}

static void do_geometry_pass()
{
    setup_camera();

    glBindFramebuffer( GL_FRAMEBUFFER, intern.deferred_fb.id );

    enable_n_attachments( 4 );

    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glViewport( 0, 0, hardware_width(), hardware_height() );
    glCullFace( GL_BACK );

    glUseProgram( intern.deferred_shader.id );

    set_uniform( intern.deferred_shader.view, intern.view );
    set_uniform( intern.deferred_shader.proj, intern.proj );

    render_scene();
}

static void compute_shadow_tile( ivec4 out, int shadow_index )
{
    out[ 0 ] = 1024 * ( shadow_index % 8 );
    out[ 1 ] = 1024 * ( shadow_index / 8 );
    out[ 2 ] = 1024;
    out[ 3 ] = 1024;
}

static void compute_shadow_matrix( mat4 out, vec3 pos, int dir )
{
    static mat4 m1;
    static mat4 m2;

    glm_perspective( glm_rad( 90 ), 1, 0.01, 10, m1 );

    if ( dir == 0 ) {
        glm_look( pos, vec3{ 1, 0, 0 }, vec3{ 0, 1, 0 }, m2 );
    } else if ( dir == 1 ) {
        glm_look( pos, vec3{ 0, 1, 0 }, vec3{ 0, 0, 1 }, m2 );
    } else if ( dir == 2 ) {
        glm_look( pos, vec3{ 0, 0, 1 }, vec3{ 1, 0, 0 }, m2 );
    } else if ( dir == 3 ) {
        glm_look( pos, vec3{ -1, 0, 0 }, vec3{ 0, 1, 0 }, m2 );
    } else if ( dir == 4 ) {
        glm_look( pos, vec3{ 0, -1, 0 }, vec3{ 0, 0, 1 }, m2 );
    } else {
        glm_look( pos, vec3{ 0, 0, -1 }, vec3{ 1, 0, 0 }, m2 );
    }

    glm_mat4_mul( m1, m2, out );
}

static void compute_shadow_map( int shadow_index, vec3 pos, int dir )
{
    ivec4 tile;
    mat4 m;

    compute_shadow_tile( tile, shadow_index );
    compute_shadow_matrix( m, pos, dir );

    glViewport( tile[ 0 ], tile[ 1 ], tile[ 2 ], tile[ 3 ] );

    // clear the tile
    // glEnable( GL_SCISSOR_TEST );
    // glScissor( tile[ 0 ], tile[ 1 ], tile[ 2 ], tile[ 3 ] );
    // glClear( GL_DEPTH_BUFFER_BIT );
    // glDisable( GL_SCISSOR_TEST );

    set_uniform( intern.shadow_shader.combined, m );

    for ( int i = 0; i < rstate.e_model_count; i++ ) {
        int e = rstate.e_model_entity_list[ i ];
        set_uniform(
            intern.shadow_shader.model,
            rstate.entity_transform_list[ e ].m
        );
        render_model( rstate.entity_model_list[ e ] );
    }
}

void compute_all_shadow_maps()
{
    glBindFramebuffer( GL_FRAMEBUFFER, intern.depth_fb.id );
    glClear( GL_DEPTH_BUFFER_BIT );
    glUseProgram( intern.shadow_shader.id );
    enable_n_attachments( 1 );

    int shadow_count = 0;
    for ( int i = 0; i < rstate.e_light_count; i++ ) {
        int e = rstate.e_light_entity_list[ i ];
        vec3 & pos = rstate.entity_transform_list[ e ].pos;
        compute_shadow_map( shadow_count++, pos, 0 );
        compute_shadow_map( shadow_count++, pos, 1 );
        compute_shadow_map( shadow_count++, pos, 2 );
        compute_shadow_map( shadow_count++, pos, 3 );
        compute_shadow_map( shadow_count++, pos, 4 );
        compute_shadow_map( shadow_count++, pos, 5 );
    }
}

static void do_shadow_pass( int shadow_index, vec3 & pos, int dir )
{
    mat4 m;
    ivec4 tile;
    compute_shadow_matrix( m, pos, dir );
    compute_shadow_tile( tile, shadow_index );

    // TODO: idk make this better
    vec4 float_tile;
    float_tile[ 0 ] = tile[ 0 ];
    float_tile[ 1 ] = tile[ 1 ];
    float_tile[ 2 ] = tile[ 2 ];
    float_tile[ 3 ] = tile[ 3 ];

    set_uniform( intern.light_shader.light_pos, pos );
    set_uniform( intern.light_shader.light_matrix, m );
    set_uniform( intern.light_shader.depth_tile, float_tile );

    render_fb();
}

static void do_all_shadow_passes()
{
    glBindFramebuffer( GL_FRAMEBUFFER, intern.light_fb.id );
    glViewport( 0, 0, hardware_width(), hardware_height() );
    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glUseProgram( intern.light_shader.id );
    enable_n_attachments( 1 );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, intern.deferred_position_texture );
    glActiveTexture( GL_TEXTURE1 );
    glBindTexture( GL_TEXTURE_2D, intern.deferred_normal_texture );
    glActiveTexture( GL_TEXTURE2 );
    glBindTexture( GL_TEXTURE_2D, intern.shadowmap_texture );
    set_uniform( intern.light_shader.position_texture, 0 );
    set_uniform( intern.light_shader.normal_texture, 1 );
    set_uniform( intern.light_shader.depth_texture, 2 );

    set_uniform( intern.light_shader.shadow_bias, rstate.shadow_bias );

    glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );
    glBlendFunc( GL_ONE, GL_ONE ); // add

    int shadow_count = 0;
    for ( int i = 0; i < rstate.e_light_count; i++ ) {
        int e = rstate.e_light_entity_list[ i ];
        vec3 & pos = rstate.entity_transform_list[ e ].pos;
        do_shadow_pass( shadow_count++, pos, 0 );
        do_shadow_pass( shadow_count++, pos, 1 );
        do_shadow_pass( shadow_count++, pos, 2 );
        do_shadow_pass( shadow_count++, pos, 3 );
        do_shadow_pass( shadow_count++, pos, 4 );
        do_shadow_pass( shadow_count++, pos, 5 );
    }

    glEnable( GL_CULL_FACE );
    glEnable( GL_DEPTH_TEST );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ); // blend alpha
}

static void do_composition_pass()
{
    vec2 size;
    size[ 0 ] = hardware_width();
    size[ 1 ] = hardware_height();

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );

    enable_n_attachments( 1 );

    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glUseProgram( intern.scene_compose_shader.id );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, intern.deferred_color_texture );
    glActiveTexture( GL_TEXTURE1 );
    glBindTexture( GL_TEXTURE_2D, intern.light_mask_texture );
    glActiveTexture( GL_TEXTURE2 );
    glBindTexture( GL_TEXTURE_2D, intern.deferred_emission_texture );

    set_uniform( intern.scene_compose_shader.color_texture, 0 );
    set_uniform( intern.scene_compose_shader.light_mask_texture, 1 );
    set_uniform( intern.scene_compose_shader.bloom_texture, 2 );
    set_uniform( intern.scene_compose_shader.size, size );

    glDisable( GL_CULL_FACE );
    glDisable( GL_DEPTH_TEST );

    render_fb();

    glEnable( GL_CULL_FACE );
    glEnable( GL_DEPTH_TEST );
}

void render()
{
    // compute_all_shadow_maps();

    do_geometry_pass();

    do_all_shadow_passes();

    do_composition_pass();

    render_highlight();

    //{
    //    glBindFramebuffer( GL_READ_FRAMEBUFFER, intern.light_fb.id );
    //    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );

    //    glReadBuffer( GL_COLOR_ATTACHMENT0 );
    //    glDrawBuffer( GL_COLOR_ATTACHMENT0 );

    //    glBlitFramebuffer(
    //        0,
    //        0,
    //        hardware_width(),
    //        hardware_height(),
    //        0,
    //        0,
    //        hardware_width(),
    //        hardware_height(),
    //        GL_COLOR_BUFFER_BIT,
    //        GL_NEAREST
    //    );
    //}
}
