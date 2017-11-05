#pragma once

#include <foundation/type.h>
#include <foundation/math/vmath.h>

struct GFXCameraID   { uint32_t i; };
struct GFXLightID    { uint32_t i; };
struct GFXMeshID     { uint32_t i; };
struct GFXMaterialID { uint32_t i; };


struct GFXLightParams
{
    vec3_t pos;
    vec3_t dir;
    uint32_t color;
};


struct GFXMaterial
{
    struct RDIXPipeline* piepeline;
    struct RDIXResourceBinding* resources;
};

struct GFXMeshParams
{
    struct RDIXRenderSource* rsource;
    GFXMaterialID material_id;
    uint32_t render_mask;
};

class GFX
{
public:
	static GFXCameraID	 CreateCamera( const char* name );
	static GFXLightID	 CreateLight( const char* name );
	static GFXMeshID	 CreateMesh( const char* name );
	static GFXMaterialID CreateMaterial( const char* name );

	static void Destroy( GFXCameraID* id );
	static void Destroy( GFXLightID* id );
	static void Destroy( GFXMeshID* id );
	static void Destroy( GFXMaterialID* id );
	
    bool Add( GFXCameraID id );
	bool Add( GFXLightID id );
	bool Add( GFXMeshID id );

	void Remove( GFXCameraID id );
	void Remove( GFXLightID id );
	void Remove( GFXMeshID id );

public:

public:

};