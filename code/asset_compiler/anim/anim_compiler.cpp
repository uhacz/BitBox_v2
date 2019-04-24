#include "anim_compiler.h"

#include <foundation/hash.h>
#include <foundation/tag.h>
#include <foundation/debug.h>
#include <foundation/common.h>
#include <foundation/io.h>
#include <foundation/math/vmath.h>
#include <foundation/hashed_string.h>
#include <foundation/string_util.h>
#include <memory/memory.h>
#include <filesystem/filesystem_plugin.h>

#include <anim/anim.h>
#include <iostream>

#include <3rd_party/assimp/cimport.h>
#include <3rd_party/assimp/scene.h>
#include "foundation/buffer.h"


static inline float4_t toFloat4( const aiVector3D& v, float w = 1.f )
{
    return float4_t( v.x, v.y, v.z, w );
}

static inline float4_t toFloat4( const aiQuaternion& q )
{
    return float4_t( q.x, q.y, q.z, q.w );
}

inline float4_t Mul4( const float4_t& f4, float f )
{
    return float4_t( f4.x * f, f4.y * f, f4.z * f, f4.w * f );
}
inline float4_t Mul3( const float4_t& f4, float f )
{
    return float4_t( f4.x * f, f4.y * f, f4.z * f, f4.w );
}


namespace tool{ namespace anim {
 
    static void _ReadNode( Skeleton* skel, uint16_t* current_index, uint16_t parent_index, aiNode* node )
    {
        //const uint32_t name_hash = simple_hash( node->mName.C_Str() );
        if( node->mNumMeshes )
            return;

        aiVector3D translation;
        aiQuaternion rotation;
        aiVector3D scale;
        node->mTransformation.Decompose( scale, rotation, translation );

        Joint joint;
        joint.translation = toFloat4( translation );
        joint.rotation = toFloat4( rotation );
        joint.scale = toFloat4( scale );

        skel->jointNames.emplace_back( node->mName.C_Str() );
        skel->basePose.push_back( joint );
        skel->parentIndices.push_back( parent_index );

        int32_t parent_for_children = *current_index;
        *current_index = parent_for_children + 1;

        for( uint32_t i = 0; i < node->mNumChildren; ++i )
        {
            _ReadNode( skel, current_index, parent_for_children, node->mChildren[i] );
        }

    }

    static void _ExtractSkeleton( Skeleton* skel, const aiScene* scene )
    {
        uint16_t index = 0;
        _ReadNode( skel, &index, 0xFFFF, scene->mRootNode );
    }

