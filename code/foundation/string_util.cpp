#include "string_util.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "type.h"
#include "debug.h"
#include "memory/memory.h"

char* string::token( char* str, char* tok, size_t toklen, char* delim )
{
    if( !str )
    {
        tok[0] = 0;
        return 0;
    }
    str += strspn( str, delim );
    size_t n = strcspn( str, delim );
    n = ( toklen - 1 < n ) ? toklen - 1 : n;
    memcpy( tok, str, n );
    tok[n] = 0;
    str += n;
    return ( *str == 0 ) ? 0 : str;
}

char* string::duplicate( char* old_string, const char* new_string, BXIAllocator* allocator )
{
    const size_t new_len = strlen( new_string );
    if( old_string )
    {
        const size_t old_len = strlen( old_string );
        if( old_len != new_len )
        {
            BX_FREE0( allocator, old_string );
            old_string = ( char* )BX_MALLOC( allocator, new_len + 1, 1 );
        }
    }
    else
    {
        old_string = (char*)BX_MALLOC( allocator, new_len + 1, 1 );
    }

    strcpy_s( old_string, new_len+1, new_string );
    return old_string;
}

void string::free( char* str, BXIAllocator* allocator )
{
    BX_FREE( allocator, str );
}

char* string::create( const char* str, size_t len, BXIAllocator* allocator )
{
    SYS_ASSERT( strlen( str ) >= len );
    char* out = (char*)BX_MALLOC( allocator, (unsigned)len + 1, 1 ); //memory_alloc( len + 1 );
    out[len] = 0;
    strncpy_s( out, len, str, len );

    return out;
}

unsigned string::count( const char* str, const char c )
{
    unsigned count = 0;
    while( *str )
    {
        const char strc = *str;
        count += strc == c;

        ++str;
    }

    return count;
}
unsigned string::length( const char* str )
{
    return ( str ) ? (unsigned)strlen( str ) : 0;
}
bool string::equal( const char* str0, const char* str1 )
{
    return strcmp( str0, str1 ) == 0;
}

int string::append( char* buffer, int size_buffer, const char* format, ... )
{
    const size_t N = 1024;
    char str_format[N] = {0};
    
    if( format )
    {
        va_list arglist;
        va_start( arglist, format );
        vsprintf_s( str_format, N, format, arglist );
        va_end( arglist );
    }

    const int len_buff = (int)strlen( buffer );

    if( strlen( str_format ) == 0 )
    {
        return len_buff;
    }

    int len_left = size_buffer - len_buff;
    if( len_left > 0 )
    {
        sprintf_s( buffer + len_buff, len_left, str_format );
        len_left = size_buffer - (int)strlen( buffer );
    }
    return len_left;
}

char* string::find( const char* str, const char* to_find, char** next /*= 0 */ )
{
    char* result = (char*)strstr( str, to_find );
    if( result && next )
    {
        next[0] = (char*)( str + strlen( to_find ) );
    }

    return result;
}

string_t::string_t( const char* str )
{
    SYS_ASSERT( string::length( str ) <= MAX_STATIC_LENGTH );
    string::create( this, str, nullptr );
}


namespace string
{
    void string::create( string_t* s, const char* data, BXIAllocator* allocator )
    {
        const uint32_t len = length( data );
        if( len > string_t::MAX_STATIC_LENGTH )
        {
            s->_dynamic = string::duplicate( s->_dynamic, data, allocator );
            s->_allocator = allocator;
        }
        else
        {
            if( s->_allocator )
            {
                string::free_and_null( &s->_dynamic, s->_allocator );
            }
            s->_allocator = nullptr;
            strcpy( s->_static, data );
        }
    }

    void string::free( string_t* s )
    {
        if( s->_allocator )
        {
            string::free_and_null( &s->_dynamic, s->_allocator );
        }
        memset( s, 0x00, sizeof( string_t ) );
    }


}

