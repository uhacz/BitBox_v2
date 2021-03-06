#include "gfx.h"
#include "gfx_internal.h"
#include "gfx_resource_loader.h"
#include <foundation/serializer.h>
#include "entity/entity_system.h"
#include "asset_app/components.h"
namespace gfx_shader
{
#include <shaders/hlsl/shadow_data.h>

}//

void Serialize( SRLInstance* srl, gfx_shader::Material* obj )
{
    SRL_ADD( 0, obj->diffuse_albedo );
    SRL_ADD( 0, obj->roughness );
    SRL_ADD( 0, obj->specular_albedo );
    SRL_ADD( 0, obj->metal );
}
void Serialize( SRLInstance* srl, GFXMaterialResource* obj )
{
    Serialize( srl, &obj->data );
    for( uint32_t i = 0; i < GFXEMaterialTextureSlot::_COUNT_; ++i )
        SRL_ADD( 0, obj->textures[i] );
}

//////////////////////////////////////////////////////////////////////////
struct GFXUtilsData
{
    BXIAllocator* allocator = nullptr;
    struct
    {
        RDIXPipeline* copy_rgba = nullptr;
    }pipeline;
};
// ---
void GFXUtils::StartUp( RDIDevice* dev, BXIFilesystem* filesystem, BXIAllocator* allocator )
{
    data->allocator = allocator;
    RDIXShaderFile* shader_file = LoadShaderFile( "shader/hlsl/bin/texture_util.shader", filesystem, allocator );

    RDIXPipelineDesc pipeline_desc = {};
    pipeline_desc.Shader( shader_file, "copy_rgba" );
    data->pipeline.copy_rgba = CreatePipeline( dev, pipeline_desc, allocator );


    UnloadShaderFile( &shader_file, allocator );
}

void GFXUtils::ShutDown( RDIDevice* dev )
{
    DestroyPipeline( &data->pipeline.copy_rgba );

    data->allocator = nullptr;
}

void GFXUtils::CopyTexture( RDICommandQueue * cmdq, RDITextureRW * output, const RDITextureRW & input, float aspect )
{
    const RDITextureInfo& src_info = input.info;
    int32_t viewport[4] = {};
    RDITextureInfo dst_info = {};

    if( output )
    {
        dst_info = input.info;
    }
    else
    {
        RDITextureRW back_buffer = GetBackBufferTexture( cmdq );
        dst_info = back_buffer.info;
    }

    ComputeViewport( viewport, aspect, dst_info.width, dst_info.height, src_info.width, src_info.height );

    RDIXResourceBinding* resources = ResourceBinding( data->pipeline.copy_rgba );
    SetResourceRO( resources, "texture0", &input );
    if( output )
        ChangeRenderTargets( cmdq, output, 1, RDITextureDepth(), false );
    else
        ChangeToMainFramebuffer( cmdq );

    SetViewport( cmdq, RDIViewport::Create( viewport ) );
    BindPipeline( cmdq, data->pipeline.copy_rgba, true );
    Draw( cmdq, 6, 0 );
}

//////////////////////////////////////////////////////////////////////////



struct GFXShaderLoadHelper
{
    GFXShaderLoadHelper( RDIDevice* dev, BXIFilesystem* filesystem, BXIAllocator* allocator )
        : _dev( dev ), _filesystem( filesystem ), _allocator( allocator ) {}

    ~GFXShaderLoadHelper()
    {
        UnloadShaderFile( &_shader_file, _allocator );
    }

    RDIDevice* _dev;
    BXIFilesystem* _filesystem;
    BXIAllocator* _allocator;

    const char* _shader_filename = nullptr;
    RDIXShaderFile* _shader_file = nullptr;

    RDIXPipeline* CreatePipeline( const char* shader_filename, const char* pass_name )
    {
        if( _shader_filename && string::equal( shader_filename, _shader_filename ) == false )
        {
            UnloadShaderFile( &_shader_file, _allocator );
            _shader_filename = nullptr;
        }
        if( !_shader_file )
        {
            _shader_file = LoadShaderFile( shader_filename, _filesystem, _allocator );
            _shader_filename = shader_filename;
        }
        if( _shader_file )
        {
            RDIXPipelineDesc pipeline_desc = {};
            pipeline_desc.Shader( _shader_file, pass_name );
            return ::CreatePipeline( _dev, pipeline_desc, _allocator );
        }
        return nullptr;
    }
};

namespace gfx_internal
{
    static void ClearTextures( RDIXResourceBinding* binding )
    {
        SetResourceRO( binding, "tex_material_base_color", nullptr );
        SetResourceRO( binding, "tex_material_normal"    , nullptr );
        SetResourceRO( binding, "tex_material_roughness" , nullptr );
        SetResourceRO( binding, "tex_material_metalness" , nullptr );
    }
    static void ApplyTextures( RDIXResourceBinding* binding, RDITextureRO* rdi_tex[GFXEMaterialTextureSlot::_COUNT_] )
    {
        SetResourceRO( binding, "tex_material_base_color", rdi_tex[GFXEMaterialTextureSlot::BASE_COLOR] );
        SetResourceRO( binding, "tex_material_normal"    , rdi_tex[GFXEMaterialTextureSlot::NORMAL] );
        SetResourceRO( binding, "tex_material_roughness" , rdi_tex[GFXEMaterialTextureSlot::ROUGHNESS] );
        SetResourceRO( binding, "tex_material_metalness" , rdi_tex[GFXEMaterialTextureSlot::METALNESS] );
    }

    static void RefreshPendingMaterials( GFXSystem* gfx )
    {
        GFXMaterialContainer& mc = gfx->_material;
        {
            static_array_t<id_t, GFX_MAX_MATERIALS> to_queue;
            
            scope_mutex_t guard( mc.to_refresh_lock );
            while( !array::empty( mc.to_refresh ) )
            {
                const id_t id = array::back( mc.to_refresh );
                array::pop_back( mc.to_refresh );

                if( !mc.IsAlive( id ) )
                    continue;

                const GFXMaterialTexture& textures = mc.textures[id.index];

                RDITextureRO* rdi_tex[GFXEMaterialTextureSlot::_COUNT_] = {};
                const RSMLoadState state = GetLoadState( rdi_tex, textures.id, GFXEMaterialTextureSlot::_COUNT_ );
                if( state.nb_loaded == GFXEMaterialTextureSlot::_COUNT_ )
                {
                    ApplyTextures( mc.binding[id.index], rdi_tex );
                }
                else if( state.nb_failed )
                {
                    ClearTextures( mc.binding[id.index] );
                    break;
                }
                else
                {
                    array::push_back( to_queue, id );
                }
            }

            for( uint32_t i = 0; i < to_queue.size; ++i )
            {
                array::push_back( mc.to_refresh, to_queue[i] );
            }
        }
    }

    static void ReleaseMeshInstance( GFXSceneContainer* sc, id_t idscene, id_t idinst )
    {
        if( !sc->IsMeshAlive( idscene, idinst ) )
            return;

        const uint32_t scene_index = idscene.index;
        const uint32_t data_index = sc->MeshInstanceIndex( idscene, idinst );// id_allocator::dense_index( sc->mesh_idalloc[scene_index], idinst );

        {
            scope_lock_t<mutex_t> lck( sc->mesh_lock[scene_index] );
            const auto remove_info = id_allocator::free( sc->mesh_idalloc[scene_index], idinst );

            SYS_ASSERT( remove_info.copy_data_to_index == data_index );
            (void)remove_info;


            RSMResourceID mesh_res_id = sc->mesh_data[scene_index]->idmesh_resource[data_index];
            RDIXRenderSource* rsource = (RDIXRenderSource*)RSM::Get( mesh_res_id );
            if( RSM::Release( mesh_res_id ) )
            {
                DestroyRenderSource( &rsource );
            }

            DestroyRenderSource( &sc->mesh_data[scene_index]->skinning_data[data_index].rsource );

            container_soa::remove_packed( sc->mesh_data[scene_index], data_index );
        }
    }

