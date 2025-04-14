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

#include <stdio.h>

#ifdef __unix__

#include <dirent.h>
#include <sys/types.h>

static void setup_resource_list()
{
    state.model_file_count = 0;

    dirent * p;
    DIR * d = opendir( "../res/" );
    if ( d ) {
        while ( ( p = readdir( d ) ) ) {
            if ( p->d_type == DT_DIR ) continue;
            if ( strstr( p->d_name, ".obj" ) == nullptr ) continue;
            if ( state.model_file_count >= 32 ) break;
            INFO_LOG( "file: %s", p->d_name );

            state.model_file_list[ state.model_file_count++ ] =
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

        state.model_file_list[ state.model_file_count++ ] =
            strdup( fdFile.cFileName );

    } while ( FindNextFile( hFind, &fdFile ) ); // Find the next file.

    FindClose( hFind ); // Always, Always, clean things up!
}
#endif

static float intersect_entity( int e, vec3 origin, vec3 direction )
{
    // bool glm_ray_triangle(vec3 origin, vec3 direction, vec3 v0, vec3 v1, vec3
    // v2, float *d)

    int model_id = rstate.entity_list[ e ].model;

    wavefront_t & model = rstate.model_list[ model_id ];
    mat4 & t = rstate.entity_list[ e ].transform.m;

    float closest_distance = FLT_MAX;

    for ( int i = 0; i < model.vertex_count / 3; i++ ) {
        vec4 v0;
        vec4 v1;
        vec4 v2;

        v0[ 3 ] = 1;
        v1[ 3 ] = 1;
        v2[ 3 ] = 1;

        float distance = 0.0f;

        glm_vec3_copy( model.pos_list + 3 * ( i * 3 + 0 ), v0 );
        glm_vec3_copy( model.pos_list + 3 * ( i * 3 + 1 ), v1 );
        glm_vec3_copy( model.pos_list + 3 * ( i * 3 + 2 ), v2 );

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

static void remove_current_entity()
{
    if ( state.current_entity == -1 ) return;

    array_swap_last(
        rstate.entity_list,
        rstate.entity_count,
        state.current_entity
    );
    rstate.entity_count--;

    state.current_entity = -1;
    rstate.hi_entity = -1;
}

static void tick_current_entity_pos()
{
    if ( state.current_entity == -1 ) return;

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
    glm_vec3_copy( center_in_world, rstate.entity_list[ e ].transform.pos );
    rstate.entity_list[ e ].transform.update();
}

static void init()
{
    using string_t = const char *;
    state.model_file_list = new string_t[ 32 ];
    setup_resource_list();

    state.enable_pos_snapping = false;
    state.pos_snapping_delta = 1.0f;

    int fan_model = add_model( "SM_Exhaust_Fan.obj" );
    int miku_model = add_model( "miku.obj" );
    int floor_model = add_model( "SM_FloorTile.obj" );
    int miku_texture = load_texture( find_res( "colors_miku.png" ) );
    int light_model = add_model( "SM_Ceiling_Light.obj" );

    rstate.model_texture_list[ miku_model ] = miku_texture;
    rstate.model_texture_list[ fan_model ] = miku_texture;
    rstate.model_texture_list[ floor_model ] = miku_texture;

    rstate.model_emission_list[ light_model ][ 0 ] = 1.0f;
    rstate.model_emission_list[ light_model ][ 1 ] = 1.0f;
    rstate.model_emission_list[ light_model ][ 2 ] = 1.0f;

    auto & model = rstate.model_list[ miku_model ];

    int miku1 = add_entity( miku_model );

    add_entity( floor_model, 0, 0, 0 );
    add_entity( floor_model, 2, 0, 0 );
    add_entity( floor_model, -2, 0, 0 );
    add_entity( floor_model, 2, 0, 2 );
    add_entity( floor_model, -2, 0, 2 );
    add_entity( floor_model, 0, 0, 2 );
    add_entity( floor_model, 2, 0, -2 );
    add_entity( floor_model, -2, 0, -2 );
    add_entity( floor_model, 0, 0, -2 );
}

static bool imgui_axis_button( const char * name, bool highlight )
{
    if ( highlight ) {
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4( 0.8f, 0.0f, 0.0f, 1.0f )
        );
        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4( 0.9f, 0.0f, 0.0f, 1.0f )
        );
        ImGui::PushStyleColor(
            ImGuiCol_ButtonActive,
            ImVec4( 1.0f, 0.0f, 0.0f, 1.0f )
        );
    }
    bool click = ImGui::Button( name );
    if ( highlight ) {
        ImGui::PopStyleColor( 3 );
    }

    return click;
}

static void rotate_entity( float dtheta )
{
    if ( state.current_entity == -1 ) return;

    transform_t & current_t =
        rstate.entity_list[ state.current_entity ].transform;

    current_t.rot[ state.current_axis ] += dtheta;
    current_t.update();
}

static void duplicate_current_entity()
{
    if ( state.current_entity == -1 ) return;

    int model = rstate.entity_list[ state.current_entity ].model;
    transform_t & t = rstate.entity_list[ state.current_entity ].transform;

    int e = add_entity( model );
    state.current_entity = e;
    rstate.hi_entity = e;

    rstate.entity_list[ e ].transform = t;
}

static void imgui_render_entity_properties()
{
    if ( state.current_entity == -1 ) return;

    transform_t & current_t =
        rstate.entity_list[ state.current_entity ].transform;

    ImGui::SeparatorText( "transform" );
    if ( imgui_axis_button( "X", state.current_axis == 0 ) )
        state.current_axis = 0;
    ImGui::SameLine();
    if ( imgui_axis_button( "Y", state.current_axis == 1 ) )
        state.current_axis = 1;
    ImGui::SameLine();
    if ( imgui_axis_button( "Z", state.current_axis == 2 ) )
        state.current_axis = 2;
    ImGui::Checkbox( "position snapping", &state.enable_pos_snapping );
    ImGui::BeginDisabled( !state.enable_pos_snapping );
    ImGui::InputFloat( "delta ##pos_delta", &state.pos_snapping_delta );
    ImGui::EndDisabled();
    ImGui::Checkbox( "rotation snapping", &state.enable_rot_snapping );
    ImGui::BeginDisabled( !state.enable_rot_snapping );
    ImGui::InputFloat( "delta ##rot_delta", &state.rot_snapping_delta );
    ImGui::EndDisabled();

    ImGui::InputFloat3( "position", current_t.pos );
    ImGui::InputFloat3( "rotation", current_t.rot );
    ImGui::InputFloat3( "scale", current_t.scale );
    current_t.update();

    ImGui::SeparatorText( "entity" );
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

    update_camera();

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

    render();

    static bool show_imgui = true;

    if ( show_imgui ) {
        ImGui::Begin(
            "MEOWGL",
            &show_imgui,
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
        );

        ImGui::Text( "fps = %f", 1.0f / state.tick_step );
        ImGui::SeparatorText( "add model" );

        if ( ImGui::BeginListBox( "##model_file", ImVec2( -FLT_MIN, 0.0f ) ) ) {
            for ( int i = 0; i < state.model_file_count; i++ ) {
                if ( ImGui::Selectable( state.model_file_list[ i ], false ) ) {
                    int model = add_model( state.model_file_list[ i ] );
                    state.current_entity = add_entity( model );
                    rstate.hi_entity = state.current_entity;
                }
            }
            ImGui::EndListBox();
        }

        if ( ImGui::Button( "refresh" ) ) {
            setup_resource_list();
        }

        imgui_render_entity_properties();

        ImGui::End();
    }

    // ImGui::ShowDemoWindow();
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
    for ( int i = 0; i < rstate.entity_count; i++ ) {
        entity_t & e = rstate.entity_list[ i ];
        cJSON * entity = cJSON_CreateObject();
        cJSON_AddItemToArray( entity_list, entity );

        const char * model_name = rstate.model_list[ e.model ].filename;
        cJSON_AddStringToObject( entity, "model", model_name );

        cJSON_AddVec3ToObject( entity, "pos", e.transform.pos );
        cJSON_AddVec3ToObject( entity, "rot", e.transform.rot );
        cJSON_AddVec3ToObject( entity, "scale", e.transform.scale );
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

    rstate.entity_count = 0;

    cJSON * entity_list =
        cJSON_GetObjectItemCaseSensitive( map, "entity_list" );

    cJSON * entity;
    cJSON_ArrayForEach( entity, entity_list )
    {
        cJSON * model_json =
            cJSON_GetObjectItemCaseSensitive( entity, "model" );

        int model = add_model( model_json->valuestring );
        int e = add_entity( model );
        transform_t & t = rstate.entity_list[ e ].transform;
        cJSON_GetVec3CaseSensitive( t.pos, entity, "pos" );
        cJSON_GetVec3CaseSensitive( t.rot, entity, "rot" );
        cJSON_GetVec3CaseSensitive( t.scale, entity, "scale" );
        t.update();

        // INFO_LOG( "read %s", model_json->valuestring );
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

    hardware_set_loop( loop );

    write_map();

    hardware_destroy();

    return 0;
}
