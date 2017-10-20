#include "rdi_backend.h"
#include "rdi_backend_dx11.h"

RDIEType::Enum RDIEType::FromName( const char* name )
{
    for( int itype = 0; itype < RDIEType::COUNT; ++itype )
    {
        if( !strcmp( name, RDIEType::name[itype] ) )
            return (RDIEType::Enum)itype;
    }
    return RDIEType::UNKNOWN;
}

RDIEType::Enum RDIEType::FindBaseType( const char* name )
{
    for( int itype = 0; itype < RDIEType::COUNT; ++itype )
    {
        if( strstr( name, RDIEType::name[itype] ) )
            return (RDIEType::Enum)itype;
    }
    return RDIEType::UNKNOWN;
}


RDIEVertexSlot::Enum RDIEVertexSlot::FromString( const char* n )
{
    for( int i = 0; i < RDIEVertexSlot::COUNT; ++i )
    {
        if( !strcmp( n, RDIEVertexSlot::name[i] ) )
            return (RDIEVertexSlot::Enum )i;
    }

    return RDIEVertexSlot::COUNT;
}

void Startup( RDIDevice** dev, RDICommandQueue** cmdq, uintptr_t hWnd, int winWidth, int winHeight, int fullScreen, BXIAllocator* allocator )
{
    bx::rdi::StartupDX11( dev, cmdq, hWnd, winWidth, winHeight, fullScreen, allocator );
}

void Shutdown( RDIDevice** dev, RDICommandQueue** cmdq, BXIAllocator* allocator )
{
	bx::rdi::ShutdownDX11( dev, cmdq, allocator );
}