    static void RemovePendingObjects( GFXSystem* gfx )
    {
        {// cameras
            GFXCameraContainer& cc = gfx->_camera;
            scope_mutex_t guard( cc.to_remove_lock );
            while( !array::empty( cc.to_remove ) )
            {
                id_t id = array::back( cc.to_remove );
                array::pop_back( cc.to_remove );

                const uint32_t data_index = cc.DataIndex( id );
                string::free( &cc.names[data_index] );

                {
                    scope_mutex_t id_guard( cc.id_lock );
                    id_array::destroy( cc.id_alloc, id );
                }
            }
        }

        {// mesh instances
            GFXSceneContainer& sc = gfx->_scene;
            scope_lock_t<mutex_t> lock_guard( sc.mesh_to_remove_lock );
            while( !array::empty( sc.mesh_to_remove ) )
            {
                GFXSceneContainer::DeadMeshInstanceID dead_id = array::back( sc.mesh_to_remove );
                array::pop_back( sc.mesh_to_remove );

                ReleaseMeshInstance( &sc, dead_id.idscene, dead_id.idinst );
            }
        }

        {// -- scenes
            GFXSceneContainer& sc = gfx->_scene;
            scope_lock_t<mutex_t> lock_guard( sc.to_remove_lock );
            while( !array::empty( sc.to_remove ) )
            {
                id_t id = array::back( sc.to_remove );
                array::pop_back( sc.to_remove );

                while( container_soa::size( sc.mesh_data[id.index] ) )
                {
                    const uint32_t last_index = container_soa::size( sc.mesh_data[id.index] ) - 1;
                    const id_t idinst = sc.mesh_data[id.index]->idinstance[last_index];
                    ReleaseMeshInstance( &sc, id, idinst );
                }

                Destroy( &sc.sky_data[id.index].sky_texture );

                DestroyCommandBuffer( &sc.command_buffer[id.index], gfx->_allocator );
                DestroyTransformBuffer( &sc.transform_buffer[id.index], gfx->_allocator );
                
                container_soa::destroy( &sc.mesh_data[id.index] );
                id_allocator::destroy( &sc.mesh_idalloc[id.index] );

                {
                    scope_lock_t<mutex_t> lck( sc.lock );
                    id_table::destroy( sc.idtable, id );
                }
            }
        }

        {// --- materials
            GFXMaterialContainer& mc = gfx->_material;
            BXIAllocator* allocator = gfx->_allocator;

            scope_lock_t<mutex_t> lock_guard( mc.to_remove_lock );
            while ( !array::empty( mc.to_remove ) )
            {
                id_t id = array::back( mc.to_remove );
                array::pop_back( mc.to_remove );

                string::free( &mc.name[id.index] );
                mc.idself[id.index] = makeInvalidHandle<id_t>();

                DestroyResourceBinding( &mc.binding[id.index] );
                //Destroy( &mc.data_gpu[id.index] );
                
                for( uint32_t itex = 0; itex < GFXEMaterialTextureSlot::_COUNT_; ++itex )
                    RSM::Release( mc.textures[id.index].id[itex] );

                {
                    scope_lock_t<mutex_t> lck( mc.lock );
                    id_table::destroy( mc.idtable, id );
                }
            }
        }
    }

    static bool IsMaterialAlive( GFXSystem* gfx, id_t id )
    {
        return id_table::has( gfx->_material.idtable, id );
    }
    static  id_t FindMaterial( GFXSystem* gfx, const char* name )
    {
        const GFXMaterialContainer& materials = gfx->_material;
        for( uint32_t i = 0; i < GFX_MAX_MATERIALS; ++i )
        {
            if( string::equal( materials.name[i].c_str(), name ) )
            {
                if( id_table::has( materials.idtable, materials.idself[i] ) )
                    return materials.idself[i];
            }
        }
        return makeInvalidHandle<id_t>();
    }

    static void UploadMaterial( GFXSystem* gfx, id_t id, const gfx_shader::Material& data )
    {
        GFXMaterialContainer& mc = gfx->_material;
        mc.data[id.index] = data;
    }

    static void ReleaseResources( array_span_t<RSMResourceID> ids )
    {
        //scope_mutex_t guard( mc->resource_to_release_lock );
        for( RSMResourceID id : ids )
        {
            RSM::Release( id );
            //if( id.i != RSMResourceID::Null().i )
            //    array::push_back( mc->resource_to_release, id );
        }
    }
    static void ChangeTextures( GFXSystem* gfx, id_t id, const GFXMaterialTexture& tex )
    {
        GFXMaterialContainer& mc = gfx->_material;
        const uint32_t index = mc.DataIndex( id );

        bool hasTextures = false;
        for( uint32_t i = 0; i < GFXEMaterialTextureSlot::_COUNT_ && !hasTextures; ++i )
            hasTextures |= RSM::IsAlive( tex.id[i] );

        if( hasTextures )
        {
            if( mc.flags[index] == GFXEMaterialFlag::PIPELINE_BASE || mc.flags[index] == 0 )
            {
                DestroyResourceBinding( &mc.binding[index] );
                mc.binding[index] = CloneResourceBinding( ResourceBinding( mc.pipeline.full ), gfx->_allocator );
                mc.flags[index] = GFXEMaterialFlag::PIPELINE_FULL;
            }
            ReleaseResources( array_span_t<RSMResourceID>( mc.textures[index].id, GFXEMaterialTextureSlot::_COUNT_ ) );
            mc.textures[index] = tex;

            ClearTextures( mc.binding[index] );
            {
                scope_mutex_t guard( mc.to_refresh_lock );
                array::push_back( mc.to_refresh, id );
            }
        }
        else
        {
            if( mc.flags[index] == GFXEMaterialFlag::PIPELINE_FULL || mc.flags[index] == 0 )
            {
                ReleaseResources( array_span_t<RSMResourceID>( mc.textures[index].id, GFXEMaterialTextureSlot::_COUNT_ ) );
                DestroyResourceBinding( &mc.binding[index] );
                mc.binding[index] = CloneResourceBinding( ResourceBinding( mc.pipeline.base_with_skybox ), gfx->_allocator );
                mc.flags[index] = GFXEMaterialFlag::PIPELINE_BASE;
            }

            mc.textures[index] = {};
        }
    }
    static RDIXResourceBinding* GetMaterialBinding( GFXSystem* gfx, id_t id )
    {
        if( !IsMaterialAlive( gfx, id ) )
        {
            id_t fallback_id = { gfx->_fallback_idmaterial.i };
            return gfx->_material.binding[fallback_id.index];
        }
        return  gfx->_material.binding[id.index];
    }
    static RDIXPipeline* GetMaterialPipeline( GFXSystem* gfx, id_t id )
    {
        if( !IsMaterialAlive( gfx, id ) )
        {
            return gfx->_material.pipeline.base;
        }

        const uint8_t flags = gfx->_material.flags[id.index];
        SYS_ASSERT( flags & (GFXEMaterialFlag::PIPELINE_BASE | GFXEMaterialFlag::PIPELINE_FULL) );
        return (flags & GFXEMaterialFlag::PIPELINE_BASE) ? gfx->_material.pipeline.base_with_skybox : gfx->_material.pipeline.full;
    }
}

static GFX* GFXAllocate( BXIAllocator* allocator )
{
    uint32_t mem_size = 0;
    mem_size += sizeof( GFX );
    mem_size += sizeof( GFXSystem );
    mem_size += sizeof( GFXUtils ) + sizeof( GFXUtilsData );

    void* memory = BX_MALLOC( allocator, mem_size, 16 );
    memset( memory, 0x00, mem_size );

    GFX* gfx_interface = (GFX*)memory;
    gfx_interface->gfx = new(gfx_interface + 1) GFXSystem();

    gfx_interface->utils = (GFXUtils*)(gfx_interface->gfx + 1);
    gfx_interface->utils->data = new(gfx_interface->utils + 1) GFXUtilsData();

    return gfx_interface;
}

static void GFXFree( GFX** gfxi, BXIAllocator* allocator )
{
    gfxi[0]->gfx->~GFXSystem();
    gfxi[0]->utils->data->~GFXUtilsData();
    BX_DELETE0( allocator, gfxi[0] );
}

