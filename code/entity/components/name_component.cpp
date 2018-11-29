#include "name_component.h"

ECSComponentID CMPName::FindComponent( ECS* ecs, const char* name )
{
    const array_span_t<CMPName*> names = Components<CMPName>( ecs );
    const uint32_t n = names.size();
    for( uint32_t i = 0; i < n; ++i )
    {
        const CMPName* comp = names[i];
        if( string::equal( comp->value.c_str(), name ) )
        {
            return ecs->Lookup( comp );
        }
    }
    return ECSComponentID::Null();
}
