#pragma once

#include <rtti/rtti.h>
#include <entity/entity.h>
#include <foundation/string_util.h>


struct TestComponent : ENTIComponent
{
    RTTI_DECLARE_TYPE( TestComponent );

    virtual ~TestComponent();

    virtual void Initialize( ENTEntityID entity_id, ENTSystemInfo* sys ) override;
    virtual void Deinitialize( ENTEntityID entity_id, ENTSystemInfo* sys ) override;

    virtual void ParallelStep( ENTEntityID entity_id, ENTSystemInfo* sys, uint64_t dt_us );
    virtual void SerialStep( ENTEntityID entity_id, ENTIComponent* parent, ENTSystemInfo*, uint64_t dt_us );

    mat44_t _mesh_pose = mat44_t::identity();
    uint32_t _shape_type = 1;
    string_t _material_name;

};
