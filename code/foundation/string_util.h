#pragma once

struct BXIAllocator;
namespace string
{
    char*    token    ( char* str, char* tok, size_t toklen, char* delim );
    char*    duplicate( char* old_string, const char* new_string, BXIAllocator* allocator );
    char*    create   ( const char* str, size_t len, BXIAllocator* allocator );
    int      append   ( char* buffer, int size_buffer, const char* format, ... );
    unsigned count    ( const char* str, const char c );
    unsigned length   ( const char* str );
    void     free     ( char* str, BXIAllocator* allocator );
    bool     equal    ( const char* str0, const char* str1 );
    char*    find     ( const char* str, const char* to_find, char** next = 0 );

    inline void free_and_null( char** str, BXIAllocator* allocator ) { string::free( str[0], allocator ); str[0] = 0; }
}
