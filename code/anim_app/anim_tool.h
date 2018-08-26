#pragma once

#include <foundation/type.h>
#include <foundation/string_util.h>
#include <filesystem/filesystem_plugin.h>

#include <anim_compiler/anim_compiler.h>

struct BXIAllocator;

struct AnimTool
{
    void StartUp( const char* src_folder, const char* dst_folder, BXIAllocator* allocator );
    void ShutDown();

    void Tick( BXIFilesystem* fs );
    void DrawMenu();

    BXIAllocator* _allocator = nullptr;

    tool::anim::Skeleton* _in_skel = nullptr;
    tool::anim::Animation* _in_anim = nullptr;

    blob_t _skel_blob = {};
    blob_t _clip_blob = {};

    string_t _src_folder;
    string_t _dst_folder;
    string_t _current_file;
    BXFileHandle _hfile = {};
    
    string_buffer_t _src_file_list;
    string_buffer_t _dst_file_list;

    union 
    {
        uint32_t all = 0;
        struct  
        {
            uint32_t refresh_src_files : 1;
            uint32_t refresh_dst_files : 1;
            uint32_t request_load : 1;
            uint32_t loading_io : 1;
        };
    } _flags;

};
