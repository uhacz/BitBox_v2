#include "node_comp.h"
#include "node.h"

#include "../foundation/debug.h"
#include "../foundation/containers.h"
#include "../foundation/static_array.h"
#include "../foundation/hashmap.h"
#include "../foundation/string_util.h"
#include "../memory/pool_allocator.h"

#include "../foundation/thread/mutex.h"
#include "../foundation/thread/rw_spin_lock.h"

using NODECompAllocator = DynamicPoolAllocator;

struct NODECompTypeDesc
{
    const NODEComp__TypeInfo* info;
    u32 index;
};
struct NODECompTypeRegistry
{
    static constexpr u32 MAX_TYPES = 128;

    static_array_t<NODECompTypeDesc, MAX_TYPES> type_desc;
    static_array_t<NODECompAllocator, MAX_TYPES> type_allocator;
    static_array_t<rw_spin_lock_t, MAX_TYPES> type_lock;
    hash_t<const NODECompTypeDesc*> hash_code_map;

    mutex_t lock;
};
NODECompTypeRegistry* TypeReg()
{
    static NODECompTypeRegistry __reg;
    return &__reg;
}

void NODECompTypeRegistryShutDown()
{
    NODECompTypeRegistry* reg = TypeReg();
    for( NODECompAllocator& allocator : reg->type_allocator )
    {
        NODECompAllocator::Destroy( &allocator );
    }
}

void NODECompRegisterType( const NODEComp__TypeInfo* tinfo )
{
    NODECompTypeRegistry* reg = TypeReg();
    reg->lock.lock();
    
    const u32 type_index = reg->type_desc.size;
    NODECompTypeDesc& desc = array::emplace_back( reg->type_desc );
    NODECompAllocator& allocator = array::emplace_back( reg->type_allocator );
    
    SYS_ASSERT( reg->type_allocator.size == reg->type_desc.size );
    SYS_ASSERT( hash::has( reg->hash_code_map, tinfo->type_hash_code ) == false );
    
    hash::set( reg->hash_code_map, tinfo->type_hash_code, (const NODECompTypeDesc*)&desc );
    
    reg->lock.unlock();

    
    desc.info = tinfo;
    desc.index = type_index;
    NODECompAllocator::Create( &allocator, BXDefaultAllocator(), tinfo->size, tinfo->alignment, tinfo->num_chunks );
}

const NODECompTypeDesc* FindType( const char* name )
{
    NODECompTypeRegistry* reg = TypeReg();
    for( const NODECompTypeDesc& desc : reg->type_desc )
    {
        if( string::equal( desc.info->name, name ) )
        {
            return &desc;
        }
    }

    SYS_LOG_ERROR( "NODEComp type '%s' not found!", name );
    return nullptr;
}

const NODECompTypeDesc* FindType( u64 hash_code )
{
    NODECompTypeRegistry* reg = TypeReg();
    const NODECompTypeDesc* null_info = nullptr;
    const NODECompTypeDesc* desc = hash::get( reg->hash_code_map, hash_code, null_info );

    if( !desc )
    {
        SYS_LOG_ERROR( "NODEComp type '%u' not found!", hash_code );
    }

    return desc;
}

NODEComp* NODECompAlloc( const NODECompTypeDesc* desc )
{
    if( !desc )
    {
        return nullptr;
    }
    
    NODECompTypeRegistry* reg = TypeReg();
    NODECompAllocator* allocator = &reg->type_allocator[desc->index];

    reg->type_lock[desc->index].lock_write();
    void* address = BX_MALLOC( allocator, desc->info->size, desc->info->alignment );
    reg->type_lock[desc->index].unlock_write();

    NODEComp* comp = desc->info->constructor( address );
    return comp;
}

NODEComp* NODECompAlloc( const char* type_name )
{
    return NODECompAlloc( FindType( type_name ) );
}
NODEComp* NODECompAlloc( u64 type_hash_code )
{
    return NODECompAlloc( FindType( type_hash_code ) );
}

void NODECompFree( const NODECompTypeDesc* desc, NODEComp* comp )
{
    SYS_ASSERT( desc != nullptr );

    desc->info->destructor( comp );

    NODECompTypeRegistry* reg = TypeReg();
    NODECompAllocator* allocator = &reg->type_allocator[desc->index];
    reg->type_lock[desc->index].lock_write();
    BX_FREE( allocator, comp );
    reg->type_lock[desc->index].unlock_write();
}

void NODECompFree( NODEComp* comp )
{
    if( !comp )
        return;

    const u64 type_hash_code = comp->TypeHashCode();
    NODECompFree( FindType( type_hash_code ), comp );
}