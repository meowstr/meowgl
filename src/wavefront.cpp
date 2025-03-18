#include "wavefront.hpp"

#include "logging.hpp"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define MAX_VERTEX_COUNT         4096
// #define MAX_OBJ_COUNT            16
#define MAX_VERTEX_COUNT         100000
#define MAX_OBJ_COUNT            1000
#define MAX_NAME_LENGTH          64
#define MAX_POLYGON_LENGTH       32
#define MAX_LINE_LENGTH          256
#define MAX_MATERIAL_GROUP_COUNT 64

#define MAX_MATERIAL_COUNT 32

/// you might be asking why i would write the parser like this. tbh fuck u, idc
/// how flexible or modern the parser is. it needs to be fast, and simple.
/// theres a reasonable limit for everything. and if things go out of limits
/// theres errors printed.

static int starts_with( const char * line, const char * str )
{
    return strncmp( line, str, strlen( str ) ) == 0;
}

// line parsers [start] ////////////////////////////////////////////////////////

static int parse_object_line( char * out_name, const char * line )
{
    snprintf( out_name, MAX_NAME_LENGTH, "%s", line + strlen( "o " ) );
    return 0;
}

static int parse_material_lib_line( char * out_name, const char * line )
{
    snprintf( out_name, MAX_NAME_LENGTH, "%s", line + strlen( "mtllib " ) );
    return 0;
}

static int parse_usemtl_line( char * out_name, const char * line )
{
    snprintf( out_name, MAX_NAME_LENGTH, "%s", line + strlen( "usemtl " ) );
    return 0;
}

static int parse_position_line( float * out3, const char * line )
{
    int count = sscanf( line, "v %f %f %f", out3 + 0, out3 + 1, out3 + 2 );
    return count != 3;
}

static int parse_normal_line( float * out3, const char * line )
{
    int count = sscanf( line, "vn %f %f %f", out3 + 0, out3 + 1, out3 + 2 );
    return count != 3;
}

static int parse_uv_line( float * out2, const char * line )
{
    int count = sscanf( line, "vt %f %f", out2 + 0, out2 + 1 );
    return count != 2;
}

static int
parse_face_line( int * out_index_list, int * out_index_count, char * line )
{
    int len = strlen( line );
    int count = 0;

    for ( int i = 0; i < len; i++ ) {
        if ( line[ i ] == '/' ) line[ i ] = ' ';
    }

    for ( int i = 0; i < len; i++ ) {
        if ( line[ i ] != ' ' ) continue;

        if ( count >= MAX_POLYGON_LENGTH * 3 ) {
            ERROR_LOG( "polygon limit exceeded" );
            return 1;
        }

        out_index_list[ count ] = atoi( line + i ) - 1;
        count++;
    }

    *out_index_count = count;

    return count % 3 != 0;
}

static int parse_newmtl_line( char * out_name, const char * line )
{
    snprintf( out_name, MAX_NAME_LENGTH, "%s", line + strlen( "newmtl " ) );
    return 0;
}

static int parse_ns_line( float * out, const char * line )
{
    int count = sscanf( line, "Ns %f", out );
    return count != 1;
}

static int parse_ka_line( float * out3, const char * line )
{
    int count = sscanf( line, "Ka %f %f %f", out3 + 0, out3 + 1, out3 + 2 );
    return count != 3;
}

static int parse_kd_line( float * out3, const char * line )
{
    int count = sscanf( line, "Kd %f %f %f", out3 + 0, out3 + 1, out3 + 2 );
    return count != 3;
}

static int parse_ks_line( float * out3, const char * line )
{
    int count = sscanf( line, "Ks %f %f %f", out3 + 0, out3 + 1, out3 + 2 );
    return count != 3;
}

static int parse_ke_line( float * out3, const char * line )
{
    int count = sscanf( line, "Ke %f %f %f", out3 + 0, out3 + 1, out3 + 2 );
    return count != 3;
}

static int parse_ni_line( float * out, const char * line )
{
    int count = sscanf( line, "Ni %f", out );
    return count != 1;
}

static int parse_d_line( float * out, const char * line )
{
    int count = sscanf( line, "d %f", out );
    return count != 1;
}

static int parse_illum_line( int * out, const char * line )
{
    int count = sscanf( line, "illum %d", out );
    return count != 1;
}

static int parse_map_kd_line( char * out_name, const char * line )
{
    snprintf( out_name, MAX_NAME_LENGTH, "%s", line + strlen( "map_Kd " ) );
    return 0;
}

