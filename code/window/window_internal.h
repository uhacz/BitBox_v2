#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include "window.h"
#include "window_callback.h"


struct BXIAllocator;
namespace bx
{
	struct Window
    {
        enum
        {
            MAX_MSG_CALLBACKS = 4,
        };
        BXWindow win;
        HWND hwnd;
        HWND parent_hwnd;
        HINSTANCE hinstance;
        HDC hdc;

		BXWin32WindowCallback callbacks[MAX_MSG_CALLBACKS] = {};
		void* callbacks_user_data[MAX_MSG_CALLBACKS] = {};
		uint32_t num_callbacks = 0;
    };


    BXWindow* WindowCreate( const char* name, unsigned width, unsigned height, bool full_screen, BXIAllocator* allocator );
    void      WindowRelease( BXWindow* win, BXIAllocator* allocator );

	bool	  WindowAddCallback( BXWindow* win, BXWin32WindowCallback function_ptr, void* user_data);
	void	  WindowRemoveCallback( BXWindow* win, BXWin32WindowCallback function_ptr);

    void WindowSetSize( Window* win, unsigned width, unsigned height );

}//