GFX* GFX::StartUp( RDIDevice* dev, const GFXDesc& desc, BXIFilesystem* filesystem, BXIAllocator* allocator )
{
    GFX* gfx_interface = GFXAllocate( allocator );
    GFXSystem* gfx = gfx_interface->gfx;

    gfx->_camera._names_allocator = allocator;
    gfx->_allocator = allocator;
    gfx->_rdidev = dev;

    { // loaders
        RSM::RegisterLoader<GFXMeshResourceLoader>();
        RSM::RegisterLoader<GFXTextureResourceLoader>();
        RSM::RegisterLoader<GFXShaderResourceLoader>();
    }

    gfx_interface->utils->StartUp( dev, filesystem, allocator );
    
    {// --- framebuffer
        RDIXRenderTargetDesc render_target_desc( desc.framebuffer_width, desc.framebuffer_height );
        render_target_desc.Texture( GFXEFramebuffer::COLOR, RDIFormat::Float4() );
        render_target_desc.Texture( GFXEFramebuffer::COLOR_SWAP, RDIFormat::Float4() );
        render_target_desc.Texture( GFXEFramebuffer::SHADOW, RDIFormat::Float() ); // shadows
        render_target_desc.Depth( RDIEType::DEPTH32F );
        gfx->_framebuffer = CreateRenderTarget( dev, render_target_desc, allocator );
    }
    {
        gfx->_gpu_camera_buffer = CreateConstantBuffer( dev, sizeof( gfx_shader::CameraData ), nullptr );
        gfx->_gpu_frame_data_buffer = CreateConstantBuffer( dev, sizeof( gfx_shader::GlobalFrameData ), nullptr );
        gfx->_gpu_material_data_buffer = CreateConstantBuffer( dev, sizeof( gfx_shader::Material ) * GFX_MAX_MATERIALS, nullptr );
    }

    {// --- samplers
        RDISamplerDesc desc = {};

        desc.Filter( RDIESamplerFilter::NEAREST );
        gfx->_samplers._point = CreateSampler( dev, desc );

        desc.Filter( RDIESamplerFilter::LINEAR );
        gfx->_samplers._linear = CreateSampler( dev, desc );

        desc.Filter( RDIESamplerFilter::BILINEAR_ANISO );
        gfx->_samplers._bilinear = CreateSampler( dev, desc );

        desc.Filter( RDIESamplerFilter::TRILINEAR_ANISO );
        gfx->_samplers._trilinear = CreateSampler( dev, desc );
    }
    
    GFXShaderLoadHelper helper( dev, filesystem, allocator );
    {// --- material
        GFXMaterialContainer& materials = gfx->_material;
        materials.pipeline.base = helper.CreatePipeline( "shader/hlsl/bin/material.shader", "base" );
        materials.pipeline.base_with_skybox = helper.CreatePipeline( "shader/hlsl/bin/material.shader", "base_with_skybox" );
        materials.pipeline.full = helper.CreatePipeline( "shader/hlsl/bin/material.shader", "full" );
        materials.pipeline.skybox = helper.CreatePipeline( "shader/hlsl/bin/skybox.shader", "skybox" );
        materials.pipeline.shadow_depth = helper.CreatePipeline( "shader/hlsl/bin/shadow.shader", "shadow_depth" );

        //materials.frame_data_gpu = CreateConstantBuffer( dev, sizeof( gfx_shader::MaterialFrameData ) );
        materials.lighting_data_gpu = CreateConstantBuffer( dev, sizeof( gfx_shader::LightningFrameData ) );
    }

    { // --- post process
        GFXPostProcess& pp = gfx->_postprocess;
        pp.shadow.pipeline_ss = helper.CreatePipeline( "shader/hlsl/bin/shadow.shader", "shadow_ss" );
        pp.shadow.pipeline_combine = helper.CreatePipeline( "shader/hlsl/bin/shadow.shader", "shadow_combine" );
        pp.shadow.cbuffer_fdata = CreateConstantBuffer( dev, sizeof( gfx_shader::ShadowData ) );
    }

    { // fallback material
        GFXMaterialDesc material_desc;
        material_desc.data.diffuse_albedo = vec3_t( 1.0, 0.0, 0.0 );
        material_desc.data.roughness = 0.9f;
        material_desc.data.specular_albedo = vec3_t( 1.0, 0.0, 0.0 );
        material_desc.data.metal = 0.f;
        gfx->_fallback_idmaterial = gfx_interface->CreateMaterial( "fallback", material_desc );
    }

    { // default materials
        { // red
            GFXMaterialDesc mat;
            mat.data.specular_albedo = vec3_t( 1.f, 0.f, 0.f );
            mat.data.diffuse_albedo = vec3_t( 1.f, 0.f, 0.f );
            mat.data.metal = 0.f;
            mat.data.roughness = 0.15f;
            gfx->_default_materials[0] = gfx_interface->CreateMaterial( "red", mat );
        }
        { // green
            GFXMaterialDesc mat;
            mat.data.specular_albedo = vec3_t( 0.f, 1.f, 0.f );
            mat.data.diffuse_albedo = vec3_t( 0.f, 1.f, 0.f );
            mat.data.metal = 0.f;
            mat.data.roughness = 0.25f;
            gfx->_default_materials[1] = gfx_interface->CreateMaterial( "green", mat );
        }
        { // blue
            GFXMaterialDesc mat;
            mat.data.specular_albedo = vec3_t( 0.f, 0.f, 1.f );
            mat.data.diffuse_albedo = vec3_t( 0.f, 0.f, 1.f );
            mat.data.metal = 0.f;
            mat.data.roughness = 0.25f;
            gfx->_default_materials[2] = gfx_interface->CreateMaterial( "blue", mat );
        }
        {
            GFXMaterialDesc mat;
            mat.data.specular_albedo = vec3_t( 0.5f, 0.5f, 0.5f );
            mat.data.diffuse_albedo = vec3_t( 0.5f, 0.5f, 0.5f );
            mat.data.metal = 0.f;
            mat.data.roughness = 0.5f;
            gfx->_default_materials[3] = gfx_interface->CreateMaterial( "rough", mat );
        }
        { // checkboard
            GFXMaterialDesc mat;
            mat.data.specular_albedo = vec3_t( 0.5f, 0.5f, 0.5f );
            mat.data.diffuse_albedo = vec3_t( 0.5f, 0.5f, 0.5f );
            mat.data.metal = 0.f;
            mat.data.roughness = 0.5f;

            mat.textures.id[GFXEMaterialTextureSlot::BASE_COLOR] = RSM::Load( "texture/checkboard/d.DDS", gfx_interface );
            mat.textures.id[GFXEMaterialTextureSlot::NORMAL]     = RSM::Load( "texture/checkboard/n.DDS", gfx_interface );
            mat.textures.id[GFXEMaterialTextureSlot::ROUGHNESS]  = RSM::Load( "texture/checkboard/r.DDS", gfx_interface );
            mat.textures.id[GFXEMaterialTextureSlot::METALNESS]  = RSM::Load( "texture/not_metal.DDS", gfx_interface );

            gfx->_default_materials[4] = gfx_interface->CreateMaterial( "checkboard", mat );
        }
    }
    { // fallback mesh
        par_shapes_mesh* shape = par_shapes_create_parametric_sphere( 8, 8, FLT_EPSILON );
        gfx->_fallback_mesh = CreateRenderSourceFromShape( dev, shape, allocator );
        par_shapes_free_mesh( shape );
    }
    { // default meshes
        const char* names[] = { "sphere", "box" };

        poly_shape_t shapes[GFXSystem::NUM_DEFAULT_MESHES] = {};
        poly_shape::createShpere( &shapes[0], 10, allocator );
        poly_shape::createBox( &shapes[1], 4, allocator );
        const uint32_t n_shapes = (uint32_t)sizeof_array( shapes );
        for( uint32_t i = 0; i < GFXSystem::NUM_DEFAULT_MESHES; ++i )
        {
            RDIXRenderSource* rsource = CreateRenderSourceFromShape( gfx->_rdidev, &shapes[i], allocator );
            gfx->_default_meshes[i] = RSM::Create( names[i], rsource );
        }
        for( uint32_t i = 0; i < GFXSystem::NUM_DEFAULT_MESHES; ++i )
        {
            poly_shape::deallocateShape( &shapes[i] );
        }
    }

    { // skinning
        gfx->_scene.cpu_skinning_data.Initialize( allocator, GFX_SKINNING_BUFFER_SIZE );
        gfx->_scene.gpu_skinning_data.Initialize( dev, GFX_SKINNING_BUFFER_SIZE );
        gfx->_scene.gpu_skinning_pipeline_pos_nrm = helper.CreatePipeline( "shader/hlsl/bin/skinning.shader", "pos_nrm" );
    }


    return gfx_interface;
}