    static inline uint32_t _FindJointAnimation( const char* joint_name, const aiAnimation* animation )
    {
        const uint32_t n = animation->mNumChannels;
        for( uint32_t i = 0; i < n; ++i )
        {
            if( string::equal( animation->mChannels[i]->mNodeName.C_Str(), joint_name ) )
                return i;
        }

        return UINT32_MAX;
    }
    static void _ExtractAnimation( Animation* anim, const Skeleton& skel, const aiScene* scene )
    {
        aiAnimation* animation = scene->mAnimations[0];

        anim->startTime = 0.f;

        /// because animation->mDuration is number of frames
        anim->endTime = (float)(animation->mDuration / animation->mTicksPerSecond);
        anim->sampleFrequency = (float)animation->mTicksPerSecond;
        const float dt = (float)(1.0 / animation->mTicksPerSecond);

        const uint32_t num_joints = (uint32_t)skel.jointNames.size();
        for( uint32_t i = 0; i < num_joints; ++i )
        {
            anim->joints.push_back( JointAnimation() );
            JointAnimation& janim = anim->joints.back();

            janim.name = skel.jointNames[i];
            janim.weight = 1.f;
        }

        uint32_t max_key_frames = 0;

        for( uint32_t i = 0; i < num_joints; ++i )
        {
            JointAnimation& janim = anim->joints[i];

            const uint32_t node_index = _FindJointAnimation( janim.name.c_str(), animation );
            if( node_index == UINT32_MAX )
                continue;

            const aiNodeAnim* node_anim = animation->mChannels[node_index];
            //SYS_ASSERT( node_anim->mNumPositionKeys == node_anim->mNumRotationKeys );
            //SYS_ASSERT( node_anim->mNumPositionKeys == node_anim->mNumScalingKeys );
            max_key_frames = max_of_2( max_key_frames, node_anim->mNumRotationKeys );
            max_key_frames = max_of_2( max_key_frames, node_anim->mNumPositionKeys );
            max_key_frames = max_of_2( max_key_frames, node_anim->mNumScalingKeys );

            for( uint32_t j = 0; j < node_anim->mNumRotationKeys; ++j )
            {
                const aiQuatKey& node_key = node_anim->mRotationKeys[j];
                aiQuaternion rotation = node_key.mValue;
                rotation.Normalize();

                janim.rotation.push_back( AnimKeyframe() );
                AnimKeyframe& key = janim.rotation.back();

                key.data = toFloat4( rotation );
                key.time = static_cast<float>(node_key.mTime * dt);
            }

            for( uint32_t j = 0; j < node_anim->mNumPositionKeys; ++j )
            {
                const aiVectorKey& node_key = node_anim->mPositionKeys[j];

                janim.translation.push_back( AnimKeyframe() );
                AnimKeyframe& key = janim.translation.back();

                key.data = toFloat4( node_key.mValue );
                key.time = static_cast<float>(node_key.mTime * dt);
            }

            for( uint32_t j = 0; j < node_anim->mNumScalingKeys; ++j )
            {
                const aiVectorKey& node_key = node_anim->mScalingKeys[j];

                janim.scale.push_back( AnimKeyframe() );
                AnimKeyframe& key = janim.scale.back();

                key.data = toFloat4( node_key.mValue );
                key.time = static_cast<float>(node_key.mTime * dt);
            }
        }

        anim->numFrames = max_key_frames;

        for( uint32_t i = 0; i < num_joints; ++i )
        {
            JointAnimation& janim = anim->joints[i];
            {
                const uint32_t n_rotations = max_key_frames - (uint32_t)janim.rotation.size();
                const uint32_t n_translations = max_key_frames - (uint32_t)janim.translation.size();
                const uint32_t n_scale = max_key_frames - (uint32_t)janim.scale.size();
                

                AnimKeyframe rotation_keyframe;
                AnimKeyframe translation_keyframe;
                AnimKeyframe scale_keyframe;

                rotation_keyframe.data = (janim.rotation.empty()) ? skel.basePose[i].rotation : janim.rotation.back().data;
                translation_keyframe.data = (janim.translation.empty()) ? skel.basePose[i].translation : janim.translation.back().data;
                scale_keyframe.data = (janim.scale.empty()) ? skel.basePose[i].scale : janim.scale.back().data;

                rotation_keyframe.time = (janim.rotation.empty()) ? 0.f : janim.rotation.back().time + dt;
                translation_keyframe.time = (janim.translation.empty()) ? 0.f : janim.translation.back().time + dt;
                scale_keyframe.time = (janim.scale.empty()) ? 0.f : janim.scale.back().time + dt;



                for( uint32_t j = 0; j < n_rotations; ++j )
                {
                    janim.rotation.push_back( rotation_keyframe );
                    rotation_keyframe.time += dt;
                }
                for( uint32_t j = 0; j < n_translations; ++j )
                {
                    janim.translation.push_back( translation_keyframe );
                    translation_keyframe.time += dt;
                }
                for( uint32_t j = 0; j < n_scale; ++j )
                {
                    janim.scale.push_back( scale_keyframe );
                    scale_keyframe.time += dt;
                }
            }
        }
    }
}}///

namespace tool{ namespace anim {
    //////////////////////////////////////////////////////////////////////////
    ///
    inline void Reserve( JointAnimation* janim, u32 size )
    {
        const u32 new_size = (u32)janim->rotation.size() + size;
        janim->rotation.reserve( new_size );
        janim->translation.reserve( new_size );
        janim->scale.reserve( new_size );
    }
    inline u32 Size( const JointAnimation& janim )
    {
        SYS_ASSERT( janim.rotation.size() == janim.translation.size() );
        SYS_ASSERT( janim.rotation.size() == janim.scale.size() );

        return (u32)janim.rotation.size();
    }

    inline void* AllocateMemory( BXIAllocator* allocator, uint32_t memSize, uint32_t alignment )
    {
        return BX_MALLOC( allocator, memSize, alignment );
    }
    inline void FreeMemory0( BXIAllocator* allocator, uint8_t*& mem )
    {
        BX_FREE0( allocator, mem );
    }

