#include "material_library.h"

#include <foundation/array.h>
#include <filesystem/filesystem_plugin.h>

MATLibrary::MATLibrary()
{}

void MATLibrary::Load( const char* filename, BXIFilesystem* filesystem, BXIAllocator* allocator )
{
    BXFileWaitResult fres = filesystem->LoadFileSync( filesystem, filename, BXEFIleMode::BIN, allocator );
    if( fres.status == BXEFileStatus::READY )
    {
        array::clear( _materials );
    }
}

void MATLibrary::Reload( BXIFilesystem* filesystem, BXIAllocator* allocator )
{

}

void MATLibrary::Tick()
{

}