void GFX::ShutDown( GFX** gfx_interface_handle )
{
    if( !gfx_interface_handle[0] )
        return;

    GFX* gfx_interface = gfx_interface_handle[0];
    GFXSystem* gfx = gfx_interface->gfx;
    RDIDevice* dev = gfx->_rdidev;
    BXIAllocator* allocator = gfx->_allocator;

    gfx_internal::RemovePendingObjects( gfx );

    { // skinning
        DestroyPipeline( &gfx->_scene.gpu_skinning_pipeline_pos_nrm );
        gfx->_scene.gpu_skinning_data.Uninitialize( dev );
        gfx->_scene.cpu_skinning_data.Uninitialize();
    }

    {// --- postprocess
        DestroyPipeline( &gfx->_postprocess.shadow.pipeline_ss );
        DestroyPipeline( &gfx->_postprocess.shadow.pipeline_combine );
        ::Destroy( &gfx->_postprocess.shadow.cbuffer_fdata );
    }

    {// --- material
        ::Destroy( &gfx->_material.lighting_data_gpu );
        //::Destroy( &gfx->_material.frame_data_gpu );
        DestroyPipeline( &gfx->_material.pipeline.base );
        DestroyPipeline( &gfx->_material.pipeline.base_with_skybox );
        DestroyPipeline( &gfx->_material.pipeline.full );
        DestroyPipeline( &gfx->_material.pipeline.skybox );
        DestroyPipeline( &gfx->_material.pipeline.shadow_depth );
    }

    for( uint32_t i = 0; i < GFXSystem::NUM_DEFAULT_MESHES; ++i )
    {
        RDIXRenderSource* rsource = nullptr;
        
        bool released = RSM::Release( gfx->_default_meshes[i], (void**)&rsource );
        SYS_ASSERT( released );
        
        gfx->_default_meshes[i] = RSMResourceID::Null();
        DestroyRenderSource( &rsource );
    }
    DestroyRenderSource( &gfx->_fallback_mesh );
    
    gfx_interface->DestroyMaterial( gfx->_fallback_idmaterial );
    for( uint32_t i = 0; i < GFXSystem::NUM_DEFAULT_MATERIALS; ++i )
    {
        gfx_interface->DestroyMaterial( gfx->_default_materials[i] );
    }
    
    gfx_internal::RemovePendingObjects( gfx );
    SYS_ASSERT( gfx->_frame_ctx.Valid() == false );

    {// --- samplers
        ::Destroy( &gfx->_samplers._trilinear );
        ::Destroy( &gfx->_samplers._bilinear );
        ::Destroy( &gfx->_samplers._linear );
        ::Destroy( &gfx->_samplers._point );
    }

    ::Destroy( &gfx->_gpu_material_data_buffer );
    ::Destroy( &gfx->_gpu_frame_data_buffer );
    ::Destroy( &gfx->_gpu_camera_buffer );

    DestroyRenderTarget( dev, &gfx->_framebuffer );
   
    gfx_interface->utils->ShutDown( dev );
    GFXFree( gfx_interface_handle, allocator );
}

RDIXRenderTarget* GFX::Framebuffer()
{
    return gfx->_framebuffer;
}

RDIXPipeline* GFX::MaterialBase()
{
    return gfx->_material.pipeline.base;
}

RDIDevice* GFX::Device()
{
    return gfx->_rdidev;
}

GFXCameraID GFX::CreateCamera( const char* name, const GFXCameraParams& params, const mat44_t& world )
{
    GFXCameraContainer& cc = gfx->_camera;

    id_t id = {};
    {
        scope_mutex_t guard( cc.id_lock );
        id = id_array::create( cc.id_alloc );
    }

    const uint32_t data_index = cc.DataIndex( id );
    cc.params[data_index] = params;
    cc.matrices[data_index].world = world;
    string::create( &cc.names[data_index], name, cc._names_allocator );

    return { id.hash };
}

void GFX::DestroyCamera( GFXCameraID idcam )
{
    id_t id = { idcam.i };

    GFXCameraContainer& cc = gfx->_camera;
    if( !cc.IsAlive( id ) )
        return;

    {
        scope_mutex_t guard( cc.id_lock );
        id = id_array::invalidate( cc.id_alloc, id );
    }
    {
        scope_mutex_t guard( cc.to_remove_lock );
        array::push_back( cc.to_remove, id );
    }
}

GFXCameraID GFX::FindCamera( const char* name ) const
{
    GFXCameraContainer& cc = gfx->_camera;
    
    uint32_t n = 0;
    {
        scope_mutex_t guard( cc.id_lock );
        n = id_array::size( cc.id_alloc );
    }

    for( uint32_t i = 0; i < n; ++i )
    {
        if( string::equal( cc.names[i].c_str(), name ) )
        {
            return { id_array::id( cc.id_alloc, i ).hash };
        }
    }
    return { 0 };
}

bool GFX::IsCameraAlive( GFXCameraID idcam )
{
    return gfx->_camera.IsAlive( { idcam.i } );
}

const GFXCameraParams& GFX::CameraParams( GFXCameraID idcam ) const
{
    const uint32_t index = gfx->_camera.DataIndex( { idcam.i } );
    return gfx->_camera.params[index];
}

const GFXCameraMatrices& GFX::CameraMatrices( GFXCameraID idcam ) const
{
    const uint32_t index = gfx->_camera.DataIndex( { idcam.i } );
    return gfx->_camera.matrices[index];
}

void GFX::SetCameraParams( GFXCameraID idcam, const GFXCameraParams& params )
{
    const uint32_t index = gfx->_camera.DataIndex( { idcam.i } );
    gfx->_camera.params[index] = params;
}

void GFX::SetCameraWorld( GFXCameraID idcam, const mat44_t& world )
{
    const uint32_t index = gfx->_camera.DataIndex( { idcam.i } );
    gfx->_camera.matrices[index].world = world;
}

void GFX::ComputeCamera( GFXCameraID idcam, GFXCameraMatrices* output )
{
    const uint32_t index = gfx->_camera.DataIndex( { idcam.i } );

    const GFXCameraParams& params = gfx->_camera.params[index];
    const GFXCameraMatrices& matrices = gfx->_camera.matrices[index];

    if( !output )
        output = &gfx->_camera.matrices[index];

    ComputeMatrices( output, params, matrices.world );
}

GFXMaterialID GFX::CreateMaterial( const char* name, const GFXMaterialDesc& desc )
{
    id_t id = gfx_internal::FindMaterial( gfx, name );
    if( gfx_internal::IsMaterialAlive( gfx, id ) )
        return {0};

    RDIDevice* dev = gfx->_rdidev;
    BXIAllocator* allocator = gfx->_allocator;

    GFXMaterialContainer& mc = gfx->_material;

    mc.lock.lock();
    id = id_table::create( mc.idtable );
    mc.lock.unlock();

    mc.data[id.index] = desc.data;
    mc.flags[id.index] = 0;

    gfx_internal::ChangeTextures( gfx, id, desc.textures );
    
    string::create( &mc.name[id.index], name, allocator );
    mc.idself[id.index] = id;

    return { id.hash };
}

void GFX::DestroyMaterial( GFXMaterialID idmat )
{
    GFXMaterialContainer& mc = gfx->_material;

    id_t id = { idmat.i };
    if( !id_table::has( mc.idtable, id ) )
        return;

    mc.lock.lock();
    id = id_table::invalidate( mc.idtable, id );
    mc.lock.unlock();

    mc.to_remove_lock.lock();
    array::push_back( mc.to_remove, id );
    mc.to_remove_lock.unlock();
}

void GFX::SetMaterialData( GFXMaterialID idmat, const gfx_shader::Material& data )
{
    if( !IsMaterialAlive( idmat ) )
        return;
    id_t id = { idmat.i };
    gfx_internal::UploadMaterial( gfx, id, data );
}

void GFX::SetMaterialTextures( GFXMaterialID idmat, const GFXMaterialTexture& tex )
{
    if( !IsMaterialAlive( idmat ) )
        return;
    
    const id_t id = { idmat.i };
    gfx_internal::ChangeTextures( gfx, id, tex );
}

GFXMaterialID GFX::FindMaterial( const char* name )
{
    return { gfx_internal::FindMaterial( gfx, name ).hash };
}

bool GFX::IsMaterialAlive( GFXMaterialID idmat )
{
    return gfx->_material.IsAlive( { idmat.i } );
}

RDIXResourceBinding* GFX::MaterialBinding( GFXMaterialID idmat )
{
    const id_t id = { idmat.i };
    return gfx_internal::GetMaterialBinding( gfx, id );
}

