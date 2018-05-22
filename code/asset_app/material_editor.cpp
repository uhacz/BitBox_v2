#include "material_editor.h"
#include <3rd_party/imgui/imgui.h>
#include "util/file_system_name.h"


void MATEditor::SetDefault( gfx_shader::Material* mat )
{
    mat->diffuse_albedo = vec3_t( 1.f, 1.f, 1.f );
    mat->specular_albedo = vec3_t( 1.f, 1.f, 1.f );
    mat->metal = 0.f;
    mat->roughness = 0.5f;
}

void MATEditor::StartUp( GFX* gfx, GFXSceneID scene_id, RSM* rsm )
{
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
    gfx->RemoveMeshFromScene( _mesh_id );
    gfx->DestroyMaterial( _mat_id );
}

void MATEditor::Tick( GFX* gfx, BXIFilesystem* fs )
{
    bool edited = false;
    if( ImGui::Begin( "Material" ) )
    {
        if( ImGui::BeginMenu( "File" ) )
        {
            if( ImGui::MenuItem( "New" ) )
            {
                SetDefault( &_mat_data );
                edited = true;
            }
            if( ImGui::MenuItem( "Load" ) )
            {

            }
            if( ImGui::BeginMenu( "Save as..." ) )
            {
                ImGui::InputText( "Filename: ", _input_txt, TXT_SIZE );

                if( ImGui::Button( "save" ) )
                {
                    static constexpr uint32_t DATA_SIZE = 2048;
                    uint8_t data_buff[DATA_SIZE] = {};
                    uint32_t data_bytes = RTTI::Serialize( data_buff, DATA_SIZE, _mat_data );
                    if( data_bytes )
                    {
                        FSName relative_filename;
                        relative_filename.Append( _folder.c_str() );
                        relative_filename.Append( _input_txt );
                        relative_filename.Append( ".material" );

                        fs->WriteFileSync( fs, relative_filename.AbsolutePath(), data_buff, data_bytes );
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        edited |= ImGui::ColorEdit3( "Diffuse albedo", _mat_data.diffuse_albedo.xyz, ImGuiColorEditFlags_NoAlpha );
        edited |= ImGui::ColorEdit3( "Specular albedo", _mat_data.specular_albedo.xyz, ImGuiColorEditFlags_NoAlpha );
        edited |= ImGui::SliderFloat( "Roughness", &_mat_data.roughness, 0.f, 1.f, "%.4f" );
    }
    ImGui::End();

    if( edited )
    {
        gfx->SetMaterialData( _mat_id, _mat_data );
    }
}
