#include "anim_tool.h"
#include <foundation/debug.h>
#include <foundation/array.h>
#include <memory/memory.h>
#include <util/color.h>

#include <common/common.h>

#include <anim/anim.h>
#include <anim/anim_player.h>

#include <rdix/rdix_debug_draw.h>
#include <3rd_party/imgui/imgui.h>



void AnimTool::StartUp( const char* src_folder, const char* dst_folder, BXIAllocator* allocator )
{
    _allocator = allocator;
    string::create( &_src_folder, src_folder, allocator );
    string::create( &_dst_folder, dst_folder, allocator );
}

void AnimTool::ShutDown()
{
    string::free( &_current_src_file );
    string::free( &_dst_folder );
    string::free( &_src_folder );

    BX_DELETE0( _allocator, _in_skel );
    BX_DELETE0( _allocator, _in_anim );

    if( _player )
    {
        _player->Unprepare();
        BX_DELETE0( _allocator, _player );
    }
    BX_FREE0( _allocator, _joints_ms );

    _skel_blob.destroy();
    _clip_blob.destroy();
}



void AnimTool::Tick( BXIFilesystem* fs, float dt )
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

        _hfile = fs->LoadFile( _current_src_file.c_str(), BXEFIleMode::BIN, _allocator );
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

            if( tool::anim::Import( _in_skel, _in_anim, file.bin, file.size, _import_params ) )
            {
                _skel_blob.destroy();
                _clip_blob.destroy();                
                BX_FREE0( _allocator, _joints_ms );

                _skel_blob = tool::anim::CompileSkeleton( *_in_skel, _allocator );
                _clip_blob = tool::anim::CompileClip( *_in_anim, *_in_skel, _allocator );

                BX_DELETE0( _allocator, _player );
                _player = BX_NEW( _allocator, ANIMSimplePlayer );
                
                ANIMSkel* skel = (ANIMSkel*)_skel_blob.raw;
                ANIMClip* clip = (ANIMClip*)_clip_blob.raw;
                _player->Prepare( skel, _allocator );
                _player->Play( clip, 0.f, 0.f, 0 );

                array::clear( _matrices_ms );
                array::resize( _matrices_ms, skel->numJoints );

                _joints_ms = anim_ext::AllocateJoints( skel, _allocator );
                array::clear( _selected_joints );
            }
            fs->CloseFile( &_hfile, true );
        }
    }

    if( _player )
    {
        const ANIMSkel* skel = (ANIMSkel*)_skel_blob.raw;
        _player->Tick( dt * _time_scale );

        const ANIMJoint* joints_ls = _player->LocalJoints();

        anim_ext::LocalJointsToWorldJoints( _joints_ms, joints_ls, skel, ANIMJoint::identity() );
        anim_ext::LocalJointsToWorldMatrices( _matrices_ms.begin(), joints_ls, skel, ANIMJoint::identity() );

        const uint32_t color = color32_t::TEAL();

        const int16_t* parent_indices = ParentIndices( skel );
        const uint32_t num_joints = skel->numJoints;

        for( uint32_t i = 0; i < num_joints; ++i )
        {
            const int16_t parent_index = parent_indices[i];
            if( parent_index == -1 )
                continue;
            
            const ANIMJoint& parent = _joints_ms[parent_index];
            const ANIMJoint& joint = _joints_ms[i];

            //const mat44_t& parent_matrix = _matrices_ms[parent_index];
            //const mat44_t& child_matrix = _matrices_ms[i];

            RDIXDebug::AddLine( parent.position.xyz(), joint.position.xyz(), RDIXDebugParams( color ) );
            {
                if( array::find( _selected_joints, (int16_t)i ) != array::npos )
                {
                    RDIXDebug::AddSphere( joint.position.xyz(), _import_params.scale, RDIXDebugParams( color32_t::GREEN() ) );
                    RDIXDebug::AddAxes( toMatrix4( joint ), RDIXDebugParams().Scale( _import_params.scale * 5.f ) );
                }
            }
            //RDIXDebug::AddAxes( mat44_t( parent.rotation, parent.position ), RDIXDebugParams().Scale(0.12f) );
            //RDIXDebug::AddAxes( parent_matrix, RDIXDebugParams().Scale( 0.2f ) );
        }
    }
}