GFXSceneID GFX::CreateScene( const GFXSceneDesc& desc )
{
    GFXSceneContainer& sc = gfx->_scene;

    gfx->_scene.lock.lock();
    id_t idscene = id_table::create( sc.idtable );
    gfx->_scene.lock.unlock();

    RDIXTransformBufferDesc transform_buffer_desc = {};
    transform_buffer_desc.capacity = desc.max_renderables;
    RDIXTransformBuffer* transform_buffer = CreateTransformBuffer( gfx->_rdidev, transform_buffer_desc, gfx->_allocator );

    const uint32_t cmd_buffer_size = 128;// desc.max_renderables * 8 + 128;
    const uint32_t cmd_buffer_data_size = 1024; // GetDataCapacity( transform_buffer );
    RDIXCommandBuffer* command_buffer = CreateCommandBuffer( gfx->_allocator, cmd_buffer_size, cmd_buffer_data_size );
    
    container_soa_desc_t cnt_desc;
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, world_matrix );
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, instance_data );
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, idmesh_resource );
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, skinning_data );
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, idmat );
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, idinstance );

    GFXSceneContainer::MeshData* mesh_data = nullptr;
    container_soa::create( &mesh_data, cnt_desc, desc.max_renderables, gfx->_allocator );

    GFXSceneContainer::MeshIDAllocator* mesh_idalloc = nullptr;
    mesh_idalloc = id_allocator::create_dense( desc.max_renderables, gfx->_allocator );

    const uint32_t scene_index = idscene.index;
    
    sc.command_buffer[scene_index] = command_buffer;
    sc.transform_buffer[scene_index] = transform_buffer;
    sc.mesh_data[scene_index] = mesh_data;
    sc.mesh_idalloc[scene_index] = mesh_idalloc;

    return { idscene.hash };
}

void GFX::DestroyScene( GFXSceneID idscene )
{
    GFXSceneContainer& sc = gfx->_scene;
    
    id_t id = { idscene.i };
    if( !sc.IsSceneAlive( id ) )
        return;

    sc.lock.lock();
    id = id_table::invalidate( sc.idtable, id );
    sc.lock.unlock();

    sc.to_remove_lock.lock();
    array::push_back( sc.to_remove, id );
    sc.to_remove_lock.unlock();
}

namespace
{
    static inline GFXMeshInstanceID EncodeMeshInstanceID( id_t idscene, id_t idmesh )
    {
        return { ((uint64_t)idscene.hash << 32ull) | (uint64_t)idmesh.hash };
    }
    static inline id_t DecodeSceneID( GFXMeshInstanceID idmesh )
    {
        return { (uint32_t)(idmesh.i >> 32) };
    }
    static inline id_t DecodeMeshInstanceID( GFXMeshInstanceID idmesh )
    {
        return { (uint32_t)idmesh.i };
    }
}

GFXMeshInstanceID GFX::AddMeshToScene( GFXSceneID idscene, const GFXMeshInstanceDesc& desc, const mat44_t& pose )
{
    const id_t idscn = { idscene.i };
    GFXSceneContainer& sc = gfx->_scene;
    if( !sc.IsSceneAlive( idscn ) )
        return { 0 };

    const uint32_t index = idscn.index;

    GFXMaterialID idmat = desc.idmaterial;
    if( !gfx->_material.IsAlive( { idmat.i } ) )
        idmat = gfx->_fallback_idmaterial;

    GFXSceneContainer::MeshData* data = sc.mesh_data[index];
    
    sc.mesh_lock[index].lock();
    const id_t idinst = id_allocator::alloc( sc.mesh_idalloc[index] );
    const uint32_t data_index = container_soa::push_back( data );
    sc.mesh_lock[index].unlock();
        
    SYS_ASSERT( data_index ==  id_allocator::dense_index( sc.mesh_idalloc[index], idinst ) );
    data->world_matrix   [data_index] = pose;
    data->idmesh_resource[data_index] = desc.idmesh_resource;
    if( desc.flags.cpu_skinning || desc.flags.gpu_skinning )
    {
        RDIXRenderSource* base_rsource = (RDIXRenderSource*)RSM::Get( desc.idmesh_resource );
        GFXMeshSkinningData& skinning_data = data->skinning_data[data_index];
        if( desc.flags.cpu_skinning )
        {
            skinning_data.rsource = CloneForCPUSkinning( gfx->_rdidev, base_rsource );
            skinning_data.flag_gpu = 0;
        }
        else if( desc.flags.gpu_skinning )
        {
            skinning_data.rsource = CloneForGPUSkinning( gfx->_rdidev, base_rsource );
            skinning_data.flag_gpu = 1;
        }
        else
        {
            SYS_ASSERT( false );
        }
        skinning_data.pin_index = GFX_DEFAULT_SKINNING_PIN;

    }
    data->idmat          [data_index] = idmat;
    data->idinstance     [data_index] = idinst;

    return EncodeMeshInstanceID( idscn, idinst );
}

void GFX::RemoveMeshFromScene( GFXMeshInstanceID idmeshi )
{
    const id_t idscene = DecodeSceneID( idmeshi );
    const id_t idinst = DecodeMeshInstanceID( idmeshi );
    
    GFXSceneContainer& sc = gfx->_scene;
    if( !sc.IsMeshAlive( idscene, idinst ) )
        return;

    sc.mesh_lock[idscene.index].lock();
    const id_t new_idinst = id_allocator::invalidate( sc.mesh_idalloc[idscene.index], idinst );
    sc.mesh_lock[idscene.index].unlock();

    const uint32_t instance_index = sc.MeshInstanceIndex( idscene, new_idinst );
    sc.mesh_data[idscene.index]->idinstance[instance_index] = new_idinst;

    GFXSceneContainer::DeadMeshInstanceID dead_id;
    dead_id.idscene = idscene;
    dead_id.idinst = new_idinst;

    sc.mesh_to_remove_lock.lock();
    array::push_back( sc.mesh_to_remove, dead_id );
    sc.mesh_to_remove_lock.unlock();
}

RSMResourceID GFX::Mesh( GFXMeshInstanceID idmeshi )
{
    const id_t idscene = DecodeSceneID( idmeshi );
    const id_t idinst = DecodeMeshInstanceID( idmeshi );

    GFXSceneContainer& sc = gfx->_scene;
    if( !sc.IsMeshAlive( idscene, idinst ) )
        return { 0 };

    return sc.mesh_data[idscene.index]->idmesh_resource[idinst.index];
}

GFXMaterialID GFX::Material( GFXMeshInstanceID idmeshi )
{
    const id_t idscene = DecodeSceneID( idmeshi );
    const id_t idinst = DecodeMeshInstanceID( idmeshi );

    GFXSceneContainer& sc = gfx->_scene;
    if( !sc.IsMeshAlive( idscene, idinst ) )
        return { 0 };

    return sc.mesh_data[idscene.index]->idmat[idinst.index];
}

void GFX::SetWorldPose( GFXMeshInstanceID idmeshi, const mat44_t& pose )
{
    const id_t idscene = DecodeSceneID( idmeshi );
    const id_t idinst = DecodeMeshInstanceID( idmeshi );

    GFXSceneContainer& sc = gfx->_scene;
    const uint32_t instance_index = sc.MeshInstanceIndex( idscene, idinst );
    sc.mesh_data[idscene.index]->world_matrix[instance_index] = pose;

}

blob_t GFX::AcquireSkinnigDataToWrite( GFXMeshInstanceID idmeshi, uint32_t size_in_bytes )
{
    const id_t idscene = DecodeSceneID( idmeshi );
    const id_t idinst = DecodeMeshInstanceID( idmeshi );

    blob_t result;

    GFXSceneContainer& sc = gfx->_scene;
    if( !sc.IsMeshAlive( idscene, idinst ) )
        return result;

    const uint32_t instance_index = sc.MeshInstanceIndex( idscene, idinst );
    GFXMeshSkinningData& mesh_skinning_data = sc.mesh_data[idscene.index]->skinning_data[instance_index];

    GFXSkinningPin pin = {};

    if( mesh_skinning_data.flag_gpu )
    {
        const uint32_t queue_index = std::atomic_fetch_add( &sc.num_meshes_to_skin_gpu, 1 );
        if( queue_index >= GFX_MAX_SKINNING_MESHES )
        {
            SYS_LOG_WARNING( "To much meshes to skin" );
            return result;
        }

        pin = sc.gpu_skinning_data.AcquirePin( size_in_bytes );
        if( pin.address )
        {
            sc.mesh_to_skin_gpu[queue_index] = idmeshi;
        }
    }
    else
    {
        const uint32_t queue_index = std::atomic_fetch_add( &sc.num_meshes_to_skin_cpu, 1 );
        if( queue_index >= GFX_MAX_SKINNING_MESHES )
        {
            SYS_LOG_WARNING( "To much meshes to skin" );
            return result;
        }

        pin = sc.cpu_skinning_data.AcquirePin( size_in_bytes );
        if( pin.address )
        {
            sc.mesh_to_skin_cpu[queue_index] = idmeshi;
        }
        
    }

    if( pin.address )
    {
        mesh_skinning_data.pin_index = pin.index;
        mesh_skinning_data.num_elements = pin.num_elements;

        result.raw = pin.address;
        result.size = size_in_bytes;
    }
    
    return result;
}