    bool Import( Skeleton* skeleton, Animation* animation, const void* data, uint32_t data_size, const ImportParams& params )
    {
        const aiScene* scene = aiImportFileFromMemory( (const char*)data, data_size, 0, nullptr );
        if( scene )
        {
            _ExtractSkeleton( skeleton, scene );
            _ExtractAnimation( animation, *skeleton, scene );
        
            aiReleaseImport( scene );

            if( params.scale != 1.0f )
            {
                for( Joint& joint : skeleton->basePose )
                {
                    joint.translation = Mul3( joint.translation, params.scale );
                }

                for( JointAnimation& janim : animation->joints )
                {
                    for( AnimKeyframe& key : janim.translation )
                    {
                        key.data = Mul3( key.data, params.scale );
                    }
                }
            }


            if( (size_t)params.root_motion_joint < skeleton->basePose.size() )
            {
                
                if( params.extract_root_motion )
                {
                    const JointAnimation& root_anim = animation->joints[params.root_motion_joint];
                    JointAnimation& root_motion = animation->root_motion;
                    Reserve( &root_motion, animation->numFrames );
                    const int nFrames = (int)root_anim.translation.size();
                    for( int iframe = 0; iframe < nFrames; ++iframe )
                    {
                        AnimKeyframe root_anim_t = root_anim.translation[iframe];
                        AnimKeyframe root_anim_r = root_anim.rotation[iframe];
                        AnimKeyframe root_anim_s = root_anim.scale[iframe];
                        
                        root_anim_t.data.y = 0.f;
                        
                        root_motion.translation.push_back( root_anim_t );
                        root_motion.rotation.push_back( root_anim_r );
                        root_motion.scale.push_back( root_anim_s );
                    }
                }

                if( params.remove_root_motion )
                {
                    JointAnimation& root_anim = animation->joints[params.root_motion_joint];
                    const int nFrames = (int)root_anim.translation.size();
                    for( int iframe = 0; iframe < nFrames; ++iframe )
                    {
                        float4_t& translation = root_anim.translation[iframe].data;
                        translation.z = 0.f;
                        translation.x = 0.f;
                    }
                    
                }
            }

            if( params.strip_namespace_name )
            {
                for( std::string& name : skeleton->jointNames )
                {
                    const size_t separator_pos = name.rfind( ':' );
                    if( separator_pos != std::string::npos )
                    {
                        name = name.substr( separator_pos + 1 );
                    }
                }
            }

            return true;
        }

        return false;
    }

    blob_t CompileSkeleton( const Skeleton& in_skeleton, BXIAllocator* allocator, u32 flags )
    {
        static_assert(sizeof( Joint ) == sizeof( ANIMJoint ), "joint size mismatch");

        const uint32_t num_joints = (uint32_t)in_skeleton.jointNames.size();
        const uint32_t parent_indices_size = num_joints * sizeof( uint16_t );
        const uint32_t base_pose_size = num_joints * sizeof( Joint );
        const uint32_t joint_name_hashes_size = num_joints * sizeof( hashed_string_t );

        u32 joint_name_strings_size = 0;
        if( flags & SKEL_CLO_INCLUDE_STRING_NAMES )
        {
            for( u32 i = 0; i < num_joints; ++i )
            {
                joint_name_strings_size += (u32)in_skeleton.jointNames[i].size() + 1;
            }
        }

        uint32_t memory_size = 0;
        memory_size += sizeof( ANIMSkel );
        memory_size += base_pose_size;
        memory_size += parent_indices_size;
        memory_size += joint_name_hashes_size;
        memory_size += joint_name_strings_size;

        blob_t blob = blob_t::allocate( allocator, memory_size, 16 );

        BufferChunker chunker( blob.raw, (u32)blob.size );

        ANIMSkel* out_skeleton = chunker.Add<ANIMSkel>();
        memset( out_skeleton, 0, sizeof( ANIMSkel ) );

        uint8_t* base_pose_address = chunker.Add<u8>( base_pose_size );
        uint8_t* parent_indices_address = chunker.Add<u8>( parent_indices_size );
        uint8_t* joint_name_hashes_address = chunker.Add<u8>( joint_name_hashes_size );
        
        out_skeleton->numJoints = num_joints;
        out_skeleton->offsetBasePose = TYPE_POINTER_GET_OFFSET( &out_skeleton->offsetBasePose, base_pose_address );
        out_skeleton->offsetParentIndices = TYPE_POINTER_GET_OFFSET( &out_skeleton->offsetParentIndices, parent_indices_address );
        out_skeleton->offsetJointNames = TYPE_POINTER_GET_OFFSET( &out_skeleton->offsetJointNames, joint_name_hashes_address );

        std::vector< ANIMJoint > world_bind_pose;
        world_bind_pose.resize( num_joints );
        const ANIMJoint* local_bind_pose = (ANIMJoint*)in_skeleton.basePose.data();
        const uint16_t* parent_indices = in_skeleton.parentIndices.data();
        LocalJointsToWorldJoints( world_bind_pose.data(), local_bind_pose, parent_indices, num_joints, ANIMJoint::identity() );

        memcpy( base_pose_address, world_bind_pose.data(), base_pose_size );
        memcpy( parent_indices_address, &in_skeleton.parentIndices[0], parent_indices_size );

        hashed_string_t* joint_name_hashes = (hashed_string_t*)joint_name_hashes_address;
        for( size_t i = 0; i < in_skeleton.jointNames.size(); ++i )
        {
            joint_name_hashes[0] = hashed_string( in_skeleton.jointNames[i].c_str() );
            ++joint_name_hashes;
        }
        SYS_ASSERT( (uintptr_t)(joint_name_hashes) == (uintptr_t)(joint_name_hashes_address + joint_name_hashes_size) );

        if( flags & SKEL_CLO_INCLUDE_STRING_NAMES )
        {
            BufferChunker::Block block = chunker.AddBlock( joint_name_strings_size, 1 );
            out_skeleton->offsetJointNamesStrings = TYPE_POINTER_GET_OFFSET( &out_skeleton->offsetJointNamesStrings, block.begin );
            u8* current = block.begin;
            
            for( u32 i = 0; i < in_skeleton.jointNames.size(); ++i )
            {
                const std::string& name = in_skeleton.jointNames[i];
                const u32 name_size = (u32)name.length() + 1;
                memcpy( current, name.c_str(), name_size );

                current += name_size;
            }

            chunker.Checkpoint( current );
        }

        chunker.Check();

        return blob;
    }

