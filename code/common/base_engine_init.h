#pragma once

struct BXIAllocator;
struct BXPluginRegistry;

struct RDIDevice;
struct RDICommandQueue;

struct RSM;
struct GFX;
struct ENT;
struct ECS;
struct BXIFilesystem;

struct CMNEngine
{
    BXIFilesystem* _filesystem = nullptr;
    RDIDevice* _rdidev = nullptr;
    RDICommandQueue* _rdicmdq = nullptr;

    RSM* _rsm = nullptr;
    GFX* _gfx = nullptr;
    ENT* _ent = nullptr;

    ECS* _ecs = nullptr;
  
    bool Startup( CMNEngine* engine, int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator );
    void Shutdown( CMNEngine* engine, BXIAllocator* allocator );
};



