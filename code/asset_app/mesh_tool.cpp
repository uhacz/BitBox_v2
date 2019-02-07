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
    _UnloadComponents( e );
    BX_FREE0( _allocator, _mesh_file );
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
                            _Save( e );
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

        _LoadComponents( e, ctx, mesh_file );

        { // file to serialize
            BX_FREE0( _allocator, _mesh_file );
            _mesh_file = srl_file::serialize( (RDIXMeshFile*)cmesh.raw, cmesh.size, _allocator );
        }
    }

    cmesh.destroy();
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
            _LoadComponents( e, ctx, mesh_file );
            _mesh_file = serialized_file;
        }
    }

    e->filesystem->CloseFile( &result.handle, false );
}

void MESHTool::_Save( CMNEngine* e )
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

void MESHTool::_UnloadComponents( CMNEngine* e )
{
    UnloadComponent<TOOLMeshComponent>( e->ecs, _id_mesh_comp, e->gfx );
    e->ecs->MarkForDestroy( _id_mesh_desc_comp );
    e->ecs->MarkForDestroy( _id_skinning_comp );
}

void MESHTool::_LoadComponents( CMNEngine* e, const TOOLContext& ctx, const RDIXMeshFile* mesh_file )
{
    RDIXRenderSource* rsource = CreateRenderSourceFromMemory( e->rdidev, mesh_file, _allocator );

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

    _UnloadComponents( e );

    _id_mesh_comp = CreateComponent< TOOLMeshComponent >( e->ecs ).id;
    TOOLMeshComponent* comp_data = InitializeComponent<TOOLMeshComponent>( e->ecs, _id_mesh_comp, e->gfx, ctx.gfx_scene, desc, mat44_t::identity() );
    e->ecs->Link( ctx.entity, &_id_mesh_comp, 1 );

    {
        _id_mesh_desc_comp = common::CreateComponentIfNotExists<TOOLMeshDescComponent>( e->ecs, ctx.entity );
        InitializeComponent<TOOLMeshDescComponent>( e->ecs, _id_mesh_desc_comp, mesh_file );
    }

    {
        _id_skinning_comp = common::CreateComponentIfNotExists<TOOLSkinningComponent>( e->ecs, ctx.entity );
        InitializeComponent<TOOLSkinningComponent>( e->ecs, _id_skinning_comp, e->ecs, _id_mesh_desc_comp, comp_data->id_mesh );
    }
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

