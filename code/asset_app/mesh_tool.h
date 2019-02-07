#pragma once

#include <foundation/string_util.h>
#include <asset_compiler/mesh/mesh_compiler.h>
#include <gfx/gfx.h>
#include <entity/entity_id.h>

#include "components.h"
#include "tool_context.h"
#include "foundation/serializer.h"

struct CMNEngine;

struct FolderContext
{
    void SetUp( BXIFilesystem* fs, BXIAllocator* allocator );

    void RequestRefresh();
    void SetFolder( const char* folder );
    
    const string_buffer_t& FileList() const;
    const char* Folder() const { return _folder.c_str(); }

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
    void StartUp( CMNEngine* e, const char* src_root, const char* dst_root, BXIAllocator* allocator );
    void ShutDown( CMNEngine* e );
    void Tick( CMNEngine* e, const TOOLContext& ctx );
    
    bool _Import( CMNEngine* e );
    void _Compile( CMNEngine* e, const TOOLContext& ctx, const tool::mesh::Streams& streams );
    void _Load( CMNEngine* e, const TOOLContext& ctx, const char* filename );
    void _Save( CMNEngine* e );
    
    void _UnloadComponents( CMNEngine* e );
    void _LoadComponents( CMNEngine* e, const TOOLContext& ctx, const RDIXMeshFile* mesh_file );

    BXIAllocator* _allocator = nullptr;
    FolderContext _root_src_folder_ctx;
    string_t _current_src_file;

    FolderContext _root_dst_folder_ctx;
    string_t _current_dst_file;

    ECSComponentID _id_mesh_comp;
    ECSComponentID _id_mesh_desc_comp;
    ECSComponentID _id_skinning_comp;

    tool::mesh::StreamsArray _loaded_streams;
    tool::mesh::CompileOptions _compile_options;
    tool::mesh::ImportOptions _import_options;

    srl_file_t* _mesh_file = nullptr;

    union 
    {
        uint32_t all = 0;
        struct
        {
            uint32_t show_import_dialog : 1;
        };
    } _flags;
};
