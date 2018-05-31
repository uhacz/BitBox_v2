#pragma once

#include <foundation/string_util.h>
#include <filesystem/filesystem_plugin.h>
#include <gfx/gfx.h>

struct FSName;
struct MATEditor
{
    static void SetDefault( gfx_shader::Material* mat );

    void StartUp( GFX* gfx, GFXSceneID scene_id, RSM* rsm, BXIAllocator* allocator );
    void ShutDown( GFX* gfx, RSM* rsm );
    void Tick( GFX* gfx, BXIFilesystem* fs );

    void _CreateRelativePath( FSName* fs_name, const char* filename );
    void _Save( const char* filename, BXIFilesystem* fs );
    void _Load( const char* filename, BXIFilesystem* fs );

    BXIAllocator* _allocator;
    GFXMeshInstanceID _mesh_id;
    GFXMaterialID _mat_id;
    gfx_shader::Material _mat_data;
    GFXMaterialTexture _mat_tex;
    string_t _folder;

    string_t _current_file;
    string_buffer_t _file_list;

    static constexpr uint32_t TXT_SIZE = 2048;
    char _input_txt[TXT_SIZE] = {};

    union 
    {
        uint32_t all = 0;
        struct
        {
            uint32_t refresh_material : 1;
            uint32_t refresh_files : 1;
        };
    }_flags;
};