// line parsers [end] //////////////////////////////////////////////////////////

static void append_vertex(
    wavefront_t * out,
    float * pos_list,
    float * normal_list,
    float * uv_list,
    int * index_list,
    int index
)
{
    if ( out->vertex_count >= MAX_VERTEX_COUNT ) {
        ERROR_LOG( "vertex limit exceeded" );
        return;
    }

    int i = out->vertex_count;

    int pos_index = index_list[ index * 3 + 0 ];
    int uv_index = index_list[ index * 3 + 1 ];
    int normal_index = index_list[ index * 3 + 2 ];

    out->pos_list[ i * 3 + 0 ] = pos_list[ pos_index * 3 + 0 ];
    out->pos_list[ i * 3 + 1 ] = pos_list[ pos_index * 3 + 1 ];
    out->pos_list[ i * 3 + 2 ] = pos_list[ pos_index * 3 + 2 ];

    out->normal_list[ i * 3 + 0 ] = normal_list[ normal_index * 3 + 0 ];
    out->normal_list[ i * 3 + 1 ] = normal_list[ normal_index * 3 + 1 ];
    out->normal_list[ i * 3 + 2 ] = normal_list[ normal_index * 3 + 2 ];

    out->uv_list[ i * 2 + 0 ] = uv_list[ uv_index * 2 + 0 ];
    out->uv_list[ i * 2 + 1 ] = uv_list[ uv_index * 2 + 1 ];

    out->vertex_count++;
}

static void append_face(
    wavefront_t * out,
    float * pos_list,
    float * normal_list,
    float * uv_list,
    int * index_list,
    int index_count
)
{
    int poly_size = index_count / 3;

    // make triangle fan
    for ( int i = 0; i < poly_size - 2; i++ ) {
        int v1 = 0;
        int v2 = i + 1;
        int v3 = i + 2;

        append_vertex( out, pos_list, normal_list, uv_list, index_list, v1 );
        append_vertex( out, pos_list, normal_list, uv_list, index_list, v2 );
        append_vertex( out, pos_list, normal_list, uv_list, index_list, v3 );
    }
}

void wavefront_t::compute_bounds( float * min_vec3, float * max_vec3 )
{
    min_vec3[ 0 ] = FLT_MAX;
    min_vec3[ 1 ] = FLT_MAX;
    min_vec3[ 2 ] = FLT_MAX;

    max_vec3[ 0 ] = -FLT_MAX;
    max_vec3[ 1 ] = -FLT_MAX;
    max_vec3[ 2 ] = -FLT_MAX;

    for ( int i = 0; i < vertex_count; i++ ) {
        min_vec3[ 0 ] = fmin( min_vec3[ 0 ], pos_list[ i * 3 + 0 ] );
        min_vec3[ 1 ] = fmin( min_vec3[ 1 ], pos_list[ i * 3 + 1 ] );
        min_vec3[ 2 ] = fmin( min_vec3[ 2 ], pos_list[ i * 3 + 2 ] );

        max_vec3[ 0 ] = fmax( max_vec3[ 0 ], pos_list[ i * 3 + 0 ] );
        max_vec3[ 1 ] = fmax( max_vec3[ 1 ], pos_list[ i * 3 + 1 ] );
        max_vec3[ 2 ] = fmax( max_vec3[ 2 ], pos_list[ i * 3 + 2 ] );
    }
}

static void print_header( wavefront_t * out )
{
    float bounds[ 3 ];

    float min[ 3 ];
    float max[ 3 ];

    out->compute_bounds( min, max );
    bounds[ 0 ] = max[ 0 ] - min[ 0 ];
    bounds[ 1 ] = max[ 1 ] - min[ 1 ];
    bounds[ 2 ] = max[ 2 ] - min[ 2 ];

    INFO_LOG( "mesh description:" );
    if ( out->obj_count > 0 ) {
        INFO_LOG( "  name:         %s", out->obj_name_list[ 0 ] );
    }
    INFO_LOG( "  vertex count: %d", out->vertex_count );
    INFO_LOG( "  object count: %d", out->obj_count );
    INFO_LOG(
        "  bounds:       %.2f x %.2f x %.2f",
        bounds[ 0 ],
        bounds[ 1 ],
        bounds[ 2 ]
    );
}

