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

struct string_t
{
    static constexpr unsigned MAX_STATIC_LENGTH = 15;
    static constexpr unsigned MAX_STATIC_SIZE = MAX_STATIC_LENGTH + 1;
    BXIAllocator* _allocator = nullptr;
    union 
    {
        char* _dynamic = nullptr;
        char _static[MAX_STATIC_SIZE];
    };

          char* c_str()       { return (_allocator) ? _dynamic : _static; }
    const char* c_str() const { return (_allocator) ? _dynamic : _static; }
};


namespace string
{
    void create( string_t* s, const char* data, BXIAllocator* allocator );
    void free( string_t* s );
}