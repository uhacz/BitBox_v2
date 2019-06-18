#pragma once

#include "../foundation/type.h"
#include "../foundation/containers.h"
#include "../util/guid.h"

struct BXIAllocator;
struct BXIFilesystem;

struct NODEFlags
{
    union Offline
    {
        u16 all;
        struct
        {
            u16 read_only : 1;
            u16 tick : 1;
        };
    } offline;

    struct Online
    {
        u16 all;
        struct
        {
            u32 initializing : 1;
            u32 uninitializing : 1;
            u32 initialized : 1;
            u32 attaching : 1;
            u32 detaching : 1;
            u32 attached : 1;
        };
    }online;
};

struct NODEContainer;
struct GFX;
struct NODESystemContext
{
    BXIFilesystem* fsys;
    GFX* gfx;
};

struct NODEInitContext : NODESystemContext
{

};
struct NODEAttachContext : NODESystemContext
{
};

struct NODETickContext : NODESystemContext
{
    f32 dt;
};

struct NODETextSerializer;
struct NODE;
struct NODEComp;

using NODEGuid = guid_t;
using NODESpan = array_span_t<NODE*>;
using NODECompSpan = array_span_t<NODEComp*>;