static void print_mtl_header( material_t * out, const char * name )
{
    INFO_LOG( "material description:" );
    INFO_LOG( "  name:   %s", name );
    INFO_LOG( "  Ns:     %f", out->ns );
    INFO_LOG( "  Ka:     %f %f %f", out->ka[ 0 ], out->ka[ 1 ], out->ka[ 2 ] );
    INFO_LOG( "  Kd:     %f %f %f", out->kd[ 0 ], out->kd[ 1 ], out->kd[ 2 ] );
    INFO_LOG( "  Ks:     %f %f %f", out->ks[ 0 ], out->ks[ 1 ], out->ks[ 2 ] );
    INFO_LOG( "  Ke:     %f %f %f", out->ke[ 0 ], out->ke[ 1 ], out->ke[ 2 ] );
    INFO_LOG( "  Ni:     %f", out->ni );
    INFO_LOG( "  d:      %f", out->d );
    INFO_LOG( "  illum:  %d", out->illum );
    INFO_LOG( "  map_Kd: %s", out->map_kd ? out->map_kd : "(null)" );
}

int load_wavefront( wavefront_t * out, res_t res )
{
    char line[ MAX_LINE_LENGTH ];

    int cursor = 0;

    int errors = 0;

    // intialize output
    using c_string_t = const char *;
    out->pos_list = new float[ MAX_VERTEX_COUNT * 3 ];
    out->normal_list = new float[ MAX_VERTEX_COUNT * 3 ];
    out->uv_list = new float[ MAX_VERTEX_COUNT * 2 ];
    out->vertex_count = 0;
    out->obj_offset_list = new int[ MAX_OBJ_COUNT ];
    out->obj_name_list = new c_string_t[ MAX_OBJ_COUNT ];
    out->obj_count = 0;
    out->material_lib_filename = nullptr;
    out->material_group_material_list =
        new c_string_t[ MAX_MATERIAL_GROUP_COUNT ];
    out->material_group_offset_list = new int[ MAX_MATERIAL_GROUP_COUNT ];
    out->material_group_count = 0;

    // local storage
    float * pos_list = new float[ MAX_VERTEX_COUNT * 3 ];
    float * normal_list = new float[ MAX_VERTEX_COUNT * 3 ];
    float * uv_list = new float[ MAX_VERTEX_COUNT * 2 ];
    int pos_count = 0;
    int normal_count = 0;
    int uv_count = 0;

    while ( cursor < res.size ) {
        // load next line
        int i;
        for ( i = 0; i + cursor < res.size; i++ ) {
            char c = (char) res.data[ i + cursor ];

            if ( c == '\n' ) break;

            if ( i >= MAX_LINE_LENGTH ) {
                ERROR_LOG( "line length exceeded" );
                break;
            }

            line[ i ] = c;
        }
        cursor += i + 1;
        line[ i ] = '\0';

        // parse o command
        if ( starts_with( line, "o " ) ) {
            if ( out->obj_count >= MAX_OBJ_COUNT ) {
                ERROR_LOG( "object limit exceeded" );
                continue;
            }
            char * name = new char[ MAX_NAME_LENGTH ];
            errors += parse_object_line( name, line );
            out->obj_name_list[ out->obj_count ] = name;
            out->obj_count++;
        }

        // parse v command
        if ( starts_with( line, "v " ) ) {
            if ( pos_count >= MAX_VERTEX_COUNT * 3 ) {
                ERROR_LOG( "vertex limit exceeded" );
                continue;
            }
            float pos[ 3 ];
            errors += parse_position_line( pos, line );
            pos_list[ pos_count * 3 + 0 ] = pos[ 0 ];
            pos_list[ pos_count * 3 + 1 ] = pos[ 1 ];
            pos_list[ pos_count * 3 + 2 ] = pos[ 2 ];
            pos_count++;
        }

        // parse vn command
        if ( starts_with( line, "vn " ) ) {
            if ( normal_count >= MAX_VERTEX_COUNT * 3 ) {
                ERROR_LOG( "vertex limit exceeded" );
                continue;
            }
            float normal[ 3 ];
            errors += parse_normal_line( normal, line );
            normal_list[ normal_count * 3 + 0 ] = normal[ 0 ];
            normal_list[ normal_count * 3 + 1 ] = normal[ 1 ];
            normal_list[ normal_count * 3 + 2 ] = normal[ 2 ];
            normal_count++;
        }

        // parse vt command
        if ( starts_with( line, "vt " ) ) {
            if ( uv_count >= MAX_VERTEX_COUNT * 2 ) {
                ERROR_LOG( "vertex limit exceeded" );
                continue;
            }
            float uv[ 2 ];
            errors += parse_uv_line( uv, line );
            uv_list[ uv_count * 2 + 0 ] = uv[ 0 ];
            uv_list[ uv_count * 2 + 1 ] = 1.0f - uv[ 1 ]; // idk why this is flipped but yk
            uv_count++;
        }

        // parse f command
        if ( starts_with( line, "f " ) ) {
            int index_list[ MAX_POLYGON_LENGTH * 3 ];
            int index_count = 0;
            errors += parse_face_line( index_list, &index_count, line );
            append_face(
                out,
                pos_list,
                normal_list,
                uv_list,
                index_list,
                index_count
            );
        }

        if ( starts_with( line, "mtllib " ) ) {
            char * name = new char[ MAX_NAME_LENGTH ];
            errors += parse_material_lib_line( name, line );
            out->material_lib_filename = name;
        }

        if ( starts_with( line, "usemtl " ) ) {
            if ( out->material_group_count >= MAX_MATERIAL_GROUP_COUNT ) {
                ERROR_LOG( "material group count exceeded" );
                continue;
            }
            char * name = new char[ MAX_NAME_LENGTH ];
            errors += parse_usemtl_line( name, line );
            out->material_group_material_list[ out->material_group_count ] =
                name;
            out->material_group_offset_list[ out->material_group_count ] =
                out->vertex_count;
            out->material_group_count++;
        }
    }

    if ( errors ) {
        ERROR_LOG( "failed to load wavefront (.obj)" );
    }

    print_header( out );

    // stfu idc
    delete[] pos_list;
    delete[] normal_list;
    delete[] uv_list;

    return errors;
}

