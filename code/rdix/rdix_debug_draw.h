#pragma once

#include <foundation/math/vmath_type.h>
#include <foundation/type.h>

namespace RDIXDebugDraw
{
    enum EFlag : uint32_t
    {
        DEPTH = BIT_OFFSET( 1 ),
        SOLID = BIT_OFFSET( 2 ),
    };
}//

struct RDIXDDrawParams
{
    uint32_t color = 0xFFFFFFFF;
    uint32_t flag = RDIXDebugDraw::DEPTH;
    float scale = 1.f;

    RDIXDDrawParams( uint32_t c = 0xFFFFFFFF ) : color( c ) {}
    RDIXDDrawParams& NoDepth() { flag &= ~RDIXDebugDraw::DEPTH; return *this; }
    RDIXDDrawParams& Solid() { flag |= RDIXDebugDraw::SOLID; return *this; }
    RDIXDDrawParams& Scale( float v ) { scale = v; }
};

struct RDIDevice;
struct RDICommandQueue;
struct RSM;

namespace RDIXDebugDraw
{
    void StartUp( RDIDevice* dev, RSM* rsm );
    void ShutDown( RDIDevice* dev );
    
    void AddAABB( const vec3_t& center, const vec3_t& extents, const RDIXDDrawParams& params = {} );
    void AddSphere( const vec3_t& pos, float radius, const RDIXDDrawParams& params = {} );
    void AddLine( const vec3_t& start, const vec3_t& end, const RDIXDDrawParams& params = {} );
    void AddArrow( const vec3_t& start, const vec3_t& end, const RDIXDDrawParams& params = {} );
    void AddAxes( const mat44_t& pose, const RDIXDDrawParams& params = {} );

    void Flush( RDICommandQueue& cmdq, const mat44_t& viewproj );
}//
