#include "material_editor.h"
#include <3rd_party/imgui/imgui.h>
#include "util/file_system_name.h"

static constexpr char MATERIAL_FILE_EXT[] = ".material";

void MATEditor::SetDefault( gfx_shader::Material* mat )
{
    mat->diffuse_albedo = vec3_t( 1.f, 1.f, 1.f );
    mat->specular_albedo = vec3_t( 1.f, 1.f, 1.f );
    mat->metal = 0.f;
    mat->roughness = 0.5f;
}

void MATEditor::StartUp( GFX* gfx, GFXSceneID scene_id, RSM* rsm, BXIAllocator* allocator )
{
    _allocator = allocator;

    SetDefault( &_mat_data );

    GFXMaterialDesc desc;
    desc.data = _mat_data;
    _mat_id = gfx->CreateMaterial( "editable", desc );

    GFXMeshInstanceDesc mesh_desc = {};
    mesh_desc.idmaterial = _mat_id;
    mesh_desc.idmesh_resource = rsm->Find( "sphere" );
    _mesh_id = gfx->AddMeshToScene( scene_id, mesh_desc, mat44_t::identity() );

    _folder = "material/";
}

void MATEditor::ShutDown( GFX* gfx )
{
    string::free( &_current_file );
    string::free( &_file_list );
    gfx->RemoveMeshFromScene( _mesh_id );
    gfx->DestroyMaterial( _mat_id );
}

void MATEditor::Tick( GFX* gfx, BXIFilesystem* fs )
{
    
    if( ImGui::Begin( "Material" ) )
    {
        if( ImGui::BeginMenu( "File" ) )
        {
            if( ImGui::MenuItem( "New" ) )
            {
                SetDefault( &_mat_data );
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
                bool selected = false;
                string_buffer_it file_it = string::iterate( _file_list, string_buffer_it() );
                while( !file_it.null() )
                {
                    if( ImGui::MenuItem( file_it.pointer, nullptr, &selected ) )
                    {
                        if( selected )
                        {
                            _Load( file_it.pointer, fs );
                        }
                    }
                    file_it = string::iterate( _file_list, file_it );
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
        _flags.refresh_material |= ImGui::ColorEdit3( "Diffuse albedo", _mat_data.diffuse_albedo.xyz, ImGuiColorEditFlags_NoAlpha );
        _flags.refresh_material |= ImGui::ColorEdit3( "Specular albedo", _mat_data.specular_albedo.xyz, ImGuiColorEditFlags_NoAlpha );
        _flags.refresh_material |= ImGui::SliderFloat( "Roughness", &_mat_data.roughness, 0.f, 1.f, "%.4f" );
    }
    ImGui::End();

    if( _flags.refresh_material )
    {
        _flags.refresh_material = 0;
        gfx->SetMaterialData( _mat_id, _mat_data );
    }
    if( _flags.refresh_files )
    {
        _flags.refresh_files = 0;
        const uint32_t initial_capacity = max_of_2( 32u, _file_list._capacity );
        string::create( &_file_list, initial_capacity, _allocator );
        fs->ListFiles( fs, &_file_list, _folder.c_str(), false, _allocator );
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
    uint8_t data_buff[DATA_SIZE] = {};
    uint32_t data_bytes = RTTI::Serialize( data_buff, DATA_SIZE, _mat_data );
    if( data_bytes )
    {
        FSName relative_filename;
        _CreateRelativePath( &relative_filename, filename );
        fs->WriteFileSync( fs, relative_filename.AbsolutePath(), data_buff, data_bytes );
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
        if( RTTI::Unserialize( &_mat_data, wait.file.bin, wait.file.size, _allocator ) )
        {
            string::create( &_current_file, filename, _allocator );
            _flags.refresh_material = 1;
        }
    }
    fs->CloseFile( wait.handle );

}
