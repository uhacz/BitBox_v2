#include "mesh_compiler.h"

#include <iostream>

#include "memory/memory.h"
#include <foundation/buffer.h>
#include <foundation/hashed_string.h>
#include "rdix/rdix_type.h"

#include <3rd_party/assimp/cimport.h>
#include <3rd_party/assimp/scene.h>
#include "assimp/postprocess.h"



namespace tool { namespace mesh {

    inline aiVector3D to_aiVector3D( const vec3_t& v )
    {
        return aiVector3D( v.x, v.y, v.z );
    }

    inline VertexData ToVertexData( const aiVector3D& v )
    {
        VertexData vd;
        memset( &vd, 0x00, sizeof( VertexData ) );
        vd.f32[0] = v.x;
        vd.f32[1] = v.y;
        vd.f32[2] = v.z;
        return vd;
    }

    void ToVertexData( VertexDataArray* output, const aiVector3D* input, uint32_t inputCount )
    {
        output->reserve( inputCount );
        for( uint32_t i = 0; i < inputCount; ++i )
        {
            output->push_back( ToVertexData( input[i] ) );
        }
    }
    void ToVertexDataWithTransformAsPoints( VertexDataArray* output, const aiVector3D* input, uint32_t inputCount, const aiMatrix4x4& xform )
    {
        output->reserve( inputCount );
        for( uint32_t i = 0; i < inputCount; ++i )
        {
            const aiVector3D original = input[i];
            const aiVector3D transformed = xform * original;
            output->push_back( ToVertexData( transformed ) );
        }
    }

    mat44_t ToMat44( const aiMatrix4x4& m )
    {
        aiMatrix4x4 mt = m;
        mt.Transpose();

        const ai_real* c0 = mt[0];
        const ai_real* c1 = mt[1];
        const ai_real* c2 = mt[2];
        const ai_real* c3 = mt[3];

        return mat44_t(
            vec4_t( c0[0], c0[1], c0[2], c0[3] ),
            vec4_t( c1[0], c1[1], c1[2], c1[3] ),
            vec4_t( c2[0], c2[1], c2[2], c2[3] ),
            vec4_t( c3[0], c3[1], c3[2], c3[3] )
        );

    }

