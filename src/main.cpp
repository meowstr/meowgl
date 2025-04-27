#include "hardware.hpp"
#include "logging.hpp"
#include "render.hpp"
#include "render_utils.hpp"
#include "state.hpp"

#include <cJSON.h>
#include <cglm/affine.h>
#include <cglm/mat4.h>
#include <cglm/project.h>
#include <cglm/ray.h>
#include <imgui.h>

#include <cgltf.h>

#include <stdio.h>

#ifdef __unix__

#include <dirent.h>
#include <sys/types.h>

static void setup_resource_list()
{
    state.avail_model_file_count = 0;

    dirent * p;
    DIR * d = opendir( "../res/" );
    if ( d ) {
        while ( ( p = readdir( d ) ) ) {
            if ( p->d_type == DT_DIR ) continue;
            if ( strstr( p->d_name, ".obj" ) == nullptr ) continue;
            if ( state.avail_model_file_count >= 32 ) break;
            INFO_LOG( "file: %s", p->d_name );

            state.avail_model_file_list[ state.avail_model_file_count++ ] =
                strdup( p->d_name );
        }

        closedir( d );
    }
}
#else

#include "windows.h"

static void setup_resource_list()
{
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;

    char sPath[ 2048 ];

    // Specify a file mask. *.* = We want everything!
    sprintf( sPath, "%s\\*.*", "../../res" );

    if ( ( hFind = FindFirstFile( sPath, &fdFile ) ) == INVALID_HANDLE_VALUE ) {
        return;
    }

    do {
        if ( strcmp( fdFile.cFileName, "." ) == 0 ) continue;
        if ( strcmp( fdFile.cFileName, ".." ) == 0 ) continue;
        if ( strstr( fdFile.cFileName, ".obj" ) == nullptr ) continue;
        if ( fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;

        state.avail_model_file_list[ state.avail_model_file_count++ ] =
            strdup( fdFile.cFileName );

    } while ( FindNextFile( hFind, &fdFile ) ); // Find the next file.

    FindClose( hFind ); // Always, Always, clean things up!
}
#endif

static float intersect_entity( int e, vec3 origin, vec3 direction )
{
    // bool glm_ray_triangle(vec3 origin, vec3 direction, vec3 v0, vec3 v1, vec3
    // v2, float *d)

    int model_id = rstate.entity_model_list[ e ];

    int model_offset = rstate.model_offset_list[ model_id ];
    int model_size = rstate.model_size_list[ model_id ]; // vertex count

    float * pos_list = rstate.vertex_pos_list + ( model_offset * 3 );

    mat4 & t = rstate.entity_transform_list[ e ].m;

    float closest_distance = FLT_MAX;

    for ( int i = 0; i < model_size / 3; i++ ) {
        vec4 v0;
        vec4 v1;
        vec4 v2;

        v0[ 3 ] = 1;
        v1[ 3 ] = 1;
        v2[ 3 ] = 1;

        float distance = 0.0f;

        glm_vec3_copy( pos_list + 3 * ( i * 3 + 0 ), v0 );
        glm_vec3_copy( pos_list + 3 * ( i * 3 + 1 ), v1 );
        glm_vec3_copy( pos_list + 3 * ( i * 3 + 2 ), v2 );

        glm_mat4_mulv( t, v0, v0 );
        glm_mat4_mulv( t, v1, v1 );
        glm_mat4_mulv( t, v2, v2 );

        bool intersect =
            glm_ray_triangle( origin, direction, v0, v1, v2, &distance );

        if ( intersect && distance < closest_distance )
            closest_distance = distance;
    }

    return closest_distance;
}

static int intersect_scene( vec3 origin, vec3 direction )
{
    int closest = -1;
    float closest_distance = FLT_MAX;

    for ( int i = 0; i < rstate.entity_count; i++ ) {
        float distance = intersect_entity( i, origin, direction );

        if ( distance < closest_distance ) {
            closest = i;
            closest_distance = distance;
        }
    }

    return closest;
}

static void pick_entity()
{
    vec3 origin;
    vec3 direction;

    vec3 point{
        hardware_touch_x(),
        hardware_height() - hardware_touch_y(),
        -1.0
    }; // far plane

    vec4 vp{ 0.0f, 0.0f, (float) hardware_width(), (float) hardware_height() };

    glm_unproject( point, rstate.combined, vp, point );

    glm_vec3_copy( rstate.camera.pos, origin );
    glm_vec3_sub( point, origin, direction );

    int e = intersect_scene( origin, direction );

    if ( e != -1 ) {
        rstate.hi_entity = e;
        state.current_entity = e;
    } else {
        state.current_entity = -1;
        rstate.hi_entity = -1;
    }
}

int add_model(
    float * pos_list,
    float * norm_list,
    float * uv_list,
    int vertex_count
)
{
    float * out_pos = rstate.vertex_pos_list;
    float * out_normal = rstate.vertex_normal_list;
    float * out_uv = rstate.vertex_uv_list;

    // resize
    while ( rstate.vertex_count + vertex_count > rstate.vertex_cap ) {
        int new_cap = rstate.vertex_cap * 2;
        out_pos = new float[ new_cap * 3 ];
        out_normal = new float[ new_cap * 3 ];
        out_uv = new float[ new_cap * 2 ];

        memcpy(
            out_pos,
            rstate.vertex_pos_list,
            sizeof( float ) * rstate.vertex_cap * 3
        );
        memcpy(
            out_normal,
            rstate.vertex_normal_list,
            sizeof( float ) * rstate.vertex_cap * 3
        );
        memcpy(
            out_uv,
            rstate.vertex_uv_list,
            sizeof( float ) * rstate.vertex_cap * 2
        );

        delete[] rstate.vertex_pos_list;
        delete[] rstate.vertex_normal_list;
        delete[] rstate.vertex_uv_list;

        rstate.vertex_pos_list = out_pos;
        rstate.vertex_normal_list = out_normal;
        rstate.vertex_uv_list = out_uv;
        rstate.vertex_cap = new_cap;
    }

    int offset = rstate.vertex_count;

    memcpy(
        out_pos + ( rstate.vertex_count * 3 ),
        pos_list,
        sizeof( float ) * vertex_count * 3
    );
    memcpy(
        out_normal + ( rstate.vertex_count * 3 ),
        norm_list,
        sizeof( float ) * vertex_count * 3
    );
    memcpy(
        out_uv + ( rstate.vertex_count * 2 ),
        uv_list,
        sizeof( float ) * vertex_count * 2
    );

    rstate.vertex_count += vertex_count;

    update_vertex_buffers();

    int id = rstate.model_count++;

    rstate.model_offset_list[ id ] = offset;
    rstate.model_size_list[ id ] = vertex_count;
    rstate.model_texture_list[ id ] = -1;
    glm_vec3_zero( rstate.model_emission_list[ id ] );

    return id;
}

static int add_model( const char * filename )
{
    for ( int i = 0; i < rstate.model_count; i++ ) {
        if ( strcmp( filename, state.model_file_list[ i ] ) == 0 ) {
            return i;
        }
    }

    wavefront_t file;
    load_wavefront( &file, find_res( filename ) );
    int id = add_model(
        file.pos_list,
        file.normal_list,
        file.uv_list,
        file.vertex_count
    );
    state.model_file_list[ id ] = strdup( filename );
    state.model_file_count++;

    return id;
}

static int add_entity()
{
    int id = rstate.entity_count++;

    rstate.entity_model_list[ id ] = -1;
    rstate.entity_transform_list[ id ].identity();

    return id;
}

static int add_model_entity()
{
    int id = rstate.e_model_count++;
    int e = add_entity();

    rstate.e_model_entity_list[ id ] = e;

    return e;
}

static int add_light_entity()
{
    int id = rstate.e_light_count++;
    int e = add_entity();

    rstate.e_light_entity_list[ id ] = e;
    rstate.entity_model_list[ e ] = rstate.light_model;

    return e;
}

static void rotate_entity( float dtheta )
{
    if ( state.current_entity == -1 ) return;

    transform_t & current_t =
        rstate.entity_transform_list[ state.current_entity ];

    current_t.rot[ state.current_axis ] += dtheta;
    current_t.update();
}

static int index_of( int * list, int count, int value )
{
    for ( int i = 0; i < count; i++ ) {
        if ( list[ i ] == value ) return i;
    }

    return -1;
}

static void duplicate_current_entity()
{
    if ( state.current_entity == -1 ) return;

    int e = state.current_entity;
    int new_e = -1;

    if ( index_of( rstate.e_model_entity_list, rstate.e_model_count, e ) !=
         -1 ) {
        new_e = add_model_entity();
    }

    if ( index_of( rstate.e_light_entity_list, rstate.e_light_count, e ) !=
         -1 ) {
        new_e = add_light_entity();
    }

    if ( new_e == -1 ) {
        ERROR_LOG( "entity not found in type lists" );
        return;
    }

    int model = rstate.entity_model_list[ e ];
    transform_t & t = rstate.entity_transform_list[ e ];

    rstate.entity_model_list[ new_e ] = model;
    rstate.entity_transform_list[ new_e ] = t;

    state.current_entity = new_e;
    rstate.hi_entity = new_e;
}

static void remove_current_entity()
{
    if ( state.current_entity == -1 ) return;

    int e = state.current_entity;

    // remove entity references
    int i = index_of( rstate.e_model_entity_list, rstate.e_model_count, e );
    if ( i != -1 ) {
        array_swap_last( rstate.e_model_entity_list, rstate.e_model_count, i );
        rstate.e_model_count--;
    }

    // remove entity references
    i = index_of( rstate.e_light_entity_list, rstate.e_light_count, e );
    if ( i != -1 ) {
        array_swap_last( rstate.e_light_entity_list, rstate.e_light_count, i );
        rstate.e_light_count--;
    }

    // remove entity
    array_swap_last( rstate.entity_model_list, rstate.entity_count, e );
    array_swap_last( rstate.entity_transform_list, rstate.entity_count, e );

    // reference last entity id with new id
    int last_index = rstate.entity_count - 1;
    int new_index = e;
    i = index_of(
        rstate.e_light_entity_list,
        rstate.e_light_count,
        last_index
    );
    if ( i != -1 ) rstate.e_light_entity_list[ i ] = new_index;

    i = index_of(
        rstate.e_model_entity_list,
        rstate.e_model_count,
        last_index
    );
    if ( i != -1 ) rstate.e_model_entity_list[ i ] = new_index;

    rstate.entity_count--;

    state.current_entity = -1;
    rstate.hi_entity = -1;
}

static void tick_current_entity_pos()
{
    if ( state.current_entity == -1 ) return;

    compute_camera_matrices();

    vec4 center_in_view{ 0.0f, 0.0f, -5.0f, 1.0f };
    vec4 center_in_world;
    glm_mat4_mulv( rstate.view_inverse, center_in_view, center_in_world );

    if ( state.enable_pos_snapping ) {
        float delta = state.pos_snapping_delta;
        center_in_world[ 0 ] = delta * floorf( center_in_world[ 0 ] / delta );
        center_in_world[ 1 ] = delta * floorf( center_in_world[ 1 ] / delta );
        center_in_world[ 2 ] = delta * floorf( center_in_world[ 2 ] / delta );
    }

    int e = state.current_entity;
    glm_vec3_copy( center_in_world, rstate.entity_transform_list[ e ].pos );
    rstate.entity_transform_list[ e ].update();
}

static void init()
{
    using string_t = char *;
    state.avail_model_file_list = new string_t[ 32 ];
    state.model_file_list = new string_t[ 32 ];
    setup_resource_list();

    state.enable_pos_snapping = false;
    state.pos_snapping_delta = 1.0f;

    state.enable_rot_snapping = true;
    state.rot_snapping_delta = 45;

    int miku_texture = load_texture( find_res( "colors_miku.png" ) );

    int fan_model = add_model( "SM_Exhaust_Fan.obj" );
    int miku_model = add_model( "miku.obj" );
    int floor_model = add_model( "SM_FloorTile.obj" );
    int light_model = add_model( "SM_Light.obj" );
    int doorway_model = add_model( "SM_Doorway.obj" );
    int ceiling_light_model = add_model( "SM_Ceiling_Light.obj" );

    rstate.light_model = light_model;

    rstate.model_texture_list[ miku_model ] = miku_texture;
    rstate.model_texture_list[ fan_model ] = miku_texture;
    rstate.model_texture_list[ floor_model ] = miku_texture;
    rstate.model_texture_list[ doorway_model ] = miku_texture;

    rstate.model_emission_list[ ceiling_light_model ][ 0 ] = 1.0f;
    rstate.model_emission_list[ ceiling_light_model ][ 1 ] = 1.0f;
    rstate.model_emission_list[ ceiling_light_model ][ 2 ] = 1.0f;

    {
        int e = add_model_entity();
        rstate.entity_model_list[ e ] = miku_model;
    }

    add_light_entity();

    static cgltf_options opt;
    cgltf_data * data = nullptr;
    cgltf_parse_file( &opt, "../../res/miku.glb", &data );
}

static void render_entity_properties()
{
    if ( state.current_entity == -1 ) return;

    transform_t & current_t =
        rstate.entity_transform_list[ state.current_entity ];

    ImGui::SeparatorText( "transform" );

    ImGui::RadioButton( "x axis", &state.current_axis, 0 );
    ImGui::SameLine();
    ImGui::RadioButton( "y axis", &state.current_axis, 1 );
    ImGui::SameLine();
    ImGui::RadioButton( "z axis", &state.current_axis, 2 );

    ImGui::Checkbox( "position snapping", &state.enable_pos_snapping );
    ImGui::BeginDisabled( !state.enable_pos_snapping );
    ImGui::InputFloat( "delta ##pos_delta", &state.pos_snapping_delta );
    ImGui::EndDisabled();
    ImGui::Checkbox( "rotation snapping", &state.enable_rot_snapping );
    ImGui::BeginDisabled( !state.enable_rot_snapping );
    ImGui::InputFloat( "delta ##rot_delta", &state.rot_snapping_delta );
    ImGui::EndDisabled();
    ImGui::Checkbox( "lock x", &state.enable_pos_lock_x );
    ImGui::SameLine();
    ImGui::Checkbox( "lock y", &state.enable_pos_lock_y );
    ImGui::SameLine();
    ImGui::Checkbox( "lock z", &state.enable_pos_lock_z );

    ImGui::InputFloat3( "position", current_t.pos );
    ImGui::InputFloat3( "rotation", current_t.rot );
    ImGui::InputFloat3( "scale", current_t.scale );
    current_t.update();

    ImGui::SeparatorText( "entity actions" );
    if ( ImGui::Button( "move" ) ) {
        state.move_mode = 1;
    }
    if ( ImGui::Button( "remove" ) ) {
        remove_current_entity();
    }
    if ( ImGui::Button( "duplicate" ) ) {
        duplicate_current_entity();
    }
}

static void render_edit_window()
{
    ImGui::Text( "fps = %f", 1.0f / state.tick_step );
    ImGui::SeparatorText( "add entity" );

    if ( ImGui::Button( "add light" ) ) {
        int e = add_light_entity();
        state.current_entity = e;
        rstate.hi_entity = e;
    }

    if ( ImGui::BeginListBox( "##model_file", ImVec2( -FLT_MIN, 0.0f ) ) ) {
        for ( int i = 0; i < state.avail_model_file_count; i++ ) {
            static char add_model_text[ 1024 ];
            snprintf(
                add_model_text,
                1024,
                "add %s",
                state.avail_model_file_list[ i ]
            );
            if ( ImGui::Selectable( add_model_text, false ) ) {
                int model = add_model( state.avail_model_file_list[ i ] );
                int e = add_model_entity();
                state.current_entity = e;
                rstate.entity_model_list[ e ] = model;
                rstate.hi_entity = e;
            }
        }
        ImGui::EndListBox();
    }

    if ( ImGui::Button( "refresh files" ) ) {
        setup_resource_list();
    }

    ImGui::SeparatorText( "render" );

    if ( ImGui::Button( "recompute shadows" ) ) {
        compute_all_shadow_maps();
    }

    ImGui::InputFloat( "shadow bias", &rstate.shadow_bias, 0.0, 0.0, "%f" );

    render_entity_properties();
}

static void loop()
{
    float time = hardware_time();

    state.tick_step = time - state.tick_time;
    state.render_step = state.tick_step;

    state.tick_time = time;
    state.render_time = time;

    vec3 forward;
    vec3 right;

    forward[ 0 ] = rstate.view_inverse[ 2 ][ 0 ];
    forward[ 1 ] = 0.0f;
    forward[ 2 ] = rstate.view_inverse[ 2 ][ 2 ];

    right[ 0 ] = rstate.view_inverse[ 0 ][ 0 ];
    right[ 1 ] = 0.0f;
    right[ 2 ] = rstate.view_inverse[ 0 ][ 2 ];

    glm_vec3_normalize( forward );
    glm_vec3_normalize( right );

    float s = 10.0f * state.tick_step;

    glm_vec3_muladds( forward, s * hardware_y_axis(), rstate.camera.pos );
    glm_vec3_muladds( right, s * hardware_x_axis(), rstate.camera.pos );

    rstate.camera.pos[ 1 ] += 10.0f * hardware_z_axis() * state.tick_step;
    rstate.camera.yaw = hardware_look_yaw();
    rstate.camera.pitch = hardware_look_pitch();

    int e_count;
    int * e_list = hardware_events( &e_count );
    for ( int i = 0; i < e_count; i++ ) {
        if ( e_list[ i ] == EVENT_ROTATE_CW ) {
            rotate_entity( -state.rot_snapping_delta );
        }
        if ( e_list[ i ] == EVENT_ROTATE_CCW ) {
            rotate_entity( state.rot_snapping_delta );
        }
        if ( e_list[ i ] == EVENT_SELECT_X ) state.current_axis = 0;
        if ( e_list[ i ] == EVENT_SELECT_Y ) state.current_axis = 1;
        if ( e_list[ i ] == EVENT_SELECT_Z ) state.current_axis = 2;
        if ( e_list[ i ] == EVENT_TOUCH ) pick_entity();
        if ( e_list[ i ] == EVENT_TOUCH ) state.move_mode = 0;
    }

    if ( state.move_mode ) {
        tick_current_entity_pos();
    }

    // update position locking
    if ( state.current_entity != -1 ) {
        vec3 & pos = rstate.entity_transform_list[ state.current_entity ].pos;

        if ( !state.enable_pos_lock_x ) state.locked_x = pos[ 0 ];
        if ( !state.enable_pos_lock_y ) state.locked_y = pos[ 1 ];
        if ( !state.enable_pos_lock_z ) state.locked_z = pos[ 2 ];

        if ( state.move_mode ) {
            if ( state.enable_pos_lock_x ) pos[ 0 ] = state.locked_x;
            if ( state.enable_pos_lock_y ) pos[ 1 ] = state.locked_y;
            if ( state.enable_pos_lock_z ) pos[ 2 ] = state.locked_z;

            rstate.entity_transform_list[ state.current_entity ].update();
        }
    }

    if ( state.move_mode ) {
        compute_all_shadow_maps();
    }

    render();

    static bool show_imgui = true;

    if ( show_imgui ) {
        ImGui::Begin(
            "MEOWGL",
            &show_imgui,
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
        );

        render_edit_window();

        ImGui::End();
    }
}

static void cJSON_AddVec3ToObject( cJSON * object, const char * name, vec3 v )
{
    cJSON * arr = cJSON_AddArrayToObject( object, name );
    cJSON_AddItemToArray( arr, cJSON_CreateNumber( v[ 0 ] ) );
    cJSON_AddItemToArray( arr, cJSON_CreateNumber( v[ 1 ] ) );
    cJSON_AddItemToArray( arr, cJSON_CreateNumber( v[ 2 ] ) );
}

static void write_map()
{
    cJSON * map = cJSON_CreateObject();
    cJSON_AddStringToObject( map, "name", "level 1" );
    cJSON_AddStringToObject( map, "color_palette", "colors_miku.png" );

    cJSON * entity_list = cJSON_AddArrayToObject( map, "entity_list" );
    cJSON * e_model_list = cJSON_AddArrayToObject( map, "e_model_list" );
    cJSON * e_light_list = cJSON_AddArrayToObject( map, "e_light_list" );

    for ( int i = 0; i < rstate.entity_count; i++ ) {
        int model = rstate.entity_model_list[ i ];
        transform_t & t = rstate.entity_transform_list[ i ];
        cJSON * entity = cJSON_CreateObject();
        cJSON_AddItemToArray( entity_list, entity );

        const char * model_name = state.model_file_list[ model ];
        cJSON_AddStringToObject( entity, "model", model_name );

        cJSON_AddVec3ToObject( entity, "pos", t.pos );
        cJSON_AddVec3ToObject( entity, "rot", t.rot );
        cJSON_AddVec3ToObject( entity, "scale", t.scale );
    }

    for ( int i = 0; i < rstate.e_model_count; i++ ) {
        cJSON_AddItemToArray(
            e_model_list,
            cJSON_CreateNumber( rstate.e_model_entity_list[ i ] )
        );
    }

    for ( int i = 0; i < rstate.e_light_count; i++ ) {
        cJSON_AddItemToArray(
            e_light_list,
            cJSON_CreateNumber( rstate.e_light_entity_list[ i ] )
        );
    }

    char * json = cJSON_Print( map );
    // printf( "%s\n", json );

    FILE * file = fopen( "../../res/map.json", "w" );
    fprintf( file, "%s", json );
    fclose( file );
}

static void
cJSON_GetVec3CaseSensitive( vec3 out, cJSON * object, const char * name )
{
    cJSON * value;
    cJSON * arr = cJSON_GetObjectItemCaseSensitive( object, name );
    int i = 0;
    cJSON_ArrayForEach( value, arr )
    {
        out[ i++ ] = (float) value->valuedouble;
    }
}

static void read_map()
{
    res_t res = find_res( "map.json" );

    if ( !res.data ) return;

    cJSON * map = cJSON_ParseWithLength( (char *) res.data, res.size );

    if ( !map ) {
        ERROR_LOG( "failed to parse map.json" );
        return;
    }

    // clear tables
    rstate.entity_count = 0;
    rstate.e_light_count = 0;
    rstate.e_model_count = 0;

    cJSON * entity_list =
        cJSON_GetObjectItemCaseSensitive( map, "entity_list" );
    cJSON * e_model_list =
        cJSON_GetObjectItemCaseSensitive( map, "e_model_list" );
    cJSON * e_light_list =
        cJSON_GetObjectItemCaseSensitive( map, "e_light_list" );
    cJSON * id;
    cJSON * entity;

    cJSON_ArrayForEach( entity, entity_list )
    {
        cJSON * model_json =
            cJSON_GetObjectItemCaseSensitive( entity, "model" );

        int model = add_model( model_json->valuestring );

        int e = add_entity();
        rstate.entity_model_list[ e ] = model;
        transform_t & t = rstate.entity_transform_list[ e ];
        cJSON_GetVec3CaseSensitive( t.pos, entity, "pos" );
        cJSON_GetVec3CaseSensitive( t.rot, entity, "rot" );
        cJSON_GetVec3CaseSensitive( t.scale, entity, "scale" );
        t.update();

        // INFO_LOG( "read %s", model_json->valuestring );
    }

    cJSON_ArrayForEach( id, e_model_list )
    {
        int e = cJSON_GetNumberValue( id );
        rstate.e_model_entity_list[ rstate.e_model_count++ ] = e;
    }

    cJSON_ArrayForEach( id, e_light_list )
    {
        int e = cJSON_GetNumberValue( id );
        rstate.e_light_entity_list[ rstate.e_light_count++ ] = e;
    }
}

#if defined( _WIN32 ) and RELEASE
int WinMain()
#else
int main()
#endif
{
    INFO_LOG( "meow" );

    hardware_init();

    render_init();

    init();

    read_map();

    compute_all_shadow_maps();

    hardware_set_loop( loop );

    write_map();

    hardware_destroy();

    return 0;
}
