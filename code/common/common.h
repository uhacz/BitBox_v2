#pragma once

#include <foundation/string_util.h>
#include <entity/entity_system.h>

struct BXIAllocator;
struct BXIFilesystem;

namespace common
{
    void RefreshFiles( string_buffer_t* flist, const char* folder, BXIFilesystem* fs, BXIAllocator* allocator );
    string_buffer_it MenuFileSelector( const string_buffer_t& file_list );

    template< typename T >
    inline void CreateComponentIfNotExists( ECSComponentID* cid, T** ptr, ECS* ecs, ECSEntityID entity )
    {
        ECSComponentID comp_id = Lookup<T>( ecs, entity );
        if( comp_id == ECSComponentID::Null() )
        {
            ECSNewComponent new_comp = CreateComponent<T>( ecs );
            cid[0] = new_comp.id;
            ptr[0] = new_comp.Cast<T>();

            ecs->Link( entity, &new_comp.id, 1 );
        }
        else
        {
            cid[0] = comp_id;
            ptr[0] = Component<T>( ecs, comp_id );
        }
    }
}

