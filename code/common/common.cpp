#include "common.h"

#include <foundation/common.h>
#include <foundation/static_array.h>
#include <filesystem/filesystem_plugin.h>

#include <3rd_party/imgui/imgui.h>

namespace common
{


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

}//