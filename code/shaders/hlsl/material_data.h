#ifndef MATERIAL_DATA_H
#define MATERIAL_DATA_H

struct Material
{
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
#endif

#endif