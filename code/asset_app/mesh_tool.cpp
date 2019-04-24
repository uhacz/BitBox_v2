#include "mesh_tool.h"
#include <3rd_party/imgui/imgui.h>

#include <foundation/array.h>
#include <entity/entity_system.h>


#include "common/base_engine_init.h"
#include "components.h"
#include "filesystem/filesystem_plugin.h"
#include "rdix/rdix_type.h"
#include "rdix/rdix.h"

#include "common/common.h"
#include "foundation/hashed_string.h"
#include "rdix/rdix_debug_draw.h"

#include "util/voxelizer/voxelize.h"

namespace helper
{
    static void SetToDefaults( tool::mesh::CompileOptions* opt )
    {
        opt[0].slot_mask = 0;
        opt[0].AddSlot( RDIEVertexSlot::POSITION )
              .AddSlot( RDIEVertexSlot::NORMAL )
              .AddSlot( RDIEVertexSlot::TEXCOORD0 )
              .AddSlot( RDIEVertexSlot::BLENDWEIGHT )
              .AddSlot( RDIEVertexSlot::BLENDINDICES );
    }

    static void SetToDefaults( tool::mesh::ImportOptions* opt, const char* filename )
    {
        if( string::find( filename, ".fbx" ) )
        {
            opt->scale = vec3_t( 0.01f );
        }
        else
        {
            opt->scale = vec3_t( 1.f );
        }
    }
}//

void MESHTool::StartUp( CMNEngine* e, const char* src_root, BXIAllocator* allocator )
{
    _allocator = allocator;
    _root_src_folder_ctx.SetUp( e->filesystem, allocator );
    _root_src_folder_ctx.SetFolder( src_root );

    helper::SetToDefaults( &_compile_options );
}

void MESHTool::ShutDown( CMNEngine* e )
{
    UnloadTOOLMeshComponent( e, _id_mesh_comp );
    BX_FREE0( _allocator, _mesh_file );
    _allocator = nullptr;
    
}

