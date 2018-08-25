#pragma once

#include <foundation/string_util.h>

struct BXIAllocator;
struct BXIFilesystem;

namespace common
{
    void RefreshFiles( string_buffer_t* flist, const char* folder, BXIFilesystem* fs, BXIAllocator* allocator );
    string_buffer_it MenuFileSelector( const string_buffer_t& file_list );
}

