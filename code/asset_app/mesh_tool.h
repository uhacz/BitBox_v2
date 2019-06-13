#pragma once

#include <foundation/string_util.h>
#include <asset_compiler/mesh/mesh_compiler.h>
#include <gfx/gfx.h>
#include <entity/entity_id.h>

#include "components.h"
#include "tool_context.h"
#include "foundation/serializer.h"
#include "common/common.h"
#include "util/grid.h"
#include "util/bbox.h"

struct CMNEngine;


struct VoxelizedMesh
{
    AABB bounds;
    f32 voxel_size;
    grid_t grid;
    array_t<u8> voxels;

    VoxelizedMesh()
    {
        Clear();
    }

    void Clear();
};

struct MESHTool : TOOLInterface
{
    void StartUp( CMNEngine* e, const char* src_root, BXIAllocator* allocator ) override;
    void ShutDown( CMNEngine* e ) override;
    void Tick( CMNEngine* e, const TOOLContext& ctx, float dt ) override;
    
    bool _Import( CMNEngine* e );
    void _Compile( CMNEngine* e, const TOOLContext& ctx, const tool::mesh::Streams& streams );
    void _Load( CMNEngine* e, const TOOLContext& ctx, const char* filename );
    void _Save( CMNEngine* e, common::FolderContext* folder );
    
    BXIAllocator* _allocator = nullptr;

    common::FolderContext _root_src_folder_ctx;
    string_t _current_src_file;
    string_t _current_dst_file;

    ECSComponentID _id_mesh_comp;

    tool::mesh::StreamsArray _loaded_streams;
    tool::mesh::CompileOptions _compile_options;
    tool::mesh::ImportOptions _import_options;

    srl_file_t* _mesh_file = nullptr;

    //VoxelizedMesh _vox_mesh;

    union 
    {
        uint32_t all = 0;
        struct
        {
            uint32_t show_import_dialog : 1;
        };
    } _flags;
};
