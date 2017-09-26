#include "filesystem_windows.h"

#include <foundation/debug.h>
#include <foundation/queue.h>
#include <foundation/io.h>

#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace bx
{

// ---
//
void FsName::Clear()
{
	_total_length = 0;
	_relative_path_offset = 0;
	_relative_path_length = 0;
	_data[0] = 0;
}
	
bool FsName::Append( const char * str )
{
	const int32_t len = (int32_t)strlen( str );
	const int32_t chars_left = MAX_LENGTH - _total_length;

	if( chars_left < len )
		return false;

	memcpy( _data + _total_length, str, len );
	_total_length += len;
	SYS_ASSERT( _total_length <= MAX_LENGTH );
	_data[_total_length] = 0;

	return true;
}

bool FsName::AppendRelativePath( const char * str )
{
	const uint32_t offset = _total_length;
	if( Append( str ) )
	{
		_relative_path_offset = offset;
		_relative_path_length = _total_length - offset;
		return true;
	}
	return false;
}

// ---
//
FilesystemWindows::FilesystemWindows( BXIAllocator* allocator )
	: _to_load( allocator )
	, _to_unload( allocator )
	, _allocator( allocator )
{
}
bool FilesystemWindows::Startup()
{
	_is_running = 1;
	_thread = std::thread( ThreadProcStatic, this );

	return true;
}
void FilesystemWindows::Shutdown()
{
	_is_running = 0;
	_semaphore.signal();
	_thread.join();
}
void FilesystemWindows::SetRoot( const char * absoluteDirPath )
{
	_root.Clear();
	bool bres = _root.Append( absoluteDirPath );
	SYS_ASSERT( bres );
}

bool FilesystemWindows::IsValid( BXFileHandle fhandle )
{
	const id_t id = { fhandle.i };
	return id_table::has( _ids, id );
}

BXFileHandle FilesystemWindows::LoadFile( const char* relativePath, EMode mode )
{
	id_t id = { 0 };
	
	_id_lock.lock();
	id = id_table::create( _ids );
	_id_lock.unlock();

	FileInputInfo& info = _input_info[id.index];
	info._mode = mode;
	info._name.Clear();
	info._name.AppendRelativePath( relativePath );

	BXFileHandle fhandle = { id.hash };

	_to_load_lock.lock();
	queue::push_back( _to_load, fhandle );
	_to_load_lock.unlock();
	_semaphore.signal();

	return fhandle;
}

void FilesystemWindows::CloseFile( BXFileHandle fhandle, bool freeData )
{
	if( !IsValid( fhandle ) )
		return;

	const id_t id = { fhandle.i };
	BXFile file = _files[id.index];

	_id_lock.lock();
	id_table::destroy( _ids, id );
	_id_lock.unlock();

	_to_unload_lock.lock();
	queue::push_back( _to_unload, file );
	_to_unload_lock.unlock();
	_semaphore.signal();
}

BXFile FilesystemWindows::File( BXFileHandle fhandle )
{
	if( !IsValid( fhandle ) )
		return BXFile();

	const id_t id = { fhandle.i };
	return _files[id.index];
}

void FilesystemWindows::ThreadProcStatic( FilesystemWindows* fs )
{
	fs->ThreadProc();
}

template< typename T>
static inline bool PopFromQueueSafe( T* fhandle, queue_t<T>& queue, std::mutex& lock )
{
	bool result = false;
	lock.lock();
	if( result = !queue::empty( queue ) )
	{
		fhandle[0] = queue::front( queue );
		queue::pop_front( queue );
	}
	lock.unlock();

	return result;
}

void FilesystemWindows::ThreadProc()
{
	while( _is_running )
	{
		_semaphore.wait();

		// load files
		while( true )
		{
			BXFileHandle fhandle = {};
			if( !PopFromQueueSafe( &fhandle, _to_load, _to_load_lock ) )
				break;
				
			const id_t id = { fhandle.i };
			const FileInputInfo& info = _input_info[id.index];

			FsName path;
			path.Append( _root.AbsolutePath() );
			if( path.AppendRelativePath( info._name.AbsolutePath() ) )
			{
				BXFile& file = _files[id.index];
				int32_t result = IO_ERROR;
				
				if( info._mode == BXIFilesystem::FILE_MODE_BIN )
					result = ReadFile( &file.bin, &file.size, path.AbsolutePath(), _allocator );
				else if( info._mode == BXIFilesystem::FILE_MODE_TXT )
					result = ReadTextFile( &file.bin, &file.size, path.AbsolutePath(), _allocator );

				if( result == IO_OK )
					InterlockedExchange( &file.status, (LONG)BXFile::STATUS_READY );
				else
					InterlockedExchange( &file.status, (LONG)BXFile::STATUS_NOT_FOUND );
			}
			else
			{
				SYS_LOG_ERROR( "Filesystem: path '%s' is to long", info._name.AbsolutePath() );
				CloseFile( fhandle, false );
			}
		}

		// close files
		while( true )
		{
			BXFile file = {};
			if( !PopFromQueueSafe( &file, _to_unload, _to_unload_lock ) )
				break;

			BX_FREE( _allocator, file.pointer );

		}
	}
}

}//

