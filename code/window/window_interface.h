#pragma once

#define BX_WINDOW_PLUGIN_NAME "window"

struct BXWindow;
struct BXIAllocator;
struct BXIWindow
{
    BXWindow* ( *Create )( const char* name, unsigned width, unsigned height, bool full_screen, BXIAllocator* allocator );
    void( *Release )( BXWindow* win, BXIAllocator* allocator );
};