    blob_t CompileClip( const Animation& in_animation, const Skeleton& in_skeleton, BXIAllocator* allocator )
    {
        const uint32_t num_joints = (uint32_t)in_skeleton.jointNames.size();
        const uint32_t num_frames = in_animation.numFrames;
        const uint32_t channel_data_size = num_joints * num_frames * sizeof( float4_t );
        const bool has_root_motion = Size( in_animation.root_motion ) == num_frames;
        const uint32_t root_motion_size = (has_root_motion) ? num_frames * sizeof( float4_t ) : 0;

        uint32_t memory_size = 0;
        memory_size += sizeof( ANIMClip );
        memory_size += channel_data_size; // rotation
        memory_size += channel_data_size; // translation
        memory_size += channel_data_size; // scale
        if( has_root_motion )
        {
            memory_size += root_motion_size;
        }

        blob_t blob = blob_t::allocate( allocator, memory_size, 16 );

        ANIMClip* clip = (ANIMClip*)blob.raw;
        memset( clip, 0, sizeof( ANIMClip ) );

        uint8_t* rotation_address = (uint8_t*)(clip + 1);
        uint8_t* translation_address = rotation_address + channel_data_size;
        uint8_t* scale_address = translation_address + channel_data_size;
        uint8_t* root_translation_address = (has_root_motion) ? scale_address + channel_data_size : nullptr;

        clip->duration = in_animation.endTime - in_animation.startTime;
        clip->sampleFrequency = in_animation.sampleFrequency;
        clip->numJoints = num_joints;
        clip->numFrames = num_frames;
        clip->offsetRotationData = TYPE_POINTER_GET_OFFSET( &clip->offsetRotationData, rotation_address );
        clip->offsetTranslationData = TYPE_POINTER_GET_OFFSET( &clip->offsetTranslationData, translation_address );
        clip->offsetScaleData = TYPE_POINTER_GET_OFFSET( &clip->offsetScaleData, scale_address );
        if( has_root_motion )
            clip->offsetRootTranslation = TYPE_POINTER_GET_OFFSET( &clip->offsetRootTranslation, root_translation_address );

        std::vector<float4_t> rotation_vector;
        std::vector<float4_t> translation_vector;
        std::vector<float4_t> scale_vector;
        std::vector<float4_t> root_translation_vector;

        const u32 channel_array_size = num_frames * num_joints;
        rotation_vector.reserve( channel_array_size );
        translation_vector.reserve( channel_array_size );
        scale_vector.reserve( channel_array_size );
        
        if( has_root_motion )
        {
            root_translation_vector.reserve( num_frames );
        }
        for( uint32_t i = 0; i < num_frames; ++i )
        {
            for( uint32_t j = 0; j < num_joints; ++j )
            {
                const AnimKeyframe& rotation_keyframe = in_animation.joints[j].rotation[i];
                const AnimKeyframe& translation_keyframe = in_animation.joints[j].translation[i];
                const AnimKeyframe& scale_keyframe = in_animation.joints[j].scale[i];

                rotation_vector.push_back( rotation_keyframe.data );
                translation_vector.push_back( translation_keyframe.data );
                scale_vector.push_back( scale_keyframe.data );
            }

            if( has_root_motion )
            {
                root_translation_vector.push_back( in_animation.root_motion.translation[i].data );
            }
        }

        SYS_ASSERT( channel_data_size == (rotation_vector.size() * sizeof( float4_t )) );
        SYS_ASSERT( channel_data_size == (translation_vector.size() * sizeof( float4_t )) );
        SYS_ASSERT( channel_data_size == (scale_vector.size() * sizeof( float4_t )) );
        SYS_ASSERT( root_motion_size  == (root_translation_vector.size() * sizeof( decltype( root_translation_vector )::value_type ) ) );

        memcpy( rotation_address, &rotation_vector[0], channel_data_size );
        memcpy( translation_address, &translation_vector[0], channel_data_size );
        memcpy( scale_address, &scale_vector[0], channel_data_size );
        if( has_root_motion )
        {
            memcpy( root_translation_address, &root_translation_vector[0], root_motion_size );
        }
        return blob;
    }