void GFX::EnableSky( GFXSceneID idscene, bool value )
{
    GFXSceneContainer& sc = gfx->_scene;
    const uint32_t index = sc.SceneIndexSafe( { idscene.i } );
    if( index != UINT32_MAX )
    {
        sc.sky_data[index].flag_enabled = value;
    }
}

bool GFX::SetSkyTextureDDS( GFXSceneID idscene, const void* data, uint32_t size )
{
    GFXSceneContainer& sc = gfx->_scene;
    const uint32_t index = sc.SceneIndexSafe( { idscene.i } );
    if( index == UINT32_MAX )
        return false;

    RDITextureRO texture = CreateTextureFromDDS( gfx->_rdidev, data, size );
    if( !texture.id )
        return false;

    if( sc.sky_data[index].sky_texture.id )
    {
        Destroy( &sc.sky_data[index].sky_texture );
    }

    sc.sky_data[index].sky_texture = texture;
    return true;
}

void GFX::SetSkyParams( GFXSceneID idscene, const GFXSkyParams& params )
{
    GFXSceneContainer& sc = gfx->_scene;
    const uint32_t index = sc.SceneIndexSafe( { idscene.i } );
    if( index == UINT32_MAX )
        return;

    sc.sky_data[index].params = params;
}

const GFXSkyParams& GFX::SkyParams( GFXSceneID idscene ) const
{
    GFXSceneContainer& sc = gfx->_scene;
    const uint32_t index = sc.SceneIndexSafe( { idscene.i } );
    SYS_ASSERT( index != UINT32_MAX );

    return sc.sky_data[index].params;
}

void GFX::DoSkinningCPU( RDICommandQueue* cmdq )
{
    GFXSceneContainer& sc = gfx->_scene;

    GFXSkinningDataCPU& sd = sc.cpu_skinning_data;
    sd.Swap();

    union BlendIndices
    {
        struct { uint8_t x, y, z, w; };
        uint8_t i[4];
    };

    array_span_t<const float4_t> skinning_data_f4 = sd.BackBuffer<float4_t>();
    const uint32_t num_meshes = std::atomic_exchange( &sc.num_meshes_to_skin_cpu, 0 );
    
    for( uint32_t i = 0; i < num_meshes; ++i )
    {
        const GFXMeshInstanceID id_mesh = sc.mesh_to_skin_cpu[i];
        const id_t idscene = DecodeSceneID( id_mesh );
        const id_t idinst = DecodeMeshInstanceID( id_mesh );
                
        if( !sc.IsMeshAlive( idscene, idinst ) )
            continue;

        const uint32_t instance_index = sc.MeshInstanceIndex( idscene, idinst );
        
        const GFXMeshSkinningData& mesh_skinning = sc.mesh_data[idscene.index]->skinning_data[instance_index];
        SYS_ASSERT( !mesh_skinning.flag_gpu );

        const float4_t* data_begin = &skinning_data_f4[ mesh_skinning.pin_index ];
        const array_span_t<mat44_t> skinning_matrices( (mat44_t*)data_begin, mesh_skinning.num_elements / 4 );

        const RSMResourceID base_id = sc.mesh_data[idscene.index]->idmesh_resource[idinst.index];
        const RDIXRenderSource* base = (RDIXRenderSource*)RSM::Get( base_id );
        const RDIXRenderSource* skinned = sc.mesh_data[idscene.index]->skinning_data[idinst.index].rsource;

        const RDIVertexBuffer vb_blendweights      = FindVertexBuffer( base, RDIEVertexSlot::BLENDWEIGHT );
        const RDIVertexBuffer vb_blendindices      = FindVertexBuffer( base, RDIEVertexSlot::BLENDINDICES );
        const RDIVertexBuffer vb_base_positions    = FindVertexBuffer( base, RDIEVertexSlot::POSITION );
        const RDIVertexBuffer vb_base_normals      = FindVertexBuffer( base, RDIEVertexSlot::NORMAL );
      
        const RDIVertexBuffer vb_skinned_positions = FindVertexBuffer( skinned, RDIEVertexSlot::POSITION );
        const RDIVertexBuffer vb_skinned_normals   = FindVertexBuffer( skinned, RDIEVertexSlot::NORMAL );

        const vec4_t* blendweights = (vec4_t*)Map( cmdq, vb_blendweights, RDIEMapType::READ );
        const BlendIndices* blendindices = (BlendIndices*)Map( cmdq, vb_blendindices, RDIEMapType::READ );

        const vec3_t* base_positions = (vec3_t*)Map( cmdq, vb_base_positions, RDIEMapType::READ );
        const vec3_t* base_normals = (vec3_t*)Map( cmdq, vb_base_normals, RDIEMapType::READ );

        vec3_t* skinned_positions = (vec3_t*)Map( cmdq, vb_skinned_positions, RDIEMapType::WRITE );
        vec3_t* skinned_normals = (vec3_t*)Map( cmdq, vb_skinned_normals, RDIEMapType::WRITE );

        const uint32_t num_vertices = NumVertices( base );
        SYS_ASSERT( NumVertices( skinned ) == num_vertices );

        for( uint32_t ivertex = 0; ivertex < num_vertices; ++ivertex )
        {
            const vec4_t& bw = blendweights[ivertex];
            const BlendIndices bi = blendindices[ivertex];
                        
            const vec3_t& base_pos = base_positions[ivertex];
            const vec3_t& base_nrm = base_normals[ivertex];

            vec3_t skinned_pos( 0.f );
            vec3_t skinned_nrm( 0.f );
            
            skinned_pos += mul_as_point( skinning_matrices[bi.x], base_pos ) * bw.x;
            skinned_pos += mul_as_point( skinning_matrices[bi.y], base_pos ) * bw.y;
            skinned_pos += mul_as_point( skinning_matrices[bi.z], base_pos ) * bw.z;
            skinned_pos += mul_as_point( skinning_matrices[bi.w], base_pos ) * bw.w;

            skinned_nrm += rotate( skinning_matrices[bi.x], base_nrm ) * bw.x;
            skinned_nrm += rotate( skinning_matrices[bi.y], base_nrm ) * bw.y;
            skinned_nrm += rotate( skinning_matrices[bi.z], base_nrm ) * bw.z;
            skinned_nrm += rotate( skinning_matrices[bi.w], base_nrm ) * bw.w;
        
            skinned_positions[ivertex] = skinned_pos;
            skinned_normals[ivertex] = skinned_nrm;
        }

        Unmap( cmdq, vb_skinned_normals );
        Unmap( cmdq, vb_skinned_positions );
        Unmap( cmdq, vb_base_normals );
        Unmap( cmdq, vb_base_positions );
        Unmap( cmdq, vb_blendindices );
        Unmap( cmdq, vb_blendweights );
    }
}

