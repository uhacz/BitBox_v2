#pragma once

#include <foundation/type.h>
#include <string>
#include <vector>

namespace bx{ namespace rdi{

struct ShaderVariableDesc
{
    std::string name;
    uint32_t hashed_name = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t type = 0;
};

struct ShaderCBufferDesc
{
    std::string name;
    uint32_t hashed_name = 0;
    uint32_t size = 0;
    uint8_t slot = 0;
    uint8_t stage_mask = 0;
    uint8_t __padd[2] = {};
    std::vector<ShaderVariableDesc> variables;
};

struct ShaderTextureDesc
{
    ShaderTextureDesc() : hashed_name( 0 ), slot( 0 ), dimm( 0 ), is_cubemap( 0 ), pass_index( -1 ) {}
    std::string name;
    uint32_t hashed_name = 0;
    uint8_t slot = 0;
    uint8_t stage_mask = 0;
    uint8_t dimm = 0;
    uint8_t is_cubemap = 0;
    int8_t pass_index = -1;
};

struct ShaderSamplerDesc
{
    std::string name;
    uint32_t hashed_name = 0;
    uint8_t slot = 0;
    uint8_t stage_mask = 0;
    int8_t pass_index = -1;
};

}}///


struct RDIShaderReflection
{
	RDIShaderReflection() {}

	std::vector<bx::rdi::ShaderCBufferDesc> cbuffers;
	std::vector<bx::rdi::ShaderTextureDesc> textures;
	std::vector<bx::rdi::ShaderSamplerDesc> samplers;
	RDIVertexLayout vertex_layout;
	uint16_t input_mask;
	uint16_t __pad0[1];
};
