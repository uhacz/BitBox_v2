#pragma once

#include "../entity_system.h"
#include <foundation/string_util.h>

struct CMPName
{
    ECS_NON_POD_COMPONENT( CMPName );
    string_t value;


    static ECSComponentID FindComponent( ECS* ecs, const char* name );
};

