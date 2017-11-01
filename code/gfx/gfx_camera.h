#pragma once

#include <stdint.h>
#include <foundation/math/vmath_type.h>

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
    mat44_t view_proj;

    vec3_t eye() const { return world.translation(); }
    vec3_t dir() const { return -world.c2.xyz(); }
};

void    ComputeViewport( int32_t xywh[4], float aspect, int dstWidth, int dstHeight, int srcWidth, int srcHeight );
mat44_t PerspectiveMatrix( float fov, float aspect, float znear, float zfar );
void    ComputeMatrices( GFXCameraMatrices* out, const GFXCameraParams& params, const mat44_t& world );