    bool _Import( Streams* streams, const aiMesh* inputMesh, const ImportOptions& options )
    {
        static constexpr uint32_t MAX_BLEND_WEIGHTS = 4;
        static constexpr uint32_t MAX_BONES = UINT8_MAX;

        const uint32_t num_vertices = inputMesh->mNumVertices;
        streams->num_vertices = num_vertices;

        aiMatrix4x4 scale_matrix;
        aiMatrix4x4::Scaling( to_aiVector3D( options.scale ), scale_matrix );

        streams->name = inputMesh->mName.C_Str();

        if( inputMesh->HasPositions() )
        {
            streams->slots[RDIEVertexSlot::POSITION] = RDIVertexBufferDesc::POS();
            ToVertexDataWithTransformAsPoints( &streams->data[RDIEVertexSlot::POSITION], inputMesh->mVertices, num_vertices, scale_matrix );
        }
        if( inputMesh->HasNormals() )
        {
            streams->slots[RDIEVertexSlot::NORMAL] = RDIVertexBufferDesc::NRM();
            ToVertexData( &streams->data[RDIEVertexSlot::NORMAL], inputMesh->mNormals, num_vertices );
        }
        if( inputMesh->HasTangentsAndBitangents() )
        {
            streams->slots[RDIEVertexSlot::TANGENT] = RDIVertexBufferDesc::TAN();
            ToVertexData( &streams->data[RDIEVertexSlot::TANGENT], inputMesh->mTangents, num_vertices );

            streams->slots[RDIEVertexSlot::BINORMAL] = RDIVertexBufferDesc::BIN();
            ToVertexData( &streams->data[RDIEVertexSlot::BINORMAL], inputMesh->mBitangents, num_vertices );
        }

        for( uint32_t iuv = RDIEVertexSlot::TEXCOORD0; iuv <= RDIEVertexSlot::TEXCOORD5; ++iuv )
        {
            const uint32_t inputIndex = iuv - RDIEVertexSlot::TEXCOORD0;
            if( inputMesh->HasTextureCoords(inputIndex) )
            {
                streams->slots[iuv] = RDIVertexBufferDesc::UV( inputIndex );
                ToVertexData( &streams->data[iuv], inputMesh->mTextureCoords[inputIndex], num_vertices );
            }
        }
        
        if( inputMesh->HasFaces() )
        {
            const bool use_16bit_indices = num_vertices <= UINT16_MAX;
            const uint32_t num_faces = inputMesh->mNumFaces;
            const uint32_t num_indices = num_faces * 3;
            
            streams->num_indices = num_indices;
            streams->flag_use_16bit_indices = use_16bit_indices;

            if( use_16bit_indices )
                streams->indices16.reserve( num_indices );
            else
                streams->indices32.reserve( num_indices );

            for( uint32_t iface = 0; iface < num_faces; ++iface )
            {
                const aiFace& face = inputMesh->mFaces[iface];
                if( face.mNumIndices != 3 )
                    continue;

                for( uint32_t i = 0; i < face.mNumIndices; ++i )
                {
                    const uint32_t vindex = face.mIndices[i];
                    if( use_16bit_indices )
                    {
                        SYS_ASSERT( vindex <= UINT16_MAX );
                        streams->indices16.push_back( (uint16_t)vindex );
                    }
                    else
                    {
                        streams->indices32.push_back( vindex );
                    }
                }
            }
        }

        if( inputMesh->HasBones() )
        {
            const uint32_t num_bones = inputMesh->mNumBones;
            streams->num_bones = num_bones;

            streams->slots[RDIEVertexSlot::POSITION];
            streams->slots[RDIEVertexSlot::NORMAL];

            if( num_bones <= MAX_BONES )
            {
                streams->bones.resize( num_bones );
                streams->bones_names.resize( num_bones );

                streams->slots[RDIEVertexSlot::BLENDWEIGHT] = RDIVertexBufferDesc::BW();
                streams->slots[RDIEVertexSlot::BLENDINDICES] = RDIVertexBufferDesc::BI();
                
                VertexDataArray& blendw = streams->data[RDIEVertexSlot::BLENDWEIGHT];
                VertexDataArray& blendi = streams->data[RDIEVertexSlot::BLENDINDICES];

                blendw.resize( num_vertices );
                blendi.resize( num_vertices );

                for( uint32_t ibone = 0; ibone < num_bones; ++ibone )
                {
                    const aiBone* bone = inputMesh->mBones[ibone];

                    aiVector3D translation;
                    aiQuaternion rotation;
                    aiVector3D scale;

                    bone->mOffsetMatrix.Decompose( scale, rotation, translation );

                    translation = translation.SymMul( to_aiVector3D( options.scale ) );

                    aiMatrix4x4 tmp( scale, rotation, translation );
                    aiMatrix4x4 bone_offset_matrix = tmp;

                    streams->bones[ibone] = ToMat44( bone_offset_matrix );
                    streams->bones_names[ibone] = bone->mName.C_Str();

                    for( uint32_t iweight = 0; iweight < bone->mNumWeights; ++iweight )
                    {
                        const aiVertexWeight& vw = bone->mWeights[iweight];

                        VertexData& bone_weights = blendw[vw.mVertexId];
                        VertexData& bone_indices = blendi[vw.mVertexId];

                        uint32_t i = 0;
                        while( bone_weights.u32[i] && i < MAX_BLEND_WEIGHTS )
                        {
                            ++i;
                        }

                        SYS_ASSERT( i < MAX_BLEND_WEIGHTS );

                        bone_weights.f32[i] = vw.mWeight;
                        bone_indices.u8[i] = (uint8_t)ibone;
                    }
                }
            }
        }

        return true;
    };

    
    StreamsArray Import( const void* data, uint32_t data_size, const ImportOptions& options )
    {
        const uint32_t flags = 
            aiProcess_GenSmoothNormals | 
            aiProcess_CalcTangentSpace | 
            aiProcess_LimitBoneWeights | 
            aiProcess_JoinIdenticalVertices |
            aiProcess_Triangulate |
            aiProcess_SortByPType |
            aiProcess_ValidateDataStructure |
            aiProcess_ImproveCacheLocality;

        const aiScene* scene = aiImportFileFromMemory( (const char*)data, data_size, flags, nullptr );
        const char* error_str = aiGetErrorString();

        std::vector<Streams> output;

        if( scene )
        {
            if( scene->HasMeshes() )
            {
                output.reserve( scene->mNumMeshes );

                for( uint32_t i = 0; i < scene->mNumMeshes; ++i )
                {
                    const aiMesh* mesh = scene->mMeshes[i];

                    Streams streams;
                    if( _Import( &streams, mesh, options ) )
                    {
                        output.emplace_back( std::move( streams ) );
                    }
                }
                
            }

            aiReleaseImport( scene );
        }


        return output;
    }