void MESHTool::Tick( CMNEngine* e, const TOOLContext& ctx, float dt )
{
    if( ImGui::Begin( "MeshTool" ) )
    {
        if( ImGui::BeginMenu( "File" ) )
        {
            if( ImGui::MenuItem( "New" ) )
            {
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if( ImGui::BeginMenu( "Load" ) )
            {
                const string_buffer_t& file_list = ctx.folders->mesh.FileList();
                string_buffer_it selected_it = common::MenuFileSelector( file_list );
                if( !selected_it.null() )
                {
                    _Load( e, ctx, selected_it.pointer );
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if( ImGui::BeginMenu( "Import" ) )
            {
                const string_buffer_t& file_list = _root_src_folder_ctx.FileList();
                string_buffer_it selected_it = common::MenuFileSelector( file_list );
                if( !selected_it.null() )
                {
                    const bool the_same_file = string::equal( selected_it.pointer, _current_src_file.c_str() );
                    if( !the_same_file )
                    {
                        string::create( &_current_src_file, selected_it.pointer, _allocator );
                        helper::SetToDefaults( &_import_options, _current_src_file.c_str() );
                    }
                    _flags.show_import_dialog = 1;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            ImGui::EndMenu();
        }
        ImGui::Separator();
        if( !_loaded_streams.empty() )
        {
            for( const tool::mesh::Streams& streams : _loaded_streams )
            {
                if( ImGui::TreeNode( streams.name.c_str() ) )
                {
                    if( ImGui::Button( "Compile" ) )
                    {
                        _Compile( e, ctx, streams );
                    }
                    if( _current_src_file.c_str() && _mesh_file )
                    {
                        ImGui::SameLine();
                        if( ImGui::Button( "Save" ) )
                        {
                            _Save( e, &ctx.folders->mesh );
                        }
                    }


                    ImGui::Separator();
                    if( ImGui::TreeNodeEx( "Stats", ImGuiTreeNodeFlags_DefaultOpen ) )
                    {
                        ImGui::Value( "Num vertices", streams.num_vertices );
                        ImGui::Value( "Num indices" , streams.num_indices );
                        ImGui::Value( "Num bones"   , streams.num_bones );
                        ImGui::TreePop();
                    }

                    if( ImGui::TreeNodeEx( "Loaded streams", ImGuiTreeNodeFlags_DefaultOpen ) )
                    {
                        for( uint32_t islot = 0; islot < RDIEVertexSlot::COUNT; ++islot )
                        {
                            const RDIVertexBufferDesc desc = streams.slots[islot];
                            if( !desc.hash )
                                continue;

                            bool has_slot = _compile_options.HasSlot( islot );
                            char slot_name[127];
                            if( RDIEVertexSlot::ToString( slot_name, 127, (RDIEVertexSlot::Enum)islot ) )
                            {   
                                if( ImGui::Checkbox( slot_name, &has_slot ) )
                                {
                                    _compile_options.EnableSlot( (RDIEVertexSlot::Enum)islot, has_slot );
                                }
                            }
                        }
                        ImGui::TreePop();
                    }
                    if( ImGui::TreeNode( "Bones" ) )
                    {
                        for( const std::string& name : streams.bones_names )
                        {
                            ImGui::Text( name.c_str() );
                        }
                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::End();
    
    if( _flags.show_import_dialog )
    {
        ImGui::OpenPopup( "ImportOptions" );
        if( ImGui::BeginPopup( "ImportOptions" ) )
        {
            ImGui::InputFloat3( "Scale", _import_options.scale.xyz, 4 );

            if( ImGui::Button( "Import" ) )
            {
                if( _Import( e ) )
                {
                    helper::SetToDefaults( &_compile_options );

                    BX_FREE0( _allocator, _mesh_file );
                    _flags.show_import_dialog = 0;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
    }

    if( !array::empty( _vox_mesh.voxels ) )
    {
        auto proxy = common::FindFirstTransformComponent( e->ecs, ctx.entity );

        const vec3_t base_pos = proxy->pose.translation();

        const vec3_t bounds_ext( 1.f );
        RDIXDebug::AddAABB( base_pos + vec3_t(0.f, bounds_ext.y, 0.f), bounds_ext );

        const vec3_t voxel_offset = base_pos + _vox_mesh.bounds.pmin;

        for( u32 iz = 0; iz < _vox_mesh.grid.depth; ++iz )
        {
            for( u32 iy = 0; iy < _vox_mesh.grid.height; ++iy )
            {
                for( u32 ix = 0; ix < _vox_mesh.grid.width; ++ix )
                {
                    const u32 index = _vox_mesh.grid.Index( ix, iy, iz );
                    if( _vox_mesh.voxels[index] == 0 )
                        continue;

                    vec3_t pos( (f32)ix, (f32)iy, (f32)iz );
                    pos *= _vox_mesh.voxel_size;
                    pos += voxel_offset;

                    RDIXDebug::AddAABB( pos, vec3_t( _vox_mesh.voxel_size * 0.5f ) );
                }
            }
        }
    }


}

bool MESHTool::_Import( CMNEngine* e )
{
    const char* filename = _current_src_file.c_str();

    BXFileWaitResult result = e->filesystem->LoadFileSync( e->filesystem, filename, BXEFIleMode::BIN, _allocator );
    
    bool success = false;

    tool::mesh::StreamsArray streams = tool::mesh::Import( result.file.pointer, result.file.size, _import_options );
    if( !streams.empty() )
    {
        _loaded_streams.clear();
        _loaded_streams = std::move( streams );
        success = true;
    }
    e->filesystem->CloseFile( &result.handle );

    return success;
}

void MESHTool::_Compile( CMNEngine* e, const TOOLContext& ctx, const tool::mesh::Streams& streams )
{
    blob_t cmesh = tool::mesh::Compile( streams, _compile_options, _allocator );
    if( !cmesh.empty() )
    {
        RDIXMeshFile* mesh_file = (RDIXMeshFile*)cmesh.raw;

        _id_mesh_comp = LoadTOOLMeshComponent( e, _id_mesh_comp, ctx, mesh_file, _allocator );

        { // file to serialize
            BX_FREE0( _allocator, _mesh_file );
            _mesh_file = srl_file::serialize<RDIXMeshFile>( cmesh, _allocator );
        }
    }

    cmesh.destroy();
}

void VoxelizeMesh( VoxelizedMesh* output, const RDIXMeshFile* mesh, f32 voxel_size )
{
    AABB bounds = AABB::Prepare();

    array_span_t<const u16> indices = GetIndexStream16( mesh );
    array_span_t<const vec3_t> positions = GetVertexStream<vec3_t>( mesh, RDIEVertexSlot::POSITION );
    for( const vec3_t& pos : positions )
    {
        bounds = AABB::Extend( bounds, pos );
    }

    const vec3_t bounds_size = AABB::Size( bounds );
    const f32 width  = ::ceilf( bounds_size.x / voxel_size );
    const f32 height = ::ceilf( bounds_size.y / voxel_size );
    const f32 depth  = ::ceilf( bounds_size.z / voxel_size );

    output->bounds = bounds;
    output->voxel_size = voxel_size;
    output->grid = grid_t( (u32)width, (u32)height, (u32)depth );
    array::resize( output->voxels, output->grid.NumCells() );
    memset( output->voxels.begin(), 0x00, array::size_in_bytes( output->voxels ) );

    Voxelize(
        to_array_span( output->voxels.begin(), output->voxels.size ),
        output->grid.width, output->grid.height, output->grid.depth,
        bounds,
        positions, indices );
}

void MESHTool::_Load( CMNEngine* e, const TOOLContext& ctx, const char* filename )
{
    BXFileWaitResult result = LoadFileSync( e->filesystem, filename, BXEFIleMode::BIN, _allocator );
    if( result.status == BXEFileStatus::READY )
    {
        srl_file_t* serialized_file = (srl_file_t*)result.file.bin;

        const RDIXMeshFile* mesh_file = serialized_file->data<RDIXMeshFile>();
        if( mesh_file )
        {
            string::free( &_current_dst_file );
            BX_FREE0( _allocator, _mesh_file );
            _id_mesh_comp = LoadTOOLMeshComponent( e, _id_mesh_comp, ctx, mesh_file, _allocator );
            _mesh_file = serialized_file;

            _vox_mesh.Clear();
            VoxelizeMesh( &_vox_mesh, mesh_file, 0.016f );
        }
    }

    e->filesystem->CloseFile( &result.handle, false );
}

void MESHTool::_Save( CMNEngine* e, common::FolderContext* folder )
{
    common::CreateDstFilename( &_current_dst_file, folder->Root(), _current_src_file, "mesh", _allocator );
    WriteFileSync( e->filesystem, _current_dst_file.c_str(), _mesh_file, _mesh_file->size );
    folder->RequestRefresh();
}

void VoxelizedMesh::Clear()
{
    bounds = AABB::Prepare();
    voxel_size = 0.f;
    grid = {};
    array::clear( voxels );
}
