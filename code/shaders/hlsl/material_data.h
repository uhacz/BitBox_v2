#ifndef MATERIAL_DATA_H
#define MATERIAL_DATA_H

struct 
#ifndef SHADER_IMPLEMENTATION
    GFX_EXPORT
#endif 
    Material
{
#ifndef SHADER_IMPLEMENTATION
    RTTI_DECLARE_TYPE( Material );
#endif
	float3 diffuse_albedo;
	float roughness;
	float3 specular_albedo;
	float metal;
};

#ifdef SHADER_IMPLEMENTATION
shared cbuffer MaterialData
{
	Material _material;
};

Texture2D tex_material_base_color;
Texture2D tex_material_normal;
Texture2D tex_material_roughness;
Texture2D tex_material_metalness;

#endif

#endif