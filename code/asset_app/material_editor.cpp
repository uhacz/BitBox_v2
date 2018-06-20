#include "material_editor.h"
#include <3rd_party/imgui/imgui.h>
#include <util/file_system_name.h>
#include <gfx/gfx_type.h>
#include <foundation/common.h>
#include "foundation/data_buffer.h"
#include "rtti/serializer.h"

static constexpr char MATERIAL_FILE_EXT[] = ".material";

void MATEditor::SetDefault( GFXMaterialResource* mat )
{
    mat->data.diffuse_albedo = vec3_t( 1.f, 1.f, 1.f );
    mat->data.specular_albedo = vec3_t( 1.f, 1.f, 1.f );
    mat->data.metal = 0.f;
    mat->data.roughness = 1.f;

    for( uint32_t i = 0; i < GFXEMaterialTextureSlot::_COUNT_; ++i )
        string::free( &mat->textures[i] );
}

void MATEditor::StartUp( GFX* gfx, GFXSceneID scene_id, RSM* rsm, BXIAllocator* allocator )
{
    _allocator = allocator;

    SetDefault( &_mat_resource );

    static const char* tex_names[] =
    {
        "texture/2/d.DDS",
        "texture/2/n.DDS",
        "texture/2/r.DDS",
        "texture/not_metal.DDS",
    };

    //_mat_tex.id[GFXEMaterialTextureSlot::BASE_COLOR] = rsm->Load( tex_names[0], gfx );
    //_mat_tex.id[GFXEMaterialTextureSlot::NORMAL]     = rsm->Load( tex_names[1], gfx );
    //_mat_tex.id[GFXEMaterialTextureSlot::ROUGHNESS]  = rsm->Load( tex_names[2], gfx );
    //_mat_tex.id[GFXEMaterialTextureSlot::METALNESS]  = rsm->Load( tex_names[3], gfx );

    GFXMaterialDesc desc;
    desc.data = _mat_resource.data;
    desc.textures = _mat_tex;
    _mat_id = gfx->CreateMaterial( "editable", desc );

    GFXMeshInstanceDesc mesh_desc = {};
    mesh_desc.idmaterial = _mat_id;
    mesh_desc.idmesh_resource = rsm->Find( "sphere" );
    _mesh_id = gfx->AddMeshToScene( scene_id, mesh_desc, mat44_t::identity() );

    _folder = "material/";
    _texture_folder = "texture/";
}

void MATEditor::ShutDown( GFX* gfx, RSM* rsm )
{
    string::free( &_current_file );
    string::free( &_file_list );
    gfx->RemoveMeshFromScene( _mesh_id );
    gfx->DestroyMaterial( _mat_id );
    for( uint32_t i = 0; i < GFXEMaterialTextureSlot::_COUNT_; ++i )
    {
        rsm->Release( _mat_tex.id[i] );
    }
}

static void RefreshFiles( string_buffer_t* flist, const char* folder, BXIFilesystem* fs, BXIAllocator* allocator )
{
    const uint32_t initial_capacity = max_of_2( 32u, flist->_capacity );
    string::create( flist, initial_capacity, allocator );
    fs->ListFiles( fs, flist, folder, true, allocator );
}


static string_buffer_it MenuFileSelector( const string_buffer_t& file_list )
{
    string_buffer_it file_it = string::iterate( file_list, string_buffer_it() );
    while( !file_it.null() )
    {
        bool selected = false;
        if( ImGui::MenuItem( file_it.pointer, nullptr, &selected ) )
        {
            if( selected )
            {
                break;
            }
        }
        file_it = string::iterate( file_list, file_it );
    }

    return file_it;
}

