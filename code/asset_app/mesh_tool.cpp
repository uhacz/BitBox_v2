#include "mesh_tool.h"
#include <3rd_party/imgui/imgui.h>

#include <entity/entity_system.h>

#include "common/common.h"
#include "common/base_engine_init.h"
#include "components.h"
#include "filesystem/filesystem_plugin.h"
#include "rdix/rdix_type.h"
#include "rdix/rdix.h"

namespace helper
{
    static void UnloadComponent( CMNEngine* e, ECSComponentID mesh_comp )
    {
        if( e->_ecs->IsAlive( mesh_comp ) )
        {
            TOOLMeshComponent* comp_data = Component<TOOLMeshComponent>( e->_ecs, mesh_comp );
            comp_data->Uninitialize( e->_gfx );
            e->_ecs->MarkForDestroy( mesh_comp );
        }
    }
}//

void MESHTool::StartUp( CMNEngine* e, const char* asset_root, BXIAllocator* allocator )
{
    _allocator = allocator;
    _root_folder_ctx.SetUp( e->_filesystem, allocator );
    _root_folder_ctx.SetFolder( asset_root );
}

void MESHTool::ShutDown( CMNEngine* e )
{
    helper::UnloadComponent( e, _id_mesh_comp );
    
    BX_DELETE0( _allocator, _loaded_streams );

    _allocator = nullptr;
}

void MESHTool::Tick( CMNEngine* e, const TOOLContext& ctx )
{
    if( ImGui::Begin( "MeshTool" ) )
    {
        if( ImGui::BeginMenu( "File" ) )
        {
            if( ImGui::MenuItem( "New" ) )
            {
            }
            ImGui::Separator();
            if( ImGui::BeginMenu( "Load" ) )
            {
                const string_buffer_t& file_list = _root_folder_ctx.FileList();
                string_buffer_it selected_it = common::MenuFileSelector( file_list );
                if( !selected_it.null() )
                {
                    string::create( &_current_file, selected_it.pointer, _allocator );
                    if( auto streams = _Import( e ) )
                    {
                        BX_DELETE( _allocator, _loaded_streams );
                        _compile_options = {};
                        _loaded_streams = streams;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            ImGui::EndMenu();
        }
        ImGui::Separator();
        if( _loaded_streams )
        {
            if( ImGui::Button( "Compile" ) )
            {
                _Compile( e, ctx );
            }
            ImGui::Separator();
            if( ImGui::TreeNodeEx( "Stats", ImGuiTreeNodeFlags_DefaultOpen ) )
            {
                ImGui::Value( "Num vertices", _loaded_streams->num_vertices );
                ImGui::Value( "Num indices", _loaded_streams->num_indices );
                ImGui::Value( "Num bones", _loaded_streams->num_bones );
                ImGui::TreePop();
            }

            if( ImGui::TreeNodeEx( "Loaded streams", ImGuiTreeNodeFlags_DefaultOpen ) )
            {
                for( uint32_t islot = 0; islot < RDIEVertexSlot::COUNT; ++islot )
                {
                    const RDIVertexBufferDesc desc = _loaded_streams->slots[islot];
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
                for( const std::string& name : _loaded_streams->bones_names )
                {
                    ImGui::Text( name.c_str() );
                }
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}

tool::mesh::Streams* MESHTool::_Import( CMNEngine* e )
{
    const char* filename = _current_file.c_str();

    BXFileWaitResult result = e->_filesystem->LoadFileSync( e->_filesystem, filename, BXEFIleMode::BIN, _allocator );
    
    tool::mesh::Streams* mesh_streams = BX_NEW( _allocator, tool::mesh::Streams );
    if( !tool::mesh::Import( mesh_streams, result.file.pointer, result.file.size ) )
    {
        BX_DELETE0( _allocator, mesh_streams );
    }
    e->_filesystem->CloseFile( &result.handle );

    return mesh_streams;
}

void MESHTool::_Compile( CMNEngine* e, const TOOLContext& ctx )
{
    tool::mesh::CompileOptions opt;
    blob_t cmesh = tool::mesh::Compile( *_loaded_streams, _compile_options, _allocator );
    if( !cmesh.empty() )
    {
        RDIXMeshFile* mesh_file = (RDIXMeshFile*)cmesh.raw;
        RDIXRenderSource* rsource = CreateRenderSourceFromMemory( e->_rdidev, mesh_file, _allocator );

        ECSComponentID mesh_comp = CreateComponent< TOOLMeshComponent >( e->_ecs );
        TOOLMeshComponent* comp_data = Component<TOOLMeshComponent>( e->_ecs, mesh_comp );

        GFXMeshInstanceDesc desc = {};
        desc.AddRenderSource( rsource );
        if( e->_ecs->IsAlive( _id_mesh_comp ) )
        {
            TOOLMeshComponent* old_comp_data = Component<TOOLMeshComponent>( e->_ecs, _id_mesh_comp );
            desc.idmaterial = e->_gfx->Material( old_comp_data->id_mesh );
        }
        else
        {
            desc.idmaterial = e->_gfx->FindMaterial( "editable" );
        }
        comp_data->Initialize( e->_gfx, ctx.gfx_scene, desc, mat44_t::identity() );

        helper::UnloadComponent( e, _id_mesh_comp );
        _id_mesh_comp = mesh_comp;
        e->_ecs->Link( ctx.entity, &mesh_comp, 1 );
    }

    cmesh.destroy();
}


void FolderContext::SetUp( BXIFilesystem* fs, BXIAllocator* allocator )
{
    _filesystem = fs;
    _allocator = allocator;
}

void FolderContext::RequestRefresh()
{
    _flag_refresh_file_list = 1;
}

void FolderContext::SetFolder( const char* folder )
{
    _flag_refresh_file_list = !string::equal( _folder.c_str(), folder );
    string::create( &_folder, folder, _allocator );
}

const string_buffer_t& FolderContext::FileList() const
{
    if( _flag_refresh_file_list )
    {
        _flag_refresh_file_list = 0;
        common::RefreshFiles( &_file_list, _folder.c_str(), _filesystem, _allocator );
    }

    return _file_list;
}