void GFX::DoSkinningGPU( RDICommandQueue* cmdq )
{
    ClearState( cmdq );

    GFXSceneContainer& sc = gfx->_scene;

    GFXSkinningDataGPU& sd = sc.gpu_skinning_data;
    sd.Swap( cmdq );

    const uint32_t num_meshes = std::atomic_exchange( &sc.num_meshes_to_skin_gpu, 0 );

    if( num_meshes )
    {
        BindPipeline( cmdq, sc.gpu_skinning_pipeline_pos_nrm, false );
        sd.Bind( cmdq );
    }

    for( uint32_t i = 0; i < num_meshes; ++i )
    {
        const GFXMeshInstanceID id_mesh = sc.mesh_to_skin_gpu[i];
        const id_t idscene = DecodeSceneID( id_mesh );
        const id_t idinst = DecodeMeshInstanceID( id_mesh );

        if( !sc.IsMeshAlive( idscene, idinst ) )
            continue;

        const uint32_t instance_index = sc.MeshInstanceIndex( idscene, idinst );

        const GFXMeshSkinningData& mesh_skinning = sc.mesh_data[idscene.index]->skinning_data[instance_index];
        SYS_ASSERT( mesh_skinning.flag_gpu );
    
        const RSMResourceID base_id = sc.mesh_data[idscene.index]->idmesh_resource[idinst.index];
        const RDIXRenderSource* base = (RDIXRenderSource*)RSM::Get( base_id );
        const RDIXRenderSource* skinned = mesh_skinning.rsource;

        RDIResourceRO in[] =
        {
            FindVertexBuffer( base, RDIEVertexSlot::POSITION ),
            FindVertexBuffer( base, RDIEVertexSlot::NORMAL ),
            FindVertexBuffer( base, RDIEVertexSlot::BLENDWEIGHT ),
            FindVertexBuffer( base, RDIEVertexSlot::BLENDINDICES ),
        };
        
        RDIResourceRW out[] =
        {
            FindVertexBuffer( skinned, RDIEVertexSlot::POSITION ),
            FindVertexBuffer( skinned, RDIEVertexSlot::NORMAL ),
        };

        const uint32_t num_vertices = NumVertices( base );

        gfx_shader::SkinningData sdata;
        sdata.pin_index = mesh_skinning.pin_index;

        UpdateCBuffer( cmdq, sd._gpu_cbuffer_data, &sdata );
        SetResourcesRO( cmdq, in, SKINNING_INPUT_POS_SLOT, (uint32_t)sizeof_array( in ), RDIEPipeline::COMPUTE_MASK );
        SetResourcesRW( cmdq, out, SKINNING_OUTPUT_POS_SLOT, (uint32_t)sizeof_array( out ), RDIEPipeline::COMPUTE_MASK );

        // dispatch 
        const uint32_t num_groups = iceil( num_vertices, 64 );

        Dispatch( cmdq, num_groups, 1, 1 );
    }

    ClearState( cmdq );
}

GFXFrameContext* GFX::BeginFrame( RDICommandQueue* cmdq )
{
    gfx_internal::RefreshPendingMaterials( gfx );
    gfx_internal::RemovePendingObjects( gfx );

    ClearState( cmdq );
    SetSamplers( cmdq, (RDISampler*)&gfx->_samplers._point, 0, 4, RDIEPipeline::ALL_STAGES_MASK );

    {// cameras
        GFXCameraContainer& cc = gfx->_camera;
        gfx_shader::CameraData cdata;
        const uint32_t n = cc.Size();
        for( uint32_t i = 0; i < n; ++i )
        {
            const GFXCameraParams& camerap = cc.params[i];
            const GFXCameraMatrices& cameram = cc.matrices[i];

            const mat44_t camera_view_proj = cameram.proj_api * cameram.view;

            cdata.world[i] = cameram.world;
            cdata.view[i] = cameram.view;
            cdata.view_proj[i] = camera_view_proj;
            cdata.view_proj_inv[i] = inverse( camera_view_proj );
            cdata.eye[i] = vec4_t( cameram.eye(), 1.f );
            cdata.dir[i] = vec4_t( cameram.dir(), 0.f );
        }
        UpdateCBuffer( cmdq, gfx->_gpu_camera_buffer, &cdata );
    }
    { // materials
        const GFXMaterialContainer& mc = gfx->_material;
        UpdateCBuffer( cmdq, gfx->_gpu_material_data_buffer, mc.data );
    }
   
    SetCbuffers( cmdq, &gfx->_gpu_frame_data_buffer, GLOBAL_FRAME_DATA_SLOT, 1, RDIEPipeline::ALL_STAGES_MASK );
    SetCbuffers( cmdq, &gfx->_gpu_camera_buffer, CAMERA_DATA_SLOT, 1, RDIEPipeline::ALL_STAGES_MASK );
    SetCbuffers( cmdq, &gfx->_gpu_material_data_buffer, MATERIAL_DATA_SLOT, 1, RDIEPipeline::ALL_STAGES_MASK );


    SYS_ASSERT( !gfx->_frame_ctx.Valid() );
    gfx->_frame_ctx.cmdq = cmdq;
    return &gfx->_frame_ctx;
}

void GFX::EndFrame( GFXFrameContext* fctx )
{
    SYS_ASSERT( fctx == &gfx->_frame_ctx );

    ::Swap( fctx->cmdq, gfx->_sync_interval );
    fctx->cmdq = nullptr;
}
void GFX::RasterizeFramebuffer( RDICommandQueue* cmdq, uint32_t texture_index, GFXCameraID idcamera )
{
    const float aspect = CameraParams( idcamera ).aspect();
    utils->CopyTexture( cmdq, nullptr, Texture( gfx->_framebuffer, texture_index), aspect );
}

static void SetupFrameData( gfx_shader::GlobalFrameData* fdata, const RDITextureInfo& render_target_info )
{
    fdata->rtarget_size = vec2_t( (float)render_target_info.width, (float)render_target_info.height );
    fdata->rtarget_size_rcp = vec2_t( 1.f / fdata->rtarget_size.x, 1.f / fdata->rtarget_size.y );
    fdata->main_camera_index = 0;
}

namespace GFXEDrawLayer
{
    enum E : uint8_t
    {
        SKYBOX = 0,
        GEOMETRY,
    };
}//
namespace GFXEDrawStage
{
    enum E : uint8_t
    {
        UPLOAD_GLOBALS = 0,
        DRAW,
        POST,
    };
}//

union GFXSortKey
{
    uint64_t key = 0;
    struct  
    {
        uint64_t depth : 16;
        uint64_t material_index : 16;
        uint64_t stage : 3;
        uint64_t layer : 4;
    };
};


