#include "hardware.hpp"
#include "logging.hpp"
#include "render.hpp"
#include "state.hpp"

#include <cglm/affine.h>
#include <cglm/mat4.h>
#include <imgui.h>

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
static void setup_resource_list()
{
}
#endif

static void tick_current_entity_pos()
{
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

    int miku_model = add_model( "miku.obj" );
    int floor_model = add_model( "SM_FloorTile.obj" );

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

    transform_t & current_t =
        rstate.entity_list[ state.current_entity ].transform;

    int e_count;
    int * e_list = hardware_events( &e_count );
    for ( int i = 0; i < e_count; i++ ) {
        if ( e_list[ i ] == EVENT_ROTATE_CW ) {
            current_t.rot[ state.current_axis ] -= state.rot_snapping_delta;
            current_t.update();
        }
        if ( e_list[ i ] == EVENT_ROTATE_CCW ) {
            current_t.rot[ state.current_axis ] += state.rot_snapping_delta;
            current_t.update();
        }
        if ( e_list[ i ] == EVENT_SELECT_X ) state.current_axis = 0;
        if ( e_list[ i ] == EVENT_SELECT_Y ) state.current_axis = 1;
        if ( e_list[ i ] == EVENT_SELECT_Z ) state.current_axis = 2;
    }

    tick_current_entity_pos();

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
                }
            }
            ImGui::EndListBox();
        }

        if ( ImGui::Button( "refresh" ) ) {
            setup_resource_list();
        }

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

        ImGui::End();
    }

    // ImGui::ShowDemoWindow();
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

    hardware_set_loop( loop );

    hardware_destroy();

    return 0;
}
