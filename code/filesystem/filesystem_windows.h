#pragma once

#include "filesystem_plugin.h"

#include <foundation/debug.h>
#include <foundation/id_table.h>
#include <foundation/thread/semaphore.h>
#include <thread>
#include <mutex>
#include <atomic>

namespace bx
{


	struct FsName
	{
		enum
		{
			MAX_LENGTH = 255,
			MAX_SIZE = MAX_LENGTH + 1,
		};
		
		char _data[MAX_SIZE] = {};

		uint32_t _total_length = 0;
		uint32_t _relative_path_offset = 0;
		uint32_t _relative_path_length = 0;

		void Clear();
		bool Empty() const { return _total_length == 0; }

		bool Append				( const char* str );
		bool AppendRelativePath	( const char* str );

		const char* RelativePath      () const { return _data + _relative_path_offset; }
		uint32_t	RelativePathLength() const { return _relative_path_length; }
		const char* AbsolutePath      () const { return _data; }
	};

	struct FileInputInfo
	{
		FsName _name;
		BXIFilesystem::EMode _mode;
	};

struct FilesystemWindows : BXIFilesystem
{
	FilesystemWindows( BXIAllocator* allocator );

	bool		 Startup();
	void		 Shutdown();
	// --- interface
	bool			 IsValid( BXFileHandle fhandle );
	void			 SetRoot( const char* absoluteDirPath ) override final;
	BXFileHandle	 LoadFile( const char* relativePath, EMode mode ) override final;
	void			 CloseFile( BXFileHandle fhandle, bool freeData ) override final;
	BXEFileStatus::E File( BXFile* file, BXFileHandle fhandle ) override final;

	// ---
	static void ThreadProcStatic( FilesystemWindows* fs );
	void ThreadProc();

	// ---

	// --- data
	enum
	{
		MAX_HANDLES = 1024,
		MAX_PATHS = 32,
	};
	using IdManager = id_table_t< MAX_HANDLES >;

	std::thread				_thread;
	light_semaphore_t		_semaphore;
	std::atomic_uint32_t	_is_running = 0;
	
	IdManager     _ids;
	std::mutex    _id_lock;
	
	FileInputInfo		_input_info  [MAX_HANDLES] = {};
	BXFile				_files       [MAX_HANDLES] = {};
	std::atomic_int32_t _files_status[MAX_HANDLES] = {};

	queue_t<BXFileHandle>  _to_load;
	queue_t<BXFile>		   _to_unload;

	std::mutex _to_load_lock;
	std::mutex _to_unload_lock;

	FsName		  _root;
	BXIAllocator* _allocator = nullptr;
};

}// 
