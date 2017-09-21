#pragma once

#include "window_callback.h"

#define BX_WINDOW_PLUGIN_NAME "window"

struct BXWindow;
struct BXIAllocator;



struct BXIWindow
{
    BXWindow* ( *Create )( const char* name, unsigned width, unsigned height, bool full_screen, BXIAllocator* allocator );
    void( *Release )( BXWindow* win, BXIAllocator* allocator );

	bool (*AddCallback)(BXWindow* win, BXWin32WindowCallback callback, void* userData );

};


