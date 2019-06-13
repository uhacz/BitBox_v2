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

struct CMNEngine
{
    BXIFilesystem* filesystem = nullptr;
    RDIDevice* rdidev = nullptr;
    RDICommandQueue* rdicmdq = nullptr;

    GFX* gfx = nullptr;
    ECS* ecs = nullptr;
    NODEContainer* nodes = nullptr;
  
    bool Startup( CMNEngine* engine, int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator );
    void Shutdown( CMNEngine* engine, BXIAllocator* allocator );
};



