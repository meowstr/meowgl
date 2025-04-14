#include "res.hpp"
#include "logging.hpp"

#include <stdio.h>

res_t find_res( const char * name )
{
    res_t res;

    res.data = nullptr;
    res.size = 0;

    char path[ 1024 ];

#ifdef EMSCRIPTEN
    snprintf( path, 1024, "./%s", name );
#else
    snprintf( path, 1024, "../../res/%s", name );
#endif

    FILE * file = fopen( path, "rb" );

    if ( !file ) {
        ERROR_LOG( "failed to find resource: %s", name );
        return res;
    }

    fseek( file, 0, SEEK_END );
    int size = ftell( file );
    fseek( file, 0, SEEK_SET );

    res.data = new unsigned char[ size ];
    res.size = size;

    fread( res.data, sizeof( unsigned char ), size, file );

    fclose( file );

    return res;
}
