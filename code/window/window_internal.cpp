#include "window_internal.h"
#include <foundation/debug.h>
#include <foundation/memory/memory.h>
#include "foundation/string_util.h"

namespace bx
{
    static void _AdjustWindowSize( HWND hwnd, HWND parent_hwnd, uint32_t width, uint32_t height, bool full_screen )
    {
        int screen_width = GetSystemMetrics( SM_CXSCREEN );
        int screen_height = GetSystemMetrics( SM_CYSCREEN );

        if( !full_screen )
        {
            if( !parent_hwnd )
            {
                DWORD window_style = WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

                RECT client;
                client.left = 0;
                client.top = 0;
                client.right = width;
                client.bottom = height;

                BOOL ret = AdjustWindowRectEx( &client, window_style, FALSE, 0 );
                SYS_ASSERT( ret );
                const int tmp_width = client.right - client.left;
                const int tmp_height = client.bottom - client.top;
                const int x = 0; //screen_width - tmp_width;
                const int y = screen_height - tmp_height;

                MoveWindow( hwnd, x, y, tmp_width, tmp_height, FALSE );
            }
            else
            {
                DWORD window_style = WS_CHILD;// | WS_BORDER;
                WINDOWINFO win_info;
                BOOL res = GetWindowInfo( parent_hwnd, &win_info );
                SYS_ASSERT( res );

                RECT client;
                client.left = 0;
                client.top = 0;
                client.right = win_info.rcWindow.right;
                client.bottom = win_info.rcWindow.bottom;

                //BOOL ret = AdjustWindowRectEx( &client, window_style, FALSE, 0 );
                //assert( ret );

                MoveWindow( hwnd, 0, 0, client.right, client.bottom, FALSE );
            }
        }
        else
        {
            MoveWindow( hwnd, 0, 0, screen_width, screen_height, FALSE );

            DEVMODE screen_settings;
            memset( &screen_settings, 0, sizeof( screen_settings ) );

            screen_settings.dmSize = sizeof( screen_settings );
            screen_settings.dmPelsWidth = screen_width;
            screen_settings.dmPelsHeight = screen_height;
            screen_settings.dmBitsPerPel = 32;
            screen_settings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

            ChangeDisplaySettings( &screen_settings, CDS_FULLSCREEN );

            ShowCursor( FALSE );
        }

        UpdateWindow( hwnd );
    }

    static uintptr_t _GetSystemHandle( BXWindow* instance )
    {
        SYS_STATIC_ASSERT( sizeof(uintptr_t) == sizeof( HWND ) );
        bx::Window* win = ( bx::Window* )instance;
        return (uintptr_t)win->hwnd;
    }

    static LRESULT CALLBACK DefaultWindowMessageProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
    
    BXWindow* WindowCreate( const char* name, unsigned width, unsigned height, bool full_screen, BXIAllocator* allocator )
    {
        HWND parent_hwnd = nullptr;

        HINSTANCE hinstance = GetModuleHandle( NULL );
        HDC hdc = 0;
        HWND hwnd = 0;

        { // create window
            WNDCLASS wndClass;
            wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	// Redraw On Size, And Own DC For Window.
            wndClass.lpfnWndProc = DefaultWindowMessageProc;
            wndClass.cbClsExtra = 0;
            wndClass.cbWndExtra = 0;
            wndClass.hInstance = hinstance;
            wndClass.hIcon = 0;
            wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
            wndClass.hbrBackground = NULL;
            wndClass.lpszMenuName = NULL;
            wndClass.lpszClassName = name;

            if( !RegisterClass( &wndClass ) )
            {
                DWORD err = GetLastError();
                printf( "create_window: Error with RegisterClass!\n" );
                return false;
            }

            if( full_screen && !parent_hwnd )
            {
                hwnd = CreateWindowEx(
                    WS_EX_APPWINDOW, name, name, WS_POPUP,
                    0, 0,
                    10, 10,
                    NULL, NULL, hinstance, 0 );
            }
            else
            {
                DWORD style = 0;
                DWORD ex_style = 0;
                if( parent_hwnd )
                {
                    style = WS_CHILD | WS_BORDER;
                    ex_style = WS_EX_TOPMOST | WS_EX_NOPARENTNOTIFY;
                }
                else
                {
                    style = WS_BORDER | WS_THICKFRAME;
                }
                hwnd = CreateWindowEx(
                    ex_style, name, name, style,
                    10, 10,
                    100, 100,
                    parent_hwnd, NULL, hinstance, 0 );
            }

            if( hwnd == NULL )
            {
                printf( "create_window: Error while creating window (Err: 0x%x)\n", GetLastError() );
                return 0;
            }

            hdc = GetDC( hwnd );
        }

        Window* win = BX_NEW( allocator, Window );
        win->win.GetSystemHandle = _GetSystemHandle;  
        strcpy_s( win->win.name, BXWindow::NAME_LEN, name );

        win->hwnd = hwnd;
        win->parent_hwnd = parent_hwnd;
        win->hinstance = hinstance;
        win->hdc = hdc;

        win->win.full_screen = full_screen;

        { // setup window
            _AdjustWindowSize( hwnd, parent_hwnd, width, height, full_screen );
            ShowWindow( hwnd, SW_SHOW );

            RECT win_rect;
            GetClientRect( hwnd, &win_rect );
            width = win_rect.right - win_rect.left;
            height = win_rect.bottom - win_rect.top;
        }

        win->win.width = width;
        win->win.height = height;

        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)win );

        return &win->win;
    }

    void WindowRelease( BXWindow* win, BXIAllocator* allocator )
    {
        Window* win_internal = (Window*)win;
        {
            ReleaseDC( win_internal->hwnd, win_internal->hdc );
            DestroyWindow( win_internal->hwnd );
            win_internal->hwnd = NULL;
        }
        UnregisterClass( win->name, win_internal->hinstance );
        BX_DELETE0( allocator, win_internal );
    }

	bool WindowAddCallback(BXWindow* win, BXWin32WindowCallback function_ptr, void * user_data)
	{
		SYS_ASSERT(function_ptr != nullptr);
		
		Window* win_internal = (Window*)win;
		if (win_internal->num_callbacks >= bx::Window::MAX_MSG_CALLBACKS)
			return false;

		const uint32_t index = win_internal->num_callbacks++;
		win_internal->callbacks[index] = function_ptr;
		win_internal->callbacks_user_data[index] = user_data;
		
		return true;
	}

	void WindowRemoveCallback(BXWindow* win, BXWin32WindowCallback function_ptr)
	{
		if (!function_ptr)
			return;

		Window* win_internal = (Window*)win;

		for (uintptr_t i = 0; i < win_internal->num_callbacks; )
		{
			if (function_ptr == win_internal->callbacks[i])
			{
				const uint32_t last_index = win_internal->num_callbacks - 1;
				win_internal->callbacks[i] = win_internal->callbacks[last_index];
				win_internal->callbacks_user_data[i] = win_internal->callbacks_user_data[last_index];
				win_internal->num_callbacks -= 1;
			}
			else
			{
				++i;
			}
		}
	}

    LRESULT CALLBACK DefaultWindowMessageProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
    {
        Window* win = (Window*)GetWindowLongPtr( hwnd, GWLP_USERDATA );
        if( !win)
        {
            return DefWindowProc( hwnd, msg, wParam, lParam );
        }

		int processed = 0;
		for (uint32_t i = 0; i < win->num_callbacks; ++i)
			processed |= win->callbacks[i]( (uintptr_t)hwnd, msg, wParam, lParam, win->callbacks_user_data[i] );
		
		if (processed)
			return DefWindowProc(hwnd, msg, wParam, lParam);


        static bool lButtonPressed = false;
        BXInput* input = &win->win.input;
        BXInput::KeyboardState* kbdState = input->kbd.CurrentState();
        BXInput::MouseState* mouseState = input->mouse.CurrentState();

        //GetKeyboardState( kbdState->keys );

        switch( msg )
        {
            //case WM_KEYUP:
            //	{
            //		if ( wParam < 256 )
            //		{
            //			kbdState->keys[wParam] = false;
            //		}
            //		break;
            //	}
            //case WM_KEYDOWN:
            //	{
            //		if ( wParam < 256 )
            //		{
            //               kbdState->keys[wParam] = true;
            //		}
            //		break;
            //	}

        case WM_LBUTTONDOWN:
            {
                mouseState->lbutton = 1;
                break;
            }

        case WM_LBUTTONUP:
            {
                mouseState->lbutton = 0;
                break;
            }
        case WM_MBUTTONDOWN:
            {
                mouseState->mbutton = 1;
                break;
            }

        case WM_RBUTTONUP:
            {
                mouseState->rbutton = 0;
                break;
            }

        case WM_RBUTTONDOWN:
            {
                mouseState->rbutton = 1;
                break;
            }

        case WM_MBUTTONUP:
            {
                mouseState->mbutton = 0;
                break;
            }

        case WM_MOUSEMOVE:
            {
                POINTS pt = MAKEPOINTS( lParam );
                //const unsigned short x = GET_X_LPARAM(lParam);
                //const unsigned short y = GET_Y_LPARAM(lParam);
                const unsigned short x = pt.x;
                const unsigned short y = pt.y;

                mouseState->x = x;
                mouseState->y = y;

                break;
            }

        case WM_SIZE:
            {
                RECT clientRect;
                GetClientRect( win->hwnd, &clientRect );

                unsigned int clientWindowWidth = clientRect.right - clientRect.left;
                unsigned int clientWindowHeight = clientRect.bottom - clientRect.top;

                win->win.width = clientWindowWidth;
                win->win.height = clientWindowHeight;

                //_push_event( &g_window, msg, lParam, wParam );

                break;
            }
        case WM_CLOSE:
            //_push_event( &g_window, msg, lParam, wParam );
            PostQuitMessage( 0 );
            break;

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;
        case WM_PARENTNOTIFY:
            PostQuitMessage( 0 );
            break;
        case WM_SYSCOMMAND:
            {
                switch( wParam )
                {
                case SC_SCREENSAVE:
                case SC_MONITORPOWER:
                    break;
                default:
                    break;
                }
            }
            break;
        }

        return DefWindowProc( hwnd, msg, wParam, lParam );
    }
}//

