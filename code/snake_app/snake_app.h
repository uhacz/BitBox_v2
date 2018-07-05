#pragma once

#include <application/application_plugin.h>

#include <rdi_backend/rdi_backend.h>
#include <rdix/rdix.h>

struct RSM;
struct GFX;
struct ENT;
struct BXIFilesystem;
struct SnakeApp : BXIApplication
{
    bool Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator ) override;
    void Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator ) override;
    bool Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator ) override;

private:
    BXIFilesystem* _filesystem = nullptr;
    RDIDevice* _rdidev = nullptr;
    RDICommandQueue* _rdicmdq = nullptr;

    GFX* _gfx = nullptr;
    ENT* _ent = nullptr;
    RSM* _rsm = nullptr;
};

BX_APPLICATION_PLUGIN_DECLARE( snake_app )