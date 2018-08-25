#pragma once

#include <foundation/type_compound.h>
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
        };

        uint32_t SkelTag();
        uint32_t ClipTag();

        ANIMSkel* CreateSkeleton( const Skeleton& in_skeleton, BXIAllocator* allocator );
        ANIMClip* CreateClip( const Animation& in_animation, const Skeleton& in_skeleton, BXIAllocator* allocator );

        bool ExportSkeleton( const char* out_filename, const Skeleton& in_skeleton, BXIAllocator* allocator );
        bool ExportAnimation( const char* out_filename, const Animation& in_animation, const Skeleton& in_skeleton, BXIAllocator* allocator );

        bool ExportSkeleton( const char* out_filename, const char* in_filename, BXIAllocator* allocator );
        bool ExportAnimation( const char* out_filename, const char* in_filename, unsigned flags, BXIAllocator* allocator );
}}//