    ////
    ////
    bool ExportSkeletonToFile( const char* out_filename, const Skeleton& in_skeleton, BXIAllocator* allocator )
    {
        blob_t skel_blob = CompileSkeleton( in_skeleton, allocator );
        int write_result = WriteFile( out_filename, skel_blob.raw, skel_blob.size );

        skel_blob.destroy();
        return (write_result == -1) ? false : true;
    }
    ////
    ////
    bool ExportAnimationToFile( const char* out_filename, const Animation& in_animation, const Skeleton& in_skeleton, BXIAllocator* allocator )
    {
        blob_t clip_blob = CompileClip( in_animation, in_skeleton, allocator );

        int write_result = WriteFile( out_filename, clip_blob.raw, clip_blob.size );

        clip_blob.destroy();
        return (write_result == -1) ? false : true;
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////

    inline void initAssImp()
    {
        struct aiLogStream stream;
        stream = aiGetPredefinedLogStream( aiDefaultLogStream_STDOUT, NULL );
        aiAttachLogStream( &stream );
    }
    inline void deinitAssImp( const aiScene* scene )
    {
        aiReleaseImport( scene );
        aiDetachAllLogStreams();
    }

    bool ExportSkeletonToFile( const char* out_filename, const char* in_filename, BXIAllocator* allocator )
    {
        const unsigned import_flags = 0;
        initAssImp();
        const aiScene* aiscene = aiImportFile( in_filename, import_flags );
        if( !aiscene )
        {
            std::cout << "export skeleton failed: input file not found\n" << std::endl;
            return false;
        }

        Skeleton skel;
        _ExtractSkeleton( &skel, aiscene );

        const bool bres = ExportSkeletonToFile( out_filename, skel, allocator );
        deinitAssImp( aiscene );

        return bres;
    }

    bool ExportAnimationToFile( const char* out_filename, const char* in_filename, unsigned flags, BXIAllocator* allocator )
    {
        const unsigned import_flags = 0;
        initAssImp();
        const aiScene* aiscene = aiImportFile( in_filename, import_flags );
        if( !aiscene )
        {
            std::cout << "export skeleton failed: input file not found!\n" << std::endl;
            return false;
        }

        if( !aiscene->HasAnimations() )
        {
            std::cout << "scene does not contain animation!\n" << std::endl;
            return false;
        }
        Skeleton skel;
        Animation anim;

        _ExtractSkeleton( &skel, aiscene );
        _ExtractAnimation( &anim, skel, aiscene );

        const bool bres = ExportAnimationToFile( out_filename, anim, skel, allocator );
        deinitAssImp( aiscene );
        return bres;
    }

}}///
