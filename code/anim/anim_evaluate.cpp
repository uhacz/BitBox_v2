#include "anim.h"
#include <foundation\math\vmath.h>
#include <foundation\common.h>

static inline void _ComputeFrame( uint32_t* frameInt, float* frameFrac, float evalTime, float sampleFrequency )
{
    float frame = evalTime * sampleFrequency;
    if( frame < 0.f )
        frame = 0.f;

    const uint32_t i = (uint32_t)frame;

    frameInt[0] = i;
    frameFrac[0] = frame - (float)i;
}

struct FrameInfo
{
    const quat_t* rotations;
    const vec4_t* translations;
    const vec4_t* scales;

    const quat_t* rotations0;
    const vec4_t* translations0;
    const vec4_t* scales0;

    const quat_t* rotations1;
    const vec4_t* translations1;
    const vec4_t* scales1;

    FrameInfo( const ANIMClip* anim, uint32_t frameInteger )
    {
        const uint32_t numJoints    = anim->numJoints;
        const uint32_t currentFrame = ( frameInteger ) % anim->numFrames;
        const uint32_t nextFrame    = ( frameInteger + 1 ) % anim->numFrames;

        rotations    = TYPE_OFFSET_GET_POINTER( const quat_t, anim->offsetRotationData );
        translations = TYPE_OFFSET_GET_POINTER( const vec4_t, anim->offsetTranslationData );
        scales       = TYPE_OFFSET_GET_POINTER( const vec4_t, anim->offsetScaleData );

        rotations0    = rotations + currentFrame * numJoints;
        translations0 = translations + currentFrame * numJoints;
        scales0       = scales + currentFrame * numJoints;

        rotations1    = rotations + nextFrame * numJoints;
        translations1 = translations + nextFrame * numJoints;
        scales1       = scales + nextFrame * numJoints;
    }
};

void EvaluateClip( ANIMJoint* out_joints, const ANIMClip* anim, float evalTime, uint32_t beginJoint, uint32_t endJoint )
{
    uint32_t frameInteger = 0;
    float frameFraction = 0.f;
    _ComputeFrame( &frameInteger, &frameFraction, evalTime, anim->sampleFrequency );
	EvaluateClip( out_joints, anim, frameInteger, frameFraction, beginJoint, endJoint );
}

void EvaluateClip( ANIMJoint* out_joints, const ANIMClip* anim, uint32_t frameInteger, float frameFraction, uint32_t beginJoint, uint32_t endJoint )
{
    const FrameInfo frame( anim, frameInteger );

    uint32_t i = ( beginJoint == UINT32_MAX ) ? 0 : beginJoint;
    endJoint = ( endJoint == UINT32_MAX ) ? anim->numJoints : endJoint;
	
    const float alpha( frameFraction );
	do 
	{
		const quat_t& q0 = frame.rotations0[i];
		const quat_t& q1 = frame.rotations1[i];
        const quat_t q = slerp( alpha, q0, q1 );

		out_joints[i].rotation = q;

	} while ( ++i < endJoint );

	i = 0;
	do 
	{
		const vec4_t& t0 = frame.translations0[i];
		const vec4_t& t1 = frame.translations1[i];
        const vec4_t t = lerp( alpha, t0, t1 );

		out_joints[i].position = t;

    } while( ++i < endJoint );

	i = 0;
	do 
	{
		const vec4_t& s0 = frame.scales0[i];
		const vec4_t& s1 = frame.scales1[i];
        const vec4_t s = lerp( alpha, s0, s1 );

		out_joints[i].scale = s;

    } while( ++i < endJoint );
}

vec3_t EvaluateRootTranslation( const ANIMClip* anim, float eval_time )
{
    uint32_t frameInteger = 0;
    float frameFraction = 0.f;
    _ComputeFrame( &frameInteger, &frameFraction, eval_time, anim->sampleFrequency );
    return EvaluateRootTranslation( anim, frameInteger, frameFraction );
}

vec3_t EvaluateRootTranslation( const ANIMClip* anim, uint32_t frame_integer, float frame_fraction )
{
    const vec4_t* root_translations = TYPE_OFFSET_GET_POINTER( const vec4_t, anim->offsetRootTranslation );
    
    const u32 num_frames = anim->numFrames;
    const uint32_t currentFrame = (frame_integer) % num_frames;
    if( currentFrame == u32(anim->numFrames - 1) )
    {
        return root_translations[frame_integer].xyz();
    }
    
    const uint32_t nextFrame = (frame_integer + 1);

    const vec3_t a = root_translations[currentFrame].xyz();
    const vec3_t b = root_translations[nextFrame].xyz();
    return lerp( frame_fraction, a, b );
}

void EvaluateClipIndexed( ANIMJoint* out_joints, const ANIMClip* anim, float evalTime, const int16_t* indices, uint32_t numIndices )
{
    uint32_t frameInteger = 0;
    float frameFraction = 0.f;
    _ComputeFrame( &frameInteger, &frameFraction, evalTime, anim->sampleFrequency );
    EvaluateClipIndexed( out_joints, anim, frameInteger, frameFraction, indices, numIndices );

}

void EvaluateClipIndexed( ANIMJoint* out_joints, const ANIMClip* anim, uint32_t frameInteger, float frameFraction, const int16_t* indices, uint32_t numIndices )
{
    const FrameInfo frame( anim, frameInteger );
    const float alpha( frameFraction );

    for( uint32_t ii = 0; ii < numIndices; ++ii )
    {
        const int16_t i = indices[ii];
        
        const quat_t& q0 = frame.rotations0[i];
        const quat_t& q1 = frame.rotations1[i];
        const quat_t q = slerp( alpha, q0, q1 );

        out_joints[ii].rotation = q;
    }

    for( uint32_t ii = 0; ii < numIndices; ++ii )
    {
        const int16_t i = indices[ii];

        const vec4_t& t0 = frame.translations0[i];
        const vec4_t& t1 = frame.translations1[i];
        const vec4_t t = lerp( alpha, t0, t1 );

        out_joints[ii].position = t;
    }

    for( uint32_t ii = 0; ii < numIndices; ++ii )
    {
        const int16_t i = indices[ii];

        const vec4_t& s0 = frame.scales0[i];
        const vec4_t& s1 = frame.scales1[i];
        const vec4_t s = lerp( alpha, s0, s1 );

        out_joints[ii].scale = s;
    }
}
