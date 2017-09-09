#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include "window.h"

//typedef int( *bxWindow_MsgCallbackPtr )( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );

struct BXIAllocator;
namespace bx
{
    struct Window
    {
        //enum
        //{
        //    eMAX_MSG_CALLBACKS = 4,
        //};
        BXWindow win;
        HWND hwnd;
        HWND parent_hwnd;
        HINSTANCE hinstance;
        HDC hdc;

        //bxWindow_MsgCallbackPtr winMsgCallbacks[eMAX_MSG_CALLBACKS];
        //i32 numWinMsgCallbacks;
    };


    BXWindow* WindowCreate( const char* name, unsigned width, unsigned height, bool full_screen, BXIAllocator* allocator );
    void      WindowRelease( BXWindow* win, BXIAllocator* allocator );

    void WindowSetSize( Window* win, unsigned width, unsigned height );
    //int bxWindow_addWinMsgCallback( bxWindow* win, bxWindow_MsgCallbackPtr ptr );
    //void bxWindow_removeWinMsgCallback( bxWindow* win, bxWindow_MsgCallbackPtr ptr );

}//

