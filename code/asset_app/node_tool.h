#pragma once

#include <foundation/string_util.h>
#include "tool_context.h"


struct NODE;
struct NODEContainer;
struct NODETool : TOOLInterface
{
    void StartUp( CMNEngine* e, const char* src_root, BXIAllocator* allocator ) override;
    void ShutDown( CMNEngine* e ) override;
    void Tick( CMNEngine* e, const TOOLContext& ctx, float dt ) override;

    // internal
    void _DrawTree( NODEContainer* node_container );

    // data
    BXIAllocator* _allocator = nullptr;
    NODE* _current_node = nullptr;
    common::FolderContext _root_src_folder_ctx;
    FSName _current_src_file;

    union
    {
        u32 all = 0;
        struct
        {
            u32 node_menu : 1;
        };
    }_flags;

};