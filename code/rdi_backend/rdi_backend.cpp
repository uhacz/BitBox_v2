#include "rdi_backend.h"
#include "rdi_backend_dx11.h"


namespace bx{ namespace rdi{ 


EDataType::Enum EDataType::FromName( const char* name )
{
    for( int itype = 0; itype < EDataType::COUNT; ++itype )
    {
        if( !strcmp( name, EDataType::name[itype] ) )
            return (EDataType::Enum)itype;
    }
    return EDataType::UNKNOWN;
}

EDataType::Enum EDataType::FindBaseType( const char* name )
{
    for( int itype = 0; itype < EDataType::COUNT; ++itype )
    {
        if( strstr( name, EDataType::name[itype] ) )
            return (EDataType::Enum)itype;
    }
    return EDataType::UNKNOWN;
}


EVertexSlot::Enum EVertexSlot::FromString( const char* n )
{
    for( int i = 0; i < EVertexSlot::COUNT; ++i )
    {
        if( !strcmp( n, EVertexSlot::name[i] ) )
            return ( EVertexSlot::Enum )i;
    }

    return EVertexSlot::COUNT;
}

void Startup( Device** dev, CommandQueue** cmdq, uintptr_t hWnd, int winWidth, int winHeight, int fullScreen, BXIAllocator* allocator )
{
    StartupDX11( dev, cmdq, hWnd, winWidth, winHeight, fullScreen, allocator );
}

void Shutdown( Device** dev, CommandQueue** cmdq, BXIAllocator* allocator )
{
    ShutdownDX11( dev, cmdq, allocator );
}

}
}///