static void DrawSkeletonTree( array_t<int16_t>& selected_joints, const tool::anim::Skeleton* skel, int16_t joint_index, ImGuiTreeNodeFlags flags )
{
    const int16_t num_joints = (int16_t)skel->parentIndices.size();

    const uint32_t selected_index = array::find( selected_joints, joint_index );
    const bool is_selected = selected_index != array::npos;

    ImGuiTreeNodeFlags current_flags = flags;
    if( is_selected )
        current_flags |= ImGuiTreeNodeFlags_Selected;

    const bool opened = ImGui::TreeNodeEx( skel->jointNames[joint_index].c_str(), current_flags );
    if( ImGui::IsItemClicked(1) )
    {
        if( is_selected )
            array::erase_swap( selected_joints, selected_index );
        else
            array::push_back( selected_joints, joint_index );
    }

    if( opened )
    {
    

        for( int16_t i = joint_index + 1; i < num_joints; ++i )
        {
            const int16_t parent_index = skel->parentIndices[i];
            if( parent_index == joint_index )
            {
                DrawSkeletonTree( selected_joints, skel, i, flags );
            }
        }
        ImGui::TreePop();
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
                    string::create( &_current_src_file, selected.pointer, _allocator );
                    _flags.show_import_dialog = 1;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if( !_skel_blob.empty() )
        {
            ImGui::InputText( "Skeleton output file", _current_dst_file_skel._data, FSName::MAX_SIZE );
            if( ImGui::Button( "save" ) )
            {
                _flags.save_skel = 1;
            }
        }
        if( !_clip_blob.empty() )
        {
            ImGui::InputText( "Clip output file", _current_dst_file_clip._data, FSName::MAX_SIZE );
            if( ImGui::Button( "save" ) )
            {
                _flags.save_clip = 1;
            }
        }

        ImGui::Separator();
        ImGui::SliderFloat( "Time scale", &_time_scale, 0.f, 5.f, "%.3f", 1.f );

        float anim_time = 0.f;
        float anim_duration = 0.f;
        if( _player )
        {
            if( _player->EvalTime( &anim_time, 0 ) && _player->Duration( &anim_duration, 0 ) )
            {
                ImGui::SliderFloat( "Anim time", &anim_time, 0.f, anim_duration );

            }
        }
        if( _in_skel )
        {
            const int16_t num_joints = (int16_t)_in_skel->jointNames.size();
            if( num_joints )
            {
                if( ImGui::TreeNodeEx( _in_skel->jointNames[0].c_str(), ImGuiTreeNodeFlags_DefaultOpen ) )
                {
                    DrawSkeletonTree( _selected_joints, _in_skel, 1, 0 );
                    ImGui::TreePop();
                }
            }
            
        }
    }
    ImGui::End();

    if( _flags.show_import_dialog )
    {
        if( ImGui::Begin( "Import parameters", nullptr, ImGuiWindowFlags_Modal ) )
        {
            ImGui::Text( "filename: %s", _current_src_file.c_str() );
            ImGui::Separator();
            ImGui::InputFloat( "scale", &_import_params.scale, 0.01f, 0.1f, 3 );
            ImGui::Checkbox( "remove root motion", &_import_params.remove_root_motion );
            ImGui::Separator();

            if( ImGui::Button( "cancel" ) )
            {
                _flags.show_import_dialog = 0;
            }
            ImGui::SameLine();
            if( ImGui::Button( "import" ) )
            {
                _flags.request_load = 1;
                _flags.show_import_dialog = 0;
            }
        }        
        ImGui::End();
    }
    
}