void GFX::GenerateCommandBuffer( GFXFrameContext* fctx, GFXSceneID idscene, GFXCameraID idcamera )
{
    GFXSceneContainer& sc = gfx->_scene;
    if( !sc.IsSceneAlive( { idscene.i } ) )
        return;

    GFXCameraContainer& cc = gfx->_camera;
    if( !cc.IsAlive( { idcamera.i } ) )
        return;

    ComputeCamera( idcamera, nullptr );
    const uint32_t camera_index = cc.DataIndex( { idcamera.i } );
    const GFXCameraParams& camerap = cc.params[camera_index];
    const GFXCameraMatrices& cameram = cc.matrices[camera_index];

    const uint32_t scene_index = make_id( idscene.i ).index;

    RDIXTransformBuffer* transform_buffer = sc.transform_buffer[scene_index];
    RDIXCommandBuffer* cmdbuffer = sc.command_buffer[scene_index];

    ClearTransformBuffer( transform_buffer );
    ClearCommandBuffer( cmdbuffer, gfx->_allocator );
    BeginCommandBuffer( cmdbuffer );

    RDIXTransformBufferBindInfo bind_info;
    bind_info.instance_offset_slot = TRANSFORM_INSTANCE_DATA_SLOT;
    bind_info.matrix_start_slot = TRANSFORM_INSTANCE_WORLD_SLOT;
    bind_info.stage_mask = RDIEPipeline::ALL_STAGES_MASK;

    const uint32_t num_meshes = container_soa::size( sc.mesh_data[scene_index] );
    const array_span_t<RSMResourceID> idmesh_array( sc.mesh_data[scene_index]->idmesh_resource, num_meshes );
    const array_span_t<GFXMeshSkinningData> skinned_mesh_array( sc.mesh_data[scene_index]->skinning_data, num_meshes );
    const array_span_t<GFXMaterialID> idmat_array( sc.mesh_data[scene_index]->idmat, num_meshes );
    const array_span_t<mat44_t> matrix_array( sc.mesh_data[scene_index]->world_matrix, num_meshes );
    
    const bool skybox_enabled = sc.sky_data[scene_index].flag_enabled != 0;
    SYS_ASSERT( skybox_enabled == true );
    //RDIXPipeline* pipeline = (skybox_enabled) ? gfx->_material.pipeline.base_with_skybox : gfx->_material.pipeline.base;

    { // frame data
        const RDITextureInfo framebuffer_info = Texture( gfx->_framebuffer, 0 ).info;
        gfx_shader::GlobalFrameData fdata;
        fdata.main_camera_index = camera_index;
        fdata.rtarget_size = vec2_t( (float)framebuffer_info.width, (float)framebuffer_info.height );
        fdata.rtarget_size_rcp = vec2_t( 1.f / (float)framebuffer_info.width, 1.f / (float)framebuffer_info.height );
        UpdateCBuffer( fctx->cmdq, gfx->_gpu_frame_data_buffer, &fdata );
    }

    array_span_t<gfx_shader::InstanceData> idata_array( sc.mesh_data[scene_index]->instance_data, num_meshes );
    {// transform buffer
        for( uint32_t i = 0; i < num_meshes; ++i )
        {
            const mat44_t& instance_matrix = matrix_array[i];
            const id_t mat_id = { idmat_array[i].i };

            const uint32_t instance_offset = AppendMatrix( transform_buffer, instance_matrix );
            gfx_shader::InstanceData& idata = idata_array[i];
            idata.offset = instance_offset;
            idata.camera_index = camera_index;
            idata.material_index = gfx->MaterialDataIndex( mat_id );
        }

        GFXSortKey sort_key;
        sort_key.layer = GFXEDrawLayer::GEOMETRY;
        sort_key.stage = GFXEDrawStage::UPLOAD_GLOBALS;

        RDIXTransformBufferCommands transform_buff_cmds = UploadAndSetTransformBuffer( cmdbuffer, nullptr, transform_buffer, bind_info );
        SubmitCommand( cmdbuffer, transform_buff_cmds.first, sort_key.key );
    }

    { //shadows
        RDIXCommandBuffer* shadow_cmdbuff = sc.sun_shadow[scene_index].cmd_buffer;
        for( uint32_t i = 0; i < num_meshes; ++i )
        {
            
        }

    }

    // color pass
    for( uint32_t i = 0; i < num_meshes; ++i )
    {
        const id_t mat_id = { idmat_array[i].i };
        const gfx_shader::InstanceData& idata = idata_array[i];
        
        RDIXCommandChain chain( cmdbuffer, nullptr );

        RDIXResourceBinding* binding = MaterialBinding( idmat_array[i] );
        RDIXPipeline* pipeline = gfx_internal::GetMaterialPipeline( gfx, mat_id);

        const uint32_t data_size = (uint32_t)sizeof( gfx_shader::InstanceData );
        chain.AppendCmdWithData<RDIXUpdateConstantBufferCmd>( data_size, GetInstanceOffsetCBuffer( transform_buffer ), &idata, data_size );
        chain.AppendCmd<RDIXSetPipelineCmd>( pipeline, false );
        chain.AppendCmd<RDIXSetResourcesCmd>( binding );
        
        if( RDIXDrawRenderSourceCmd* draw_cmd = chain.AppendCmd<RDIXDrawRenderSourceCmd>() )
        {
            const GFXMeshSkinningData& skinning_data = skinned_mesh_array[i];
            RDIXRenderSource* rsource = skinning_data.rsource;
            if( !rsource || skinning_data.pin_index == GFX_DEFAULT_SKINNING_PIN )
            {
                rsource = (RDIXRenderSource*)RSM::Get( idmesh_array[i] );
            }
            
            draw_cmd->rsource = rsource ? rsource : gfx->_fallback_mesh;
            draw_cmd->num_instances = 1;
        }

        GFXSortKey sort_key;
        sort_key.depth = 0;
        sort_key.layer = GFXEDrawLayer::GEOMETRY;
        sort_key.stage = GFXEDrawStage::DRAW;

        chain.Submit( sort_key.key );
    }
    
    if( sc.sky_data[scene_index].flag_enabled )
    {
        const GFXSceneContainer::SkyData& sky = sc.sky_data[scene_index];
        gfx_shader::LightningFrameData ldata;
        ldata.sun_L = -vec4_t( sky.params.sun_dir, 0.f );
        ldata.sun_color = vec4_t( 0.9f, 0.8f, 0.5f, 1.f );
        ldata.sun_intensity = sky.params.sun_intensity;
        ldata.sky_intensity = sky.params.sky_intensity;
        ldata.environment_map_width = sky.sky_texture.info.width;
        ldata.environment_map_max_mip = sky.sky_texture.info.mips - 1;

        {
            RDIXCommandChain chain( cmdbuffer, nullptr );
            const uint32_t data_size = (uint32_t)sizeof( ldata );
            chain.AppendCmdWithData<RDIXUpdateConstantBufferCmd>( data_size, gfx->_material.lighting_data_gpu, &ldata, data_size );
            chain.AppendCmd<RDIXSetConstantBufferCmd>( gfx->_material.lighting_data_gpu, LIGHTING_FRAME_DATA_SLOT, RDIEPipeline::PIXEL_MASK );
            chain.AppendCmd<RDIXSetResourceROCmd>( sky.sky_texture, LIGHTING_SKY_CUBEMAP_SLOT, RDIEPipeline::PIXEL_MASK );
            chain.AppendCmd<RDIXSetRenderTargetCmd>( gfx->_framebuffer );
            chain.AppendCmd<RDIXClearRenderTargetCmd>( gfx->_framebuffer, 0.f, 0.f, 0.f, 1.f, -1.f );
            chain.AppendCmd<RDIXSetPipelineCmd>( gfx->_material.pipeline.skybox, false );
            chain.AppendCmd<RDIXDrawCmd>( 6, 0 );

            GFXSortKey sort_key;
            sort_key.layer = GFXEDrawLayer::SKYBOX;
            sort_key.stage = GFXEDrawStage::DRAW;
            chain.Submit( sort_key.key );
        }
    }

    EndCommandBuffer( cmdbuffer );

}

void GFX::SubmitCommandBuffer( GFXFrameContext* fctx, GFXSceneID idscene )
{
    GFXSceneContainer& sc = gfx->_scene;
    if( !sc.IsSceneAlive( { idscene.i } ) )
        return;

    const uint32_t scene_index = make_id( idscene.i ).index;

    RDIXCommandBuffer* cmdbuffer = sc.command_buffer[scene_index];
    ::SubmitCommandBuffer( fctx->cmdq, cmdbuffer );
}

void GFX::PostProcess( GFXFrameContext* fctx, const GFXCameraID idcamera )
{
    GFXCameraContainer& cc = gfx->_camera;
    if( !cc.IsAlive( { idcamera.i } ) )
        return;

    const uint32_t camera_index = cc.DataIndex( { idcamera.i } );
    const GFXCameraParams& camerap = cc.params[camera_index];
    const GFXCameraMatrices& cameram = cc.matrices[camera_index];

    GFXPostProcess& pp = gfx->_postprocess;
    {// shadows
        gfx_shader::ShadowData sdata;
        sdata.camera_world = cameram.world; 
        sdata.camera_view_proj = cameram.proj_api * cameram.view;
        sdata.camera_view_proj_inv = inverse( sdata.camera_view_proj );

        RDITextureInfo tex_info = Texture( gfx->_framebuffer, 0 ).info;
        sdata.rtarget_size = vec2_t( (float)tex_info.width, (float)tex_info.height );
        sdata.rtarget_size_rcp = vec2_t( 1.f / sdata.rtarget_size.x, 1.f / sdata.rtarget_size.y );

        sdata.light_pos_ws = vec4_t( -gfx->_scene.sky_data[0].params.sun_dir * 10.f, 1.0f );

        RDITextureDepth depth_tex = TextureDepth( gfx->_framebuffer );
       

        UpdateCBuffer( fctx->cmdq, pp.shadow.cbuffer_fdata, &sdata );
        SetCbuffers( fctx->cmdq, &pp.shadow.cbuffer_fdata, SHADOW_DATA_SLOT, 1, RDIEPipeline::PIXEL_MASK );

        BindRenderTarget( fctx->cmdq, gfx->_framebuffer, { GFXEFramebuffer::SHADOW }, false );
        SetResourcesRO( fctx->cmdq, &depth_tex, 0, 1, RDIEPipeline::PIXEL_MASK );
        BindPipeline( fctx->cmdq, pp.shadow.pipeline_ss, false );
        
        Draw( fctx->cmdq, 6, 0 );


        RDITextureRW color_tex = Texture( gfx->_framebuffer, GFXEFramebuffer::COLOR );
        RDITextureRW shadow_tex = Texture( gfx->_framebuffer, GFXEFramebuffer::SHADOW );

        BindRenderTarget( fctx->cmdq, gfx->_framebuffer, { GFXEFramebuffer::COLOR_SWAP }, false );
        SetResourcesRO( fctx->cmdq, &shadow_tex, 0, 1, RDIEPipeline::PIXEL_MASK );
        SetResourcesRO( fctx->cmdq, &color_tex, 1, 1, RDIEPipeline::PIXEL_MASK );
        BindPipeline( fctx->cmdq, pp.shadow.pipeline_combine, false );

        Draw( fctx->cmdq, 6, 0 );

    }
}