void MATEditor::Tick( GFX* gfx, BXIFilesystem* fs )
{
    if( ImGui::Begin( "Material" ) )
    {
        if( ImGui::BeginMenu( "File" ) )
        {
            if( ImGui::MenuItem( "New" ) )
            {
                SetDefault( &_mat_resource );
                _flags.refresh_material = 1;
                string::free( &_current_file );
            }
            ImGui::Separator();
            if( ImGui::BeginMenu( "Load" ) )
            {
                if( _file_list.null() )
                {
                    _flags.refresh_files = 1;
                }
                
                string_buffer_it selected_it = MenuFileSelector( _file_list );
                if( !selected_it.null() )
                {
                    _Load( selected_it.pointer, fs );
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if( _current_file.length() )
            {
                if( ImGui::MenuItem( "Save" ) )
                {
                    _Save( _current_file.c_str(), fs );
                }
            }
            if( ImGui::BeginMenu( "Save as..." ) )
            {
                ImGui::InputText( "Filename: ", _input_txt, TXT_SIZE );

                if( ImGui::Button( "Save" ) )
                {
                    _Save( _input_txt, fs );
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::Text( "Current file: %s", _current_file.c_str() );
        
        ImGui::Separator();
        _flags.refresh_material |= ImGui::ColorEdit3( "Diffuse albedo", _mat_resource.data.diffuse_albedo.xyz, ImGuiColorEditFlags_NoAlpha );
        _flags.refresh_material |= ImGui::ColorEdit3( "Specular albedo", _mat_resource.data.specular_albedo.xyz, ImGuiColorEditFlags_NoAlpha );
        _flags.refresh_material |= ImGui::SliderFloat( "Roughness", &_mat_resource.data.roughness, 0.f, 1.f, "%.4f" );
        ImGui::Separator();
        

        //if( ImGui::BeginMenu() )
        {
            ImGui::Text( "Base  : %s", _mat_resource.textures[0].c_str() );
            ImGui::SameLine();
            if( ImGui::BeginMenu( "file..." ) )
            {
                if( _texture_file_list.null() )
                {
                    _flags.refresh_files_texture = 1;
                }

                string_buffer_it selected_it = MenuFileSelector( _file_list );
                if( !selected_it.null() )
                {
                    string::create( &_mat_resource.textures[0], selected_it.pointer, _allocator );
                }
                ImGui::EndMenu();
            }

            ImGui::Text( "Normal: %s", _mat_resource.textures[1].c_str() );
            ImGui::Text( "Roughness: %s", _mat_resource.textures[2].c_str() );
            ImGui::Text( "Metalness: %s", _mat_resource.textures[3].c_str() );
        }


        //ImGui::InputText


    }
    ImGui::End();

    if( _flags.refresh_material )
    {
        _flags.refresh_material = 0;
        gfx->SetMaterialData( _mat_id, _mat_resource.data );
    }
    if( _flags.refresh_files )
    {
        _flags.refresh_files = 0;
        RefreshFiles( &_file_list, _folder.c_str(), fs, _allocator );
    }
    if( _flags.refresh_files_texture )
    {
        _flags.refresh_files_texture = 0;
        RefreshFiles( &_texture_file_list, _texture_folder.c_str(), fs, _allocator );
    }
}

void MATEditor::_CreateRelativePath( FSName* fs_name, const char* filename )
{
    fs_name->Append( _folder.c_str() );
    fs_name->Append( filename );

    if( !string::find( filename, MATERIAL_FILE_EXT ) )
    {
        fs_name->Append( MATERIAL_FILE_EXT );
    }
}

void MATEditor::_Save( const char* filename, BXIFilesystem* fs )
{
    static constexpr uint32_t DATA_SIZE = 2048;
    uint8_t data[DATA_SIZE] = {};

    SRLInstance srl = SRLInstance::CreateWriterStatic( 0, data, DATA_SIZE, _allocator );
    Serialize( &srl, &_mat_resource );

    //uint8_t data_buff[DATA_SIZE] = {};
    //uint32_t data_bytes = RTTI::Serialize( data_buff, DATA_SIZE, _mat_data );
    if( data_buffer::size( srl.data ) )
    {
        FSName relative_filename;
        _CreateRelativePath( &relative_filename, filename );
        fs->WriteFileSync( fs, relative_filename.AbsolutePath(), srl.data.begin(), data_buffer::size( srl.data ) );
        _flags.refresh_files = 1;
    }
}

void MATEditor::_Load( const char* filename, BXIFilesystem* fs )
{
    FSName relative_filename;
    _CreateRelativePath( &relative_filename, filename );

    BXFileWaitResult wait = fs->LoadFileSync( fs, relative_filename.AbsolutePath(), BXIFilesystem::FILE_MODE_BIN, _allocator );
    if( wait.status == BXEFileStatus::READY )
    {
        SRLInstance srl = SRLInstance::CreateReader( 0, wait.file.pointer, wait.file.size, _allocator );
        Serialize( &srl, &_mat_resource );
        _flags.refresh_material = 1;
        //if( RTTI::Unserialize( &_mat_data, wait.file.bin, wait.file.size, _allocator ) )
        //{
        //    string::create( &_current_file, filename, _allocator );
        //    _flags.refresh_material = 1;
        //}
    }
    fs->CloseFile( wait.handle );

}
