#include "anim_tool.h"
#include <foundation/debug.h>
#include <memory/memory.h>
#include <common/common.h>

#include <3rd_party/imgui/imgui.h>


void AnimTool::StartUp( const char* src_folder, const char* dst_folder, BXIAllocator* allocator )
{
    _allocator = allocator;
    string::create( &_src_folder, src_folder, allocator );
    string::create( &_dst_folder, dst_folder, allocator );
}

void AnimTool::ShutDown()
{
    BX_DELETE0( _allocator, _in_skel );
    BX_DELETE0( _allocator, _in_anim );

    _skel_blob.destroy();
    _clip_blob.destroy();
}

void AnimTool::Tick( BXIFilesystem* fs )
{
    if( _flags.refresh_src_files )
    {
        _flags.refresh_src_files = 0;
        common::RefreshFiles( &_src_file_list, _src_folder.c_str(), fs, _allocator );
    }
    if( _flags.refresh_dst_files )
    {
        _flags.refresh_dst_files = 0;
        common::RefreshFiles( &_dst_file_list, _dst_folder.c_str(), fs, _allocator );
    }

    if( _flags.request_load )
    {
        SYS_ASSERT( !IsValid( _hfile ) );
        _flags.request_load = 0;

        _hfile = fs->LoadFile( _current_file.c_str(), BXEFIleMode::BIN, _allocator );
    }
    if( IsValid( _hfile ) )
    {
        BXFile file = {};
        if( fs->File( &file, _hfile ) == BXEFileStatus::READY )
        {
            BX_DELETE0( _allocator, _in_skel );
            BX_DELETE0( _allocator, _in_anim );

            _in_skel = BX_NEW( _allocator, tool::anim::Skeleton );
            _in_anim = BX_NEW( _allocator, tool::anim::Animation );

            if( tool::anim::Import( _in_skel, _in_anim, file.bin, file.size ) )
            {
                _skel_blob.destroy();
                _clip_blob.destroy();                

                _skel_blob = tool::anim::CreateSkeleton( *_in_skel, _allocator );
                _clip_blob = tool::anim::CreateClip( *_in_anim, *_in_skel, _allocator );
            }
            fs->CloseFile( _hfile, true );
        }
    }
}

void AnimTool::DrawMenu()
{
    if( ImGui::Begin( "AnimTool" ) )
    {
        if( ImGui::BeginMenu( "Import" ) )
        {
            if( _src_file_list.null() )
            {
                _flags.refresh_src_files = 1;
            }
            else
            {
                string_buffer_it selected = common::MenuFileSelector( _src_file_list );
                if( !selected.null() && !IsValid( _hfile ) )
                {
                    string::create( &_current_file, selected.pointer, _allocator );
                    _flags.request_load = 1;
                }
            }
            ImGui::EndMenu();
        }
    }
    ImGui::End();
    
}
