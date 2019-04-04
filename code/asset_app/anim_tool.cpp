#include "anim_tool.h"
#include <foundation/debug.h>
#include <foundation/array.h>
#include <memory/memory.h>
#include <util/color.h>

#include <anim/anim.h>
#include <anim/anim_player.h>

#include <gfx/gfx.h>

#include <rdix/rdix_debug_draw.h>
#include <3rd_party/imgui/imgui.h>
#include "common/base_engine_init.h"
#include "tool_context.h"
#include "components.h"

#include <common/common.h>
#include "foundation/serializer.h"
#include "rdix/rdix_type.h"
#include "anim/anim_debug.h"

void ANIMTool::StartUp( CMNEngine* e, const char* src_folder, BXIAllocator* allocator )
{
    _allocator = allocator;
    _root_src_folder_ctx.SetUp( e->filesystem, allocator );
    _root_src_folder_ctx.SetFolder( src_folder );
}

void ANIMTool::ShutDown( CMNEngine* e )
{
    e->ecs->MarkForDestroy( _anim_desc_component );
    UnloadTOOLMeshComponent( e, _mesh_comp );

    string::free( &_current_src_file );

    BX_DELETE0( _allocator, _in_skel );
    BX_DELETE0( _allocator, _in_anim );

    if( _player )
    {
        _player->Unprepare();
        BX_DELETE0( _allocator, _player );
    }
    BX_FREE0( _allocator, _joints_ms );

    BX_FREE0( _allocator, _skel_file );
    BX_FREE0( _allocator, _clip_file );
}

