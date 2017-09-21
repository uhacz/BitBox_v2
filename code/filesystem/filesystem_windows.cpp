#include "filesystem_windows.h"

#include <foundation/debug.h>
#include <foundation/queue.h>

namespace bx
{
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
		bool bres = _root.Append( absoluteDirPath );
		SYS_ASSERT( bres );
	}
	BXFileHandle FilesystemWindows::LoadFile( const char * relativePath, EMode mode )
	{
		return BXFileHandle();
	}
	void FilesystemWindows::CloseFile( BXFileHandle fhandle, bool freeData )
	{
	}
	BXFile FilesystemWindows::File( BXFileHandle fhandle )
	{
		return BXFile();
	}
	void FilesystemWindows::ThreadProcStatic( FilesystemWindows * fs )
	{
		fs->ThreadProc();
	}

	static inline bool PopFromQueueSafe( BXFileHandle* fhandle, queue_t<BXFileHandle>& queue, std::mutex& lock )
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

				FsName absolute_path;
				absolute_path.Append( _root.AbsolutePath() );
				if( absolute_path.AppendRelativePath( info._name.AbsolutePath() ) )
				{
					// load file from disk
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
				BXFileHandle fhandle = {};
				if( !PopFromQueueSafe( &fhandle, _to_unload, _to_unload_lock ) )
					break;

				const id_t id = { fhandle.i };

			}
		}
	}
}//