int load_material_lib( material_lib_t * out, res_t res )
{
    int errors = 0;
    int cursor = 0;
    char line[ MAX_LINE_LENGTH ];

    using c_string_t = const char *;
    out->material_name_list = new c_string_t[ MAX_MATERIAL_COUNT ];
    out->material_list = new material_t[ MAX_MATERIAL_COUNT ];
    out->material_count = 0;

    // zero out all the materials
    memset( out->material_list, 0, MAX_MATERIAL_COUNT * sizeof( material_t ) );

    while ( cursor < res.size ) {
        // load next line
        int i;
        for ( i = 0; i + cursor < res.size; i++ ) {
            char c = (char) res.data[ i + cursor ];

            if ( c == '\n' ) break;

            if ( i >= MAX_LINE_LENGTH ) {
                ERROR_LOG( "line length exceeded" );
                break;
            }

            line[ i ] = c;
        }
        cursor += i + 1;
        line[ i ] = '\0';

        // parse newmtl command
        if ( starts_with( line, "newmtl " ) ) {
            if ( out->material_count >= MAX_MATERIAL_COUNT ) {
                ERROR_LOG( "material count exceeded" );
                continue;
            }
            char * newmtl = new char[ MAX_NAME_LENGTH ];
            errors += parse_newmtl_line( newmtl, line );
            out->material_name_list[ out->material_count ] = newmtl;
            out->material_count++;
        }

        // ignore lines until we have a material
        if ( out->material_count <= 0 ) continue;

        // get current material
        material_t * mat = out->material_list + ( out->material_count - 1 );

        if ( starts_with( line, "Ns " ) ) {
            errors += parse_ns_line( &mat->ns, line );
        }
        if ( starts_with( line, "Ka " ) ) {
            errors += parse_ka_line( mat->ka, line );
        }
        if ( starts_with( line, "Kd " ) ) {
            errors += parse_kd_line( mat->kd, line );
        }
        if ( starts_with( line, "Ks " ) ) {
            errors += parse_ks_line( mat->ks, line );
        }
        if ( starts_with( line, "Ke " ) ) {
            errors += parse_ke_line( mat->ke, line );
        }
        if ( starts_with( line, "Ni " ) ) {
            errors += parse_ni_line( &mat->ni, line );
        }
        if ( starts_with( line, "d " ) ) {
            errors += parse_d_line( &mat->d, line );
        }
        if ( starts_with( line, "illum " ) ) {
            errors += parse_illum_line( &mat->illum, line );
        }
        if ( starts_with( line, "map_Kd " ) ) {
            char * filename = new char[ MAX_NAME_LENGTH ];
            errors += parse_map_kd_line( filename, line );
            mat->map_kd = filename;
        }
    }

    if ( errors ) {
        ERROR_LOG( "failed to load wavefront (.mtl)" );
    }

    for ( int i = 0; i < out->material_count; i++ ) {
        print_mtl_header(
            out->material_list + i,
            out->material_name_list[ i ]
        );
    }

    return errors;
}
