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

namespace helper
{
    static void UnloadComponent( CMNEngine* e, ECSComponentID mesh_comp )
    {
        if( e->ecs->IsAlive( mesh_comp ) )
        {
            TOOLMeshComponent* comp_data = Component<TOOLMeshComponent>( e->ecs, mesh_comp );
            comp_data->Uninitialize( e->gfx );
            e->ecs->MarkForDestroy( mesh_comp );
        }
    }

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

void MESHTool::StartUp( CMNEngine* e, const char* src_root, const char* dst_root, BXIAllocator* allocator )
{
    _allocator = allocator;
    _root_src_folder_ctx.SetUp( e->filesystem, allocator );
    _root_src_folder_ctx.SetFolder( src_root );

    _root_dst_folder_ctx.SetUp( e->filesystem, allocator );
    _root_dst_folder_ctx.SetFolder( dst_root );

    helper::SetToDefaults( &_compile_options );
}

void MESHTool::ShutDown( CMNEngine* e )
{
    helper::UnloadComponent( e, _id_mesh_comp );
    _allocator = nullptr;
}

uint32_t find_r( const char* begin, uint32_t len, const char needle )
{
    int32_t pos = len;
    while( pos )
    {
        if( begin[pos] == needle )
            break;

        --pos;
    }
    return pos;
}



void MESHTool::Tick( CMNEngine* e, const TOOLContext& ctx )
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
                const string_buffer_t& file_list = _root_dst_folder_ctx.FileList();
                string_buffer_it selected_it = common::MenuFileSelector( file_list );
                if( !selected_it.null() )
                {
                    string::create( &_current_dst_file, selected_it.pointer, _allocator );
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

        if( _current_src_file.c_str() && _mesh_file )
        {
            if( ImGui::Button( "Save" ) )
            {
                const char d = '.';
                const char s = '/';

                const uint32_t src_len = _current_src_file.length();
                const uint32_t s_pos = find_r( _current_src_file.c_str(), src_len, s );

                char tmp[255] = {};

                const char* src_name = _current_src_file.c_str();
                if( s_pos != UINT32_MAX )
                    src_name += s_pos + 1;
                
                uint32_t len = snprintf( tmp, 255, "%s", src_name );
                
                const uint32_t d_pos = find_r( tmp, len, d );
                if( d_pos != UINT32_MAX )
                {
                    snprintf( tmp + d_pos + 1, 255 - (len + d_pos + 1), "mesh" );
                }

                char tmp_path[255] = {};
                string::append( tmp_path, 255, "%s/%s", _root_dst_folder_ctx.Folder(), tmp );
                string::create( &_current_dst_file, tmp_path, _allocator );

                WriteFileSync( e->filesystem, _current_dst_file.c_str(), _mesh_file, _mesh_file->size );
                _root_dst_folder_ctx.RequestRefresh();
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
        RDIXRenderSource* rsource = CreateRenderSourceFromMemory( e->rdidev, mesh_file, _allocator );

        ECSNewComponent new_comp = CreateComponent< TOOLMeshComponent >( e->ecs );
        ECSComponentID mesh_comp = new_comp.id;
        TOOLMeshComponent* comp_data = new_comp.Cast<TOOLMeshComponent>();

        GFXMeshInstanceDesc desc = {};
        desc.AddRenderSource( rsource );
        if( e->ecs->IsAlive( _id_mesh_comp ) )
        {
            TOOLMeshComponent* old_comp_data = Component<TOOLMeshComponent>( e->ecs, _id_mesh_comp );
            desc.idmaterial = e->gfx->Material( old_comp_data->id_mesh );
        }
        else
        {
            desc.idmaterial = e->gfx->FindMaterial( "editable" );
        }
        desc.flags.gpu_skinning = mesh_file->num_bones > 0;
        comp_data->Initialize( e->gfx, ctx.gfx_scene, desc, mat44_t::identity() );

        helper::UnloadComponent( e, _id_mesh_comp );
        _id_mesh_comp = mesh_comp;
        e->ecs->Link( ctx.entity, &mesh_comp, 1 );

        {
            ECSComponentID desc_comp_id;
            TOOLMeshDescComponent* desc_comp;
            common::CreateComponentIfNotExists( &desc_comp_id, &desc_comp, e->ecs, ctx.entity );
        
            const uint32_t num_bones = mesh_file->num_bones;
            array::clear( desc_comp->bones_names );
            array::clear( desc_comp->bones_offsets );
            array::reserve( desc_comp->bones_names, num_bones );
            array::reserve( desc_comp->bones_offsets, num_bones );

            const hashed_string_t* bones_names = TYPE_OFFSET_GET_POINTER( hashed_string_t, mesh_file->offset_bones_names );
            const mat44_t* bones_offsets = TYPE_OFFSET_GET_POINTER( mat44_t, mesh_file->offset_bones );

            for( uint32_t i = 0; i < num_bones; ++i )
            {
                array::push_back( desc_comp->bones_names, bones_names[i] );
                array::push_back( desc_comp->bones_offsets, bones_offsets[i] );
            }
        }

        {
            ECSComponentID skinning_id;
            TOOLSkinningComponent* skinning_comp = nullptr;
            common::CreateComponentIfNotExists( &skinning_id, &skinning_comp, e->ecs, ctx.entity );
            InitializeSkinningComponent( skinning_id, e->ecs );
            skinning_comp->id_mesh = comp_data->id_mesh;
        }

        { // file to serialize
            BX_FREE0( _allocator, _mesh_file );
            _mesh_file = srl_file::serialize( (RDIXMeshFile*)cmesh.raw, cmesh.size, _allocator );
        }
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

