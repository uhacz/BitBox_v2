#pragma once

#include <foundation/math/vmath_type.h>
#include <foundation/type.h>

namespace RDIXDebug
{
    enum EFlag : uint32_t
    {
        DEPTH = BIT_OFFSET( 0 ),
        SOLID = BIT_OFFSET( 1 ),
    };
}//

struct RDIXDebugParams
{
    uint32_t color = 0xFFFFFFFF;
    uint32_t flag = RDIXDebug::DEPTH;
    float scale = 1.f;

    RDIXDebugParams( uint32_t c = 0xFFFFFFFF ) : color( c ) {}
    RDIXDebugParams& NoDepth()  { flag &= ~RDIXDebug::DEPTH; return *this; }
    RDIXDebugParams& Solid()    { flag |=  RDIXDebug::SOLID; return *this; }
    RDIXDebugParams& Scale( float v ) { scale = v; }
};

struct BXIAllocator;
struct RDIDevice;
struct RDICommandQueue;
struct RSM;

namespace RDIXDebug
{
    void StartUp( RDIDevice* dev, RSM* rsm, BXIAllocator* allocator );
    void ShutDown( RDIDevice* dev );
    
    void AddAABB( const vec3_t& center, const vec3_t& extents, const RDIXDebugParams& params = {} );
    void AddSphere( const vec3_t& pos, float radius, const RDIXDebugParams& params = {} );
    void AddLine( const vec3_t& start, const vec3_t& end, const RDIXDebugParams& params = {} );
    void AddAxes( const mat44_t& pose, const RDIXDebugParams& params = {} );

    void Flush( RDICommandQueue& cmdq, const mat44_t& viewproj );
}//
