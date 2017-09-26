#pragma once

#include <foundation/plugin/plugin_interface.h>
#include <stdint.h>

#define BX_FILESYSTEM_PLUGIN_NAME "filesystem"

struct BXFileHandle { uint32_t i = 0; };
inline bool IsValid( BXFileHandle h ) { return h.i != 0; }

struct BXFile
{
	enum EStatus : int32_t
	{
		STATUS_NOT_FOUND = -1,
		STATUS_READY = 0,
		STATUS_LOADING,
		STATUS_EMPTY,
	};

	union
	{
		void* pointer = nullptr;
		uint8_t* bin;
		const char* txt;
	};

	uint32_t size = 0;
	volatile long status = STATUS_EMPTY;
};

// --- 
struct BXIFilesystem
{
	enum EMode : int32_t
	{
		FILE_MODE_TXT = 0,
		FILE_MODE_BIN,
	};

	virtual ~BXIFilesystem() {}

	virtual void		 SetRoot  ( const char* absoluteDirPath )          = 0;
	virtual BXFileHandle LoadFile ( const char* relativePath, EMode mode ) = 0;
	virtual void		 CloseFile( BXFileHandle fhandle, bool freeData)   = 0;
	virtual BXFile		 File     ( BXFileHandle fhandle)                  = 0;

};

extern "C" {          // we need to export the C interface

	PLUGIN_EXPORT void* BXLoad_filesystem(BXIAllocator* allocator);
	PLUGIN_EXPORT void  BXUnload_filesystem(void* plugin, BXIAllocator* allocator);

}

