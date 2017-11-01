#include "gfx_camera.h"
#include <foundation/common.h>
#include <foundation/math/mat44.h>

float GFXCameraParams::aspect() const
{
    return( ::fabsf( v_aperture ) < 0.0001f ) ? h_aperture : h_aperture / v_aperture;
}

float GFXCameraParams::fov() const
{
    return 2.f * atanf( ( 0.5f * h_aperture ) / ( focal_length * 0.03937f ) );
}

void ComputeViewport( int32_t xywh[4], float aspect, int dstWidth, int dstHeight, int srcWidth, int srcHeight )
{
    const int windowWidth = dstWidth;
    const int windowHeight = dstHeight;

    const float aspectRT = (float)srcWidth / (float)srcHeight;

    int imageWidth;
    int imageHeight;
    int offsetX = 0, offsetY = 0;


    if( aspect > aspectRT )
    {
        imageWidth = windowWidth;
        imageHeight = (int)( windowWidth / aspect + 0.0001f );
        offsetY = windowHeight - imageHeight;
        offsetY = offsetY / 2;
    }
    else
    {
        float aspect_window = (float)windowWidth / (float)windowHeight;
        if( aspect_window <= aspectRT )
        {
            imageWidth = windowWidth;
            imageHeight = (int)( windowWidth / aspectRT + 0.0001f );
            offsetY = windowHeight - imageHeight;
            offsetY = offsetY / 2;
        }
        else
        {
            imageWidth = (int)( windowHeight * aspectRT + 0.0001f );
            imageHeight = windowHeight;
            offsetX = windowWidth - imageWidth;
            offsetX = offsetX / 2;
        }
    }

    xywh[0] = offsetX;
    xywh[1] = offsetY;
    xywh[2] = imageWidth;
    xywh[3] = imageHeight;
}

mat44_t PerspectiveMatrix( float fov, float aspect, float znear, float zfar )
{
    const float f = tanf( ( (float)(PI_HALF)-( 0.5f * fov ) ) );
    const float range_rcp = ( 1.0f / ( znear - zfar ) );
    return mat44_t(
        vec4_t( ( f / aspect ), 0.0f, 0.0f, 0.0f ),
        vec4_t( 0.0f, f, 0.0f, 0.0f ),
        vec4_t( 0.0f, 0.0f, ( ( znear + zfar ) * range_rcp ), -1.0f ),
        vec4_t( 0.0f, 0.0f, ( ( ( znear * zfar ) * range_rcp ) * 2.0f ), 0.0f )
    );
}

void ComputeMatrices( GFXCameraMatrices* out, const GFXCameraParams& params, const mat44_t& world )
{
    out->world = world;
    out->view = inverse( world );
    out->proj = PerspectiveMatrix( params.fov(), params.aspect(), params.znear, params.zfar );
    out->view_proj = out->proj * out->view;
}
