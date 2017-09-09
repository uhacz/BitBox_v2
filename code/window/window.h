#pragma once

#include <foundation/type.h>
#include <input/input.h>

struct BXWindow
{
    enum { NAME_LEN = 63 };
    char name[NAME_LEN+1] = {};
    u32 width             = 0;
    u32 height            = 0;
    u32 full_screen       = 0;
    BXInput input         = {};

    uintptr_t( *GetSystemHandle )( BXWindow* instance );
};
