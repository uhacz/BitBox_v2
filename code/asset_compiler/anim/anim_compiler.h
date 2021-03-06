#pragma once

#include <foundation/type_compound.h>
#include <foundation/blob.h>
#include <string>
#include <vector>

struct BXIAllocator;
struct ANIMSkel;
struct ANIMClip;

namespace tool { namespace anim {

        struct Joint
        {
            float4_t rotation;
            float4_t translation;
            float4_t scale;
        };

        struct Skeleton
        {
            std::vector<uint16_t> parentIndices;
            std::vector<Joint> basePose;
            std::vector<std::string> jointNames;
        };

        struct AnimKeyframe
        {
            float time;
            float4_t data;
        };

        struct JointAnimation
        {
            std::string name;
            float weight;

            std::vector<AnimKeyframe> rotation;
            std::vector<AnimKeyframe> translation;
            std::vector<AnimKeyframe> scale;
        };

        struct Animation
        {
            float startTime;
            float endTime;
            float sampleFrequency;
            uint32_t numFrames;

            std::vector< JointAnimation > joints;
            JointAnimation root_motion;
        };
        
        struct ImportParams
        {
            float scale = 0.01f;
            int16_t root_motion_joint = -1;
            bool extract_root_motion = true;
            bool remove_root_motion = false;
            bool strip_namespace_name = true;
        };

        bool Import( Skeleton* skeleton, Animation* animation, const void* data, uint32_t data_size, const ImportParams& params = {} );
        
        enum ComplileOptions
        {
            SKEL_CLO_INCLUDE_STRING_NAMES = 1,
        };
        // produces data with ANIMSkel header
        blob_t CompileSkeleton( const Skeleton& in_skeleton, BXIAllocator* allocator, u32 flags = 0 );
        // produces data with ANIMClip header
        blob_t CompileClip( const Animation& in_animation, const Skeleton& in_skeleton, BXIAllocator* allocator );

        bool ExportSkeletonToFile( const char* out_filename, const Skeleton& in_skeleton, BXIAllocator* allocator );
        bool ExportAnimationToFile( const char* out_filename, const Animation& in_animation, const Skeleton& in_skeleton, BXIAllocator* allocator );

        bool ExportSkeletonToFile( const char* out_filename, const char* in_filename, BXIAllocator* allocator );
        bool ExportAnimationToFile( const char* out_filename, const char* in_filename, unsigned flags, BXIAllocator* allocator );
}}//