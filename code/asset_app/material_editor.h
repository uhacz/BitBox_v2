#pragma once

#include <foundation/string_util.h>
#include <filesystem/filesystem_plugin.h>
#include <gfx/gfx.h>

struct MATEditor
{
    static void SetDefault( gfx_shader::Material* mat );

    void StartUp( GFX* gfx, GFXSceneID scene_id, RSM* rsm );
    void ShutDown( GFX* gfx );
    void Tick( GFX* gfx, BXIFilesystem* fs );

    GFXMeshInstanceID _mesh_id;
    GFXMaterialID _mat_id;
    gfx_shader::Material _mat_data;
    string_t _folder;

    string_t _load_request;
    BXFileHandle _file_handle;
    BXFile _file;

    static constexpr uint32_t TXT_SIZE = 2048;
    char _input_txt[TXT_SIZE] = {};
};