    CompileOptions::CompileOptions( uint32_t default_slots /*= UINT32_MAX */ )
    {
        slot_mask = default_slots;
    }

    CompileOptions& CompileOptions::AddSlot( RDIEVertexSlot::Enum slot )
    {
        slot_mask |= 1 << slot;
        return *this;
    }

    CompileOptions& CompileOptions::RemSlot( RDIEVertexSlot::Enum slot )
    {
        slot_mask &= ~(1 << slot);
        return *this;
    }

    CompileOptions& CompileOptions::EnableSlot( RDIEVertexSlot::Enum slot, bool value )
    {
        if( value )
            AddSlot( slot );
        else
            RemSlot( slot );
        
        return *this;
    }

    blob_t Compile( const Streams& streams, const CompileOptions& opt, BXIAllocator* allocator )
    {
        constexpr uint32_t MAX_STREAMS = RDIEVertexSlot::COUNT;

        uint32_t memory_size_streams[MAX_STREAMS] = {};
        RDIVertexBufferDesc streams_descs[MAX_STREAMS] = {};
        uint32_t num_streams = 0;

        for( uint32_t istream = 0; istream < MAX_STREAMS; ++istream )
        {
            if( streams.data[istream].empty() || !opt.HasSlot( istream ) )
                continue;

            const uint32_t index = num_streams++;

            const RDIVertexBufferDesc desc = streams.slots[istream];
            streams_descs[index] = desc;
            memory_size_streams[index] = desc.ByteWidth() * streams.num_vertices;
        }

        const uint32_t index_size_in_bytes = (streams.flag_use_16bit_indices) ? sizeof( uint16_t ) : sizeof( uint32_t );
        const uint32_t memory_size_for_indices = index_size_in_bytes * streams.num_indices;

        const uint32_t bone_size_in_bytes = sizeof( BoneDataArray::value_type );
        const uint32_t memory_size_for_bones = bone_size_in_bytes * streams.num_bones;
        const uint32_t memory_size_for_bones_names = sizeof( hashed_string_t ) * streams.num_bones;


        uint32_t memory_size_for_all_streams = 0;

        for( uint32_t i = 0; i < MAX_STREAMS; ++i )
            memory_size_for_all_streams += memory_size_streams[i];

        memory_size_for_all_streams = TYPE_ALIGN( memory_size_for_all_streams, 16 );
        memory_size_for_all_streams += memory_size_for_bones;
        memory_size_for_all_streams += memory_size_for_bones_names;
        memory_size_for_all_streams += memory_size_for_indices;

        

        uint32_t total_memory_size = 0;
        total_memory_size += sizeof( RDIXMeshFile );
        total_memory_size += memory_size_for_all_streams;


        blob_t blob = blob_t::allocate( allocator, total_memory_size, sizeof( void* ) );

        // fill header
        RDIXMeshFile* header = new( blob.raw )(RDIXMeshFile);
        header->num_streams = num_streams;
        header->num_vertices = streams.num_vertices;
        header->num_indices = streams.num_indices;
        header->num_bones = streams.num_bones;
        header->num_draw_ranges = 0;
        header->flag_use_16bit_indices = streams.flag_use_16bit_indices;

        // fill data
        uint8_t* data_memory = (uint8_t*)(header + 1);
        BufferChunker chunker( data_memory, memory_size_for_all_streams );

        // streams
        for( uint32_t i = 0; i < num_streams; ++i )
        {
            const RDIVertexBufferDesc desc = streams_descs[i];

            const uint32_t vertex_stride = desc.ByteWidth();
            
            BufferChunker::Block data_block = chunker.AddBlock( memory_size_streams[i], 4 );
            uint8_t* data_iterator = data_block.begin;

            const VertexDataArray& src_vertices = streams.data[desc.slot];
            for( uint32_t ivertex = 0; ivertex < streams.num_vertices; ++ivertex )
            {
                const uint8_t* src = &src_vertices[ivertex].u8[0];
                memcpy( data_iterator, src, vertex_stride );
                data_iterator += vertex_stride;
            }
            chunker.Checkpoint( data_iterator );
        
            header->descs[i] = streams_descs[i];
            header->offset_streams[i] = TYPE_POINTER_GET_OFFSET( &header->offset_streams[i], data_block.begin );
        }

        {
            uint8_t* tmp = data_memory;
            for( uint32_t i = 0; i < num_streams; ++i )
                tmp += memory_size_streams[i];

            chunker.Checkpoint( tmp );
        }

        

        // bones
        {
            BufferChunker::Block data_block = chunker.AddBlock( memory_size_for_bones, 16 );

            mat44_t* dst_bones = (mat44_t*)data_block.begin;
            for( uint32_t i = 0; i < streams.num_bones; ++i )
            {
                dst_bones[0] = streams.bones[i];
                ++dst_bones;
            }
            chunker.Checkpoint( dst_bones );

            header->offset_bones = TYPE_POINTER_GET_OFFSET( &header->offset_bones, data_block.begin );
        }
        // bones names
        {
            BufferChunker::Block data_block = chunker.AddBlock( memory_size_for_bones_names );

            hashed_string_t* dst_bones_names = (hashed_string_t*)data_block.begin;
            for( uint32_t i = 0; i < streams.num_bones; ++i )
            {
                dst_bones_names[0] = hashed_string( streams.bones_names[i].c_str() );
                ++dst_bones_names;
            }
            chunker.Checkpoint( dst_bones_names );

            header->offset_bones_names = TYPE_POINTER_GET_OFFSET( &header->offset_bones_names, data_block.begin );
        }

        // indices
        {
            BufferChunker::Block data_block = chunker.AddBlock( memory_size_for_indices, 2 );

            if( streams.flag_use_16bit_indices )
            {
                uint16_t* dst_indices16 = (uint16_t*)data_block.begin;
                for( uint32_t i = 0; i < streams.num_indices; ++i )
                {
                    dst_indices16[0] = streams.indices16[i];
                    ++dst_indices16;
                }
                chunker.Checkpoint( dst_indices16 );
            }
            else
            {
                uint32_t* dst_indices32 = (uint32_t*)data_block.begin;
                for( uint32_t i = 0; i < streams.num_indices; ++i )
                {
                    dst_indices32[0] = streams.indices32[i];
                    ++dst_indices32;
                }
                chunker.Checkpoint( dst_indices32 );
            }
            header->offset_indices = TYPE_POINTER_GET_OFFSET( &header->offset_indices, data_block.begin );
        }

        chunker.Check();

        return blob;
    }



}}