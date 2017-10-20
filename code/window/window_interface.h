#pragma once

#include "window_callback.h"

#define BX_WINDOW_PLUGIN_NAME "window"

struct BXWindow;
struct BXIAllocator;

struct BXIWindow
{
	virtual BXWindow* Create		( const char* name, unsigned width, unsigned height, bool full_screen, BXIAllocator* allocator ) = 0;
	virtual void	  Destroy		() = 0;
	virtual bool	  AddCallback	( BXWin32WindowCallback callback, void* userData ) = 0;
	virtual void	  RemoveCallback( BXWin32WindowCallback callback ) = 0;
	
	virtual const BXWindow* GetWindow() const = 0;
};


