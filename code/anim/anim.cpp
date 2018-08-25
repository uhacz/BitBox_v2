#include "anim.h"
#include <foundation/debug.h>
#include <memory/memory.h>

ANIMContext* contextInit( const ANIMSkel& skel, BXIAllocator* allocator )
{
	const uint32_t poseMemorySize = skel.numJoints * sizeof( ANIMJoint );
	
	uint32_t memSize = 0;
	memSize += sizeof( ANIMContext );
	memSize += poseMemorySize * ANIMContext::ePOSE_CACHE_SIZE;
	memSize += poseMemorySize * ANIMContext::ePOSE_STACK_SIZE;
	memSize += sizeof( Cmd ) * ANIMContext::eCMD_ARRAY_SIZE;

	uint8_t* memory = (uint8_t*)BX_MALLOC( allocator, memSize, 16 );
	memset( memory, 0, memSize );

	ANIMContext* ctx = (ANIMContext*)memory;

	uint8_t* current_pointer = memory + sizeof(ANIMContext);
	for( uint32_t i = 0; i < ANIMContext::ePOSE_CACHE_SIZE; ++i )
	{
		ctx->poseCache[i] = (ANIMJoint*)current_pointer;
		current_pointer += poseMemorySize;
	}

	for( uint32_t i = 0; i < ANIMContext::ePOSE_STACK_SIZE; ++i )
	{
		ctx->poseStack[i] = (ANIMJoint*)current_pointer;
		current_pointer += poseMemorySize;
	}

	ctx->cmdArray = (Cmd*)current_pointer;
	current_pointer += sizeof(Cmd) * ANIMContext::eCMD_ARRAY_SIZE;
	SYS_ASSERT( (uintptr_t)current_pointer == (uintptr_t)( memory + memSize ) );
    ctx->numJoints = skel.numJoints;

    for( uint32_t i = 0; i < ANIMContext::ePOSE_STACK_SIZE; ++i )
    {
        ANIMJoint* joints = ctx->poseStack[i];
        for( uint32_t j = 0; j < skel.numJoints; ++j )
        {
            joints[j] = ANIMJoint::identity();
        }
    }

    ctx->allocator = allocator;

	return ctx;
}

void contextDeinit( ANIMContext** ctx )
{
    if( ctx[0] == nullptr )
        return;

    BXIAllocator* allocator = ctx[0]->allocator;
	BX_FREE0( allocator, ctx[0] );
}

#include <resource_manager/resource_manager.h>

namespace anim_ext {

    //static uptr _LoadResource( bx::ResourceManager* resourceManager, const char* relativePath )
    //{
    //    bx::ResourceID resourceId = bx::ResourceManager::createResourceID( relativePath );
    //    uptr resourceData = resourceManager->lookup( resourceId );
    //    if ( resourceData )
    //    {
    //        resourceManager->referenceAdd( resourceId );
    //    }
    //    else
    //    {
    //        bxFS::File file = resourceManager->readFileSync( relativePath );
    //        if ( file.ok() )
    //        {
    //            resourceData = uptr( file.bin );
    //            resourceManager->insert( resourceId, resourceData );
    //        }
    //    }
    //    return resourceData;
    //}
    //static void _UnloadResource( bx::ResourceManager* resourceManager, uptr resourceData )
    //{
    //    bx::ResourceID resourceId = resourceManager->find( resourceData );
    //    if ( !resourceId )
    //    {
    //        bxLogError( "resource not found!" );
    //        return;
    //    }

    //    int refLeft = resourceManager->referenceRemove( resourceId );
    //    if ( refLeft == 0 )
    //    {
    //        void* ptr = (void*)resourceData;
    //        BX_FREE( bxDefaultAllocator(), ptr );
    //    }
    //}

    //ANIMSkel* loadANIMSkelFromFile( bx::ResourceManager* resourceManager, const char* relativePath )
    //{
    //    bx::ResourceLoadResult load_result = resourceManager->loadResource( relativePath, bx::EResourceFileType::BINARY );
    //    return (ANIMSkel*)load_result.ptr;
    //    //uptr resourceData = _LoadResource( resourceManager, relativePath );
    //    //return (ANIMSkel*)resourceData;
    //}

    //Clip* loadAnimFromFile( bx::ResourceManager* resourceManager, const char* relativePath )
    //{
    //    bx::ResourceLoadResult load_result = resourceManager->loadResource( relativePath, bx::EResourceFileType::BINARY );
    //    return (Clip*)load_result.ptr;

    //    //uptr resourceData = _LoadResource( resourceManager, relativePath );
    //    //return (Clip*)resourceData;
    //}

    //void unloadANIMSkelFromFile( bx::ResourceManager* resourceManager, ANIMSkel** skel )
    //{
    //    if ( !skel[0] )
    //        return;

    //    resourceManager->unloadResource( (bx::ResourcePtr*)skel );
    //    //_UnloadResource( resourceManager, uptr( skel[0] ) );
    //    //skel[0] = 0;
    //}

    //void unloadAnimFromFile( bx::ResourceManager* resourceManager, Clip** clip )
    //{
    //    if( !clip[0] )
    //        return;
    //
    //    resourceManager->unloadResource( ( bx::ResourcePtr* )clip );
    //    //_UnloadResource( resourceManager, uptr( clip[0] ) );
    //    //clip[0] = 0;
    //}

    void LocalJointsToWorldJoints( ANIMJoint* outJoints, const ANIMJoint* inJoints, const ANIMSkel* skel, const ANIMJoint& rootJoint )
    {
        const uint16_t* parentIndices = TYPE_OFFSET_GET_POINTER( const uint16_t, skel->offsetParentIndices );
        LocalJointsToWorldJoints( outJoints, inJoints, parentIndices, skel->numJoints, rootJoint );
    }

    void LocalJointsToWorldMatrices( mat44_t* outMatrices, const ANIMJoint* inJoints, const ANIMSkel* skel, const ANIMJoint& rootJoint )
    {
        const uint16_t* parentIndices = TYPE_OFFSET_GET_POINTER( const uint16_t, skel->offsetParentIndices );
        LocalJointsToWorldMatrices4x4( outMatrices, inJoints, parentIndices, skel->numJoints, rootJoint );
    }

    void ProcessBlendTree( ANIMContext* ctx, const uint16_t root_index, const ANIMBlendBranch* blend_branches, uint32_t num_branches, const ANIMBlendLeaf* blend_leaves, uint32_t num_leaves )
    {
        EvaluateBlendTree( ctx, root_index, blend_branches, num_branches, blend_leaves, num_leaves );
        EvaluateCommandList( ctx );
    }
}