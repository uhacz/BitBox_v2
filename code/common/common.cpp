#include "common.h"

#include <foundation/common.h>
#include <foundation/static_array.h>
#include <filesystem/filesystem_plugin.h>

#include <3rd_party/imgui/imgui.h>
#include <stdio.h>
#include "foundation/serializer.h"

namespace common
{
    void FolderContext::SetUp( BXIFilesystem* fs, BXIAllocator* allocator )
    {
        _filesystem = fs;
        _allocator = allocator;
    }

    void FolderContext::RequestRefresh()
    {
        _flag_refresh_file_list = 1;
    }

    void FolderContext::SetFolder( const char* folder )
    {
        _flag_refresh_file_list = !string::equal( _folder.c_str(), folder );
        string::create( &_folder, folder, _allocator );
    }

    const string_buffer_t& FolderContext::FileList() const
    {
        if( _flag_refresh_file_list )
        {
            _flag_refresh_file_list = 0;
            common::RefreshFiles( &_file_list, _folder.c_str(), _filesystem, _allocator );
        }

        return _file_list;
    }

    void RefreshFiles( string_buffer_t* flist, const char* folder, BXIFilesystem* fs, BXIAllocator* allocator )
    {
        const uint32_t initial_capacity = max_of_2( 32u, flist->_capacity );
        string::create( flist, initial_capacity, allocator );
        fs->ListFiles( fs, flist, folder, BXEFileListFlag::RECURSE, allocator );
    }

    string_buffer_it MenuFileSelector( const string_buffer_t& file_list )
    {
        static_array_t<uint8_t, 255> menu_enter_stack;

        string_buffer_it file_it = string::iterate( file_list, string_buffer_it() );
        while( !file_it.null() )
        {
            const uint8_t last_menu_entered = (array::empty( menu_enter_stack )) ? 1 : array::back( menu_enter_stack );

            const char* type = file_it.pointer;
            if( string::equal( type, "D+" ) )
            {
                file_it = string::iterate( file_list, file_it );

                const uint8_t entered = ImGui::BeginMenu( file_it.pointer );
                array::push_back( menu_enter_stack, entered );
            }
            else if( string::equal( type, "D-" ) )
            {
                array::pop_back( menu_enter_stack );
                if( last_menu_entered )
                {
                    ImGui::EndMenu();
                }
            }
            else if( string::equal( type, "F" ) && last_menu_entered )
            {
                file_it = string::iterate( file_list, file_it );

                bool selected = false;
                if( ImGui::MenuItem( file_it.pointer, nullptr, &selected ) )
                {
                    if( selected )
                    {
                        break;
                    }
                }
            }
            file_it = string::iterate( file_list, file_it );
        }

        while( !array::empty( menu_enter_stack ) )
        {
            const uint8_t entered = array::back( menu_enter_stack );
            array::pop_back( menu_enter_stack );

            if( entered )
            {
                ImGui::EndMenu();
            }
        }

        return file_it;
    }


    static inline uint32_t find_r( const char* begin, uint32_t len, const char needle )
    {
        int32_t pos = len;
        while( pos )
        {
            if( begin[pos] == needle )
                break;

            --pos;
        }
        return pos;
    }

    void CreateDstFilename( FSName* dst_file, const char* dst_folder, const string_t& src_file, const char* ext )
    {
        constexpr char d = '.';
        constexpr char s = '/';

        const uint32_t src_len = src_file.length();
        const uint32_t s_pos = find_r( src_file.c_str(), src_len, s );

        char tmp[FSName::MAX_LENGTH] = {};

        const char* src_name = src_file.c_str();
        if( s_pos != UINT32_MAX )
            src_name += s_pos + 1;

        uint32_t len = snprintf( tmp, FSName::MAX_LENGTH, "%s", src_name );

        const uint32_t d_pos = find_r( tmp, len, d );
        if( d_pos != UINT32_MAX )
        {
            snprintf( tmp + d_pos + 1, FSName::MAX_LENGTH - (len + d_pos + 1), ext );
        }

        dst_file->Clear();
        dst_file->Append( dst_folder );
        dst_file->AppendRelativePath( tmp );
    }

    void CreateDstFilename( string_t* dst_file, const char* dst_folder, const string_t& src_file, const char* ext, BXIAllocator* allocator )
    {
        FSName tmp;
        CreateDstFilename( &tmp, dst_folder, src_file, ext );
        string::create( dst_file, tmp.AbsolutePath(), allocator );
    }

       

}//

namespace common
{


}