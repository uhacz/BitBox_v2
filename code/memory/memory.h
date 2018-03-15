#pragma once

#include "allocator.h"

#ifndef alignof
#define ALIGNOF(x) __alignof(x)
#endif

#define BX_MALLOC( a, siz, align ) a->Alloc( a, siz, align )
#define BX_ALLOCATE( a, typ ) (typ*)( a->Alloc( a, sizeof(typ), ALIGNOF(typ)) )
#define BX_FREE( a, ptr ) (a->Free( a, ptr ) )
#define BX_FREE0( a, ptr ) { BX_FREE(a, ptr); ptr = 0; }

#include <new>
#define BX_NEW(a, T, ...) (new ((a)->Alloc( a, sizeof(T), ALIGNOF(T))) T(__VA_ARGS__))

template<typename T>
void BX_DELETE( BXIAllocator* alloc, T* ptr )
{
    if( ptr )
    {
        ptr->~T();
        BX_FREE( alloc, ptr );
    }
}
#define BX_DELETE0( a, ptr ) { BX_DELETE( a, ptr ); ptr = 0; }

#include "dll_interface.h"


extern "C"{

    MEMORY_PLUGIN_EXPORT BXIAllocator* BXDefaultAllocator();
}