void ANIMTool::Tick( CMNEngine* e, const TOOLContext& ctx, float dt )
{
    ECSComponentProxy<TOOLAnimDescComponent> desc_comp = common::CreateComponentIfNotExists< TOOLAnimDescComponent >( e->ecs, ctx.entity );
    _anim_desc_component = desc_comp.Id();

    DrawMenu( e, ctx );

    BXIFilesystem* fs = e->filesystem;

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
            BX_RENEW( _allocator, &_in_skel );
            BX_RENEW( _allocator, &_in_anim );

            if( tool::anim::Import( _in_skel, _in_anim, file.bin, file.size, _import_params ) )
            {
                common::CreateDstFilename( &_current_dst_file_skel, ctx.folders->anim.Root(), _current_src_file, "skel" );
                common::CreateDstFilename( &_current_dst_file_clip, ctx.folders->anim.Root(), _current_src_file, "clip" );

                BX_FREE0( _allocator, _skel_file );
                BX_FREE0( _allocator, _clip_file );
                BX_FREE0( _allocator, _joints_ms );

                blob_t skel_blob = tool::anim::CompileSkeleton( *_in_skel, _allocator, tool::anim::SKEL_CLO_INCLUDE_STRING_NAMES );
                blob_t clip_blob = tool::anim::CompileClip( *_in_anim, *_in_skel, _allocator );

                _skel_file = srl_file::serialize<ANIMSkel>( skel_blob, _allocator );
                _clip_file = srl_file::serialize<ANIMClip>( clip_blob, _allocator );

                skel_blob.destroy();
                clip_blob.destroy();


                if( _player )
                    _player->Unprepare();

                BX_RENEW( _allocator, &_player );

                const ANIMSkel* skel = _skel_file->data<ANIMSkel>();
                const ANIMClip* clip = _clip_file->data<ANIMClip>();
                _player->Prepare( skel, _allocator );
                _player->Play( clip, 0.f, 0.f, 0 );

                array::clear( _matrices_ms );
                array::resize( _matrices_ms, skel->numJoints );

                _joints_ms = anim_ext::AllocateJoints( skel, _allocator );
                array::clear( _selected_joints );

                desc_comp->Initialize( skel );

                ECSEntityProxy eproxy( e->ecs, ctx.entity );
                ECSComponentProxy<TOOLSkinningComponent> skinning( eproxy );
                if( skinning )
                {
                    ECSComponentID mesh_desc_id = eproxy.Lookup<TOOLMeshDescComponent>();
                    skinning->Initialize( e->ecs, mesh_desc_id );
                }
            }
            fs->CloseFile( &_hfile, true );
        }
    }

    if( _flags.save_skel )
    {
        _flags.save_skel = 0;
        WriteFileSync( e->filesystem, _current_dst_file_skel.AbsolutePath(), _skel_file, _skel_file->size );
        ctx.folders->anim.RequestRefresh();
    }
    if( _flags.save_clip )
    {
        _flags.save_clip = 0;
        WriteFileSync( e->filesystem, _current_dst_file_clip.AbsolutePath(), _clip_file, _clip_file->size );
        ctx.folders->anim.RequestRefresh();
    }

    if( _player )
    {
        const ANIMSkel* skel = _skel_file->data<ANIMSkel>();
        _player->Tick( dt * _time_scale );

        const ANIMJoint* joints_ls = _player->LocalJoints();

        const mat44_t entity_pose = common::GetEntityRootTransform( e->ecs, ctx.entity );

        const ANIMJoint root_joint = toAnimJoint_noScale( entity_pose );
        anim_ext::LocalJointsToWorldJoints( _joints_ms, joints_ls, skel, root_joint );
        anim_ext::LocalJointsToWorldMatrices( _matrices_ms.begin(), joints_ls, skel, ANIMJoint::identity() );

        ECSEntityProxy eproxy( e->ecs, ctx.entity );
        ECSComponentProxy<TOOLSkinningComponent> skinning_comp( eproxy );
        if( skinning_comp )
        {
            blob_t skinning_data = e->gfx->AcquireSkinnigDataToWrite( skinning_comp->id_mesh, skinning_comp->bone_offsets.size * sizeof( mat44_t ) );
            array_span_t<mat44_t> skinning_matrices = to_array_span<mat44_t>( skinning_data, skinning_comp->bone_offsets.size );

            skinning_comp->ComputeSkinningMatrices( skinning_matrices, array_span_t<const mat44_t>( _matrices_ms.begin(), _matrices_ms.end() ) );
        }

        const uint32_t color = color32_t::TEAL();
        const f32 debug_scale = 0.12f;
        
        anim_debug::DrawPose( skel, to_array_span( _joints_ms, skel->numJoints ), color, debug_scale );
        
        const int16_t* parent_indices = ParentIndices( skel );
        const uint32_t num_joints = skel->numJoints;

        for( uint32_t i : _selected_joints )
        {
            const ANIMJoint& joint = _joints_ms[i];
            RDIXDebug::AddSphere( joint.position.xyz(), debug_scale * 0.5f, RDIXDebugParams( color32_t::GREEN() ) );
            RDIXDebug::AddAxes( toMatrix4( joint ), RDIXDebugParams().Scale( debug_scale * 5.f ) );
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
    
    const char* name = skel->jointNames[joint_index].c_str();
    const bool opened = ImGui::TreeNodeEx( name, current_flags, "%d: %s", joint_index, name );
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

void ANIMTool::DrawMenu( CMNEngine* e, const TOOLContext& ctx )
{
    if( ImGui::Begin( "AnimTool", nullptr, ImGuiWindowFlags_MenuBar ) )
    {
        if( ImGui::BeginMenuBar() )
        {
            if( ImGui::BeginMenu( "Import" ) )
            {
                string_buffer_it selected = common::MenuFileSelector( _root_src_folder_ctx.FileList() );
                if( !selected.null() && !IsValid( _hfile ) )
                {
                    string::create( &_current_src_file, selected.pointer, _allocator );
                    _flags.show_import_dialog = 1;
                }
                ImGui::EndMenu();
            }
            if( ImGui::BeginMenu( "Load mesh" ) )
            {
                string_buffer_it selected = common::MenuFileSelector( ctx.folders->mesh.FileList() );
                if( !selected.null() )
                {
                    common::AssetFileHelper<RDIXMeshFile> helper;
                    if( helper.LoadSync( e->filesystem, _allocator, selected.pointer ) )
                    {
                        _mesh_comp = LoadTOOLMeshComponent( e, _mesh_comp, ctx, helper.data, _allocator );
                        helper.FreeFile();
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        
        if( _skel_file )
        {
            ImGui::InputText( "Skeleton output file", _current_dst_file_skel._data, FSName::MAX_SIZE );
            if( ImGui::Button( "Save skel" ) )
            {
                _flags.save_skel = 1;
            }
        }
        if( _clip_file )
        {
            ImGui::InputText( "Clip output file", _current_dst_file_clip._data, FSName::MAX_SIZE );
            if( ImGui::Button( "Save clip" ) )
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
                const u32 flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;
                if( ImGui::TreeNodeEx( _in_skel->jointNames[0].c_str(), flags ) )
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
