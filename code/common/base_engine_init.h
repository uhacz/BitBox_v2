#pragma once

struct BXIAllocator;
struct BXPluginRegistry;

struct RDIDevice;
struct RDICommandQueue;

struct GFX;
struct ENT;
struct ECS;
struct BXIFilesystem;
struct NODEContainer;

struct CMNBaseEngine
{
    BXIAllocator* allocator = nullptr;
    BXIFilesystem* filesystem = nullptr;
    RDIDevice* rdidev = nullptr;
    RDICommandQueue* rdicmdq = nullptr;

    static bool Startup( CMNBaseEngine* e, int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* main_allocator );
    static void Shutdown( CMNBaseEngine* e );
};

struct CMNEngine : CMNBaseEngine
{
    GFX* gfx = nullptr;
    ECS* ecs = nullptr;
    NODEContainer* nodes = nullptr;
  
    static bool Startup( CMNEngine* e, int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator );
    static void Shutdown( CMNEngine* e );
};



