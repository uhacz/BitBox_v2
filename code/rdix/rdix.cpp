#include "rdix.h"

#include <filesystem/filesystem_plugin.h>

RDIXShaderFile* LoadShaderFile( const char* name, BXIFilesystem* filesystem, BXIAllocator* allocator )
{
	BXFileWaitResult load_result = filesystem->LoadFileSync( filesystem, name, BXIFilesystem::FILE_MODE_BIN, allocator );
	return LoadShaderFile( load_result.file.pointer, load_result.file.size );
}

RDIXShaderFile* LoadShaderFile( const void * data, uint32_t dataSize )
{
	RDIXShaderFile* sfile = (RDIXShaderFile*)data;
	if( sfile->tag != tag32_t( "SF01" ) )
	{
		SYS_LOG_ERROR( "ShaderFile: wrong file tag!" );
		return nullptr;
	}

	if( sfile->version != RDIXShaderFile::VERSION )
	{
		SYS_LOG_ERROR( "ShaderFile: wrong version!" );
		return nullptr;
	}
		
	return sfile;
}

void UnloadShaderFile( RDIXShaderFile ** shaderfile )
{
}
