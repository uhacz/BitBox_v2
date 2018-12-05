#pragma once

#include <vector>
#include <array>
#include <foundation/blob.h>
#include <rdi_backend\rdi_backend_type.h>
#include<foundation\math\vmath_type.h>

namespace tool { namespace mesh {

    union VertexData
    {
        uint32_t u32[4] = {};
        uint16_t u16[4];
        uint8_t  u8[4];
        float    f32[4];
    };

    using VertexDataArray = std::vector<VertexData>;
    using BoneDataArray = std::vector<mat44_t>;

    struct Streams
    {
        static constexpr uint32_t COUNT = RDIEVertexSlot::COUNT;

        std::array<RDIVertexBufferDesc, COUNT> slots;
        std::array<VertexDataArray, COUNT > data;
        std::vector<uint32_t> indices32;
        std::vector<uint16_t> indices16;

        std::vector<std::string> bones_names;
        BoneDataArray bones;

        uint32_t num_vertices = 0;
        uint32_t num_bones = 0;
        uint32_t num_indices = 0;
        uint32_t flag_use_16bit_indices = false;
    };

    bool Import( Streams* sterams, const void* data, uint32_t data_size );
    blob_t Compile( const Streams& streams, BXIAllocator* allocator );
}}