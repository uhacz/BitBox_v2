#pragma once

#include <foundation/string_util.h>
#include <asset_compiler/mesh/mesh_compiler.h>
#include <gfx/gfx.h>
#include <entity/entity_id.h>

#include "components.h"
#include "tool_context.h"

struct CMNEngine;

struct FolderContext
{
    void SetUp( BXIFilesystem* fs, BXIAllocator* allocator );

    void RequestRefresh();
    void SetFolder( const char* folder );
    const string_buffer_t& FileList() const;

private:
    string_t _current_file;
    string_t _folder;

    mutable string_buffer_t _file_list;
    mutable uint8_t _flag_refresh_file_list = 0;

    BXIFilesystem* _filesystem = nullptr;
    BXIAllocator* _allocator = nullptr;
};

struct MESHTool
{
    void StartUp( CMNEngine* e, const char* asset_root, BXIAllocator* allocator );
    void ShutDown( CMNEngine* e );
    void Tick( CMNEngine* e, const TOOLContext& ctx );
    
    tool::mesh::Streams* _Import( CMNEngine* e );
    void _Compile( CMNEngine* e, const TOOLContext& ctx );
    
    BXIAllocator* _allocator = nullptr;
    FolderContext _root_folder_ctx;
    string_t _current_file;

    ECSComponentID _id_mesh_comp;

    tool::mesh::Streams* _loaded_streams = nullptr;
    tool::mesh::CompileOptions _compile_options;
};
