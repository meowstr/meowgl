#pragma once

void logger_log(
    int level,
    const char * f_name,
    int line,
    const char * str,
    ...
) __attribute__( ( format( printf, 4, 5 ) ) );

#define DEBUG_LOG( ... ) logger_log( 0, __func__, __LINE__, __VA_ARGS__ );
#define INFO_LOG( ... )  logger_log( 1, __func__, __LINE__, __VA_ARGS__ );
#define ERROR_LOG( ... ) logger_log( 2, __func__, __LINE__, __VA_ARGS__ );
