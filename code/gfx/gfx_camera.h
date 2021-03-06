#pragma once

#include "dll_interface.h"
#include <foundation/type.h>
#include <foundation/math/vmath.h>

struct GFXCameraParams
{
    float h_aperture = 1.8f;
    float v_aperture = 1.f;
    float focal_length = 50.f;
    float znear = 0.25f;
    float zfar = 300.f;
    float ortho_width = 10.f;
    float ortho_height = 10.f;

    float aspect() const;
    float fov() const;
};

struct GFXCameraMatrices
{
    mat44_t world;
    mat44_t view;
    mat44_t proj;
    mat44_t proj_api;

    vec3_t eye() const { return world.translation(); }
    vec3_t dir() const { return -world.c2.xyz(); }
};

void    ComputeViewport( int32_t xywh[4], float aspect, int dstWidth, int dstHeight, int srcWidth, int srcHeight );
mat44_t PerspectiveMatrix( float fov, float aspect, float znear, float zfar );
void    ComputeMatrices( GFXCameraMatrices* out, const GFXCameraParams& params, const mat44_t& world );


// --- movement utils
struct GFXCameraInputContext
{
	float _left_x = 0.f;
	float _left_y = 0.f;
	float _right_x = 0.f;
	float _right_y = 0.f;
	float _up_down = 0.f;

    void ReadInput( int mouseL, int mouseM, int mouseR, int mouseDx, int mouseDy, float mouseSensitivityInPix );

	void UpdateInput( int mouseL, int mouseM, int mouseR, int mouseDx, int mouseDy, float mouseSensitivityInPix, float dt );
	void UpdateInput( float analogX, float analogY, float dt );
	bool AnyMovement() const;
};

mat44_t CameraMovementFPP( const GFXCameraInputContext& input, const mat44_t& world, float sensitivity );
void CameraInputFilter( GFXCameraInputContext* output, const GFXCameraInputContext& raw, const GFXCameraInputContext& prev, f32 rc, f32 dt );
void CameraInputLerp( GFXCameraInputContext* output, f32 t, const GFXCameraInputContext& a, const GFXCameraInputContext& b );