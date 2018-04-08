#pragma once

#include <plugin/plugin_interface.h>
#include <stdint.h>

#define BX_FILESYSTEM_PLUGIN_NAME "filesystem"

struct BXFileHandle { uint32_t i = 0; };
inline bool IsValid( BXFileHandle h ) { return h.i != 0; }

namespace BXEFileStatus
{
	enum E : int32_t
	{
		EMPTY = 0,
		NOT_FOUND,
		LOADING,
		READY,
	};
}//

struct BXFile
{
	union
	{
		void* pointer = nullptr;
		uint8_t* bin;
		const char* txt;
	};
	uint32_t size = 0;
	BXIAllocator* allocator = nullptr;
};

struct BXFileWaitResult
{
	BXEFileStatus::E status;
	BXFileHandle handle;
	BXFile file;
};

struct BXIFilesystem;
struct BXPostLoadCallback
{
    typedef void( *CallbackFunction )(BXIFilesystem* filesystem, BXFileHandle hfile, BXEFileStatus::E status, void* user_data0, void* user_data1, void* user_data2 );
    CallbackFunction callback = nullptr;
    void* user_data0 = nullptr;
    void* user_data1 = nullptr;
    void* user_data2 = nullptr;

    BXPostLoadCallback( CallbackFunction cb, void* ud0, void* ud1 = nullptr, void* ud2 = nullptr )
        : callback( cb ), user_data0( ud0 ), user_data1( ud1 ), user_data2( ud2 ) {}

    BXPostLoadCallback() = default;
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

	virtual void			 SetRoot  ( const char* absoluteDirPath ) = 0;
	
	virtual BXFileHandle	 LoadFile ( const char* relativePath, EMode mode, BXPostLoadCallback callback, BXIAllocator* allocator = nullptr ) = 0;
    virtual BXFileHandle	 LoadFile ( const char* relativePath, EMode mode, BXIAllocator* allocator = nullptr ) { return LoadFile( relativePath, mode, BXPostLoadCallback{ nullptr,nullptr }, allocator ); }
	virtual void			 CloseFile( BXFileHandle fhandle, bool freeData = true )							  = 0;
	
	virtual BXEFileStatus::E File     ( BXFile* file, BXFileHandle fhandle )            = 0;

	BXFileWaitResult (*LoadFileSync)( BXIFilesystem* fs, const char* relativePath, EMode mode, BXIAllocator* allocator );
};



extern "C" {          // we need to export the C interface

	PLUGIN_EXPORT void* BXLoad_filesystem(BXIAllocator* allocator);
	PLUGIN_EXPORT void  BXUnload_filesystem(void* plugin, BXIAllocator* allocator);

}

