#pragma once
#include "entity_id.h"

struct ECS;

struct ECSEntityProxy
{
    ECSEntityProxy( ECS* ecs, ECSEntityID id )
        : _ecs( ecs ), _id( id ) {}

    template< typename T>
    inline ECSComponentID Lookup()
    {
        return _ecs->Lookup( _id, typeid(T).hash_code() );
    }

    ECSComponentIDSpan Components()
    {
        return _ecs->Components( _id );
    }

    ECSEntityID Id() const { return _id; }
    ECS* System() { return _ecs; }


private:
    ECS* _ecs;
    ECSEntityID _id;
};




template< typename TComp >
struct ECSComponentProxy
{
    void Release()
    {
        _ecs->MarkForDestroy( _id );
        _comp = nullptr;
        _id = ECSComponentID::Null();
    }

    bool Reset( ECSComponentID id )
    {
        Release();
        Swap( id );

        return _comp != nullptr;
    }

    void SetOwner( ECSEntityID eid )
    {
        _ecs->Link( eid, &_id, 1 );
    }

    ECSEntityID Owner() const { return _ecs->Owner( _id ); }
    ECSEntityProxy OwnerProxy() const { return ECSEntityProxy( _ecs, Owner() ); }
    ECSComponentID Id() const { return _id; }
    
    ECS* System() { return _ecs; }

          TComp* Get() { return _comp; }
    const TComp* Get() const { return _comp; }

          TComp* operator -> () { return _comp; }
    const TComp* operator -> () const { return _comp; }

    operator bool() const { return _comp != nullptr; }
    
    ECSComponentProxy( ECS* ecs, ECSComponentID id )
        : _ecs( ecs ), _id( ECSComponentID::Null() )
    {
        Swap( id );
    }

    ECSComponentProxy( ECSEntityProxy eproxy )
    {
        _ecs = eproxy.System();
        Swap( eproxy.Lookup<TComp>() );
    }

    static ECSComponentProxy<TComp> New( ECS* ecs )
    {
        const auto result = CreateComponent<TComp>( ecs );
        return ECSComponentProxy<TComp>( ecs, result.id, result.pointer );
    }

private:
    ECSComponentProxy( ECS* ecs, ECSComponentID id, ECSRawComponent* pointer )
        : _ecs( ecs ), _id( id ), _comp( (TComp*)pointer )
    {}

private:
    void Swap( ECSComponentID id )
    {
        _id = id;
        _comp = (TComp*)_ecs->ComponentSafe( id, typeid(TComp).hash_code() );
    }

private:
    ECS* _ecs;
    TComp* _comp;
    ECSComponentID _id;

    friend struct ECSComponentIterator;
};


struct ECSComponentIterator
{
    ECSComponentIterator( ECS* ecs, ECSEntityID eid )
        : _ecs( ecs ), _id( eid ), _index( 0 )
    {}

    template<typename TComp>
    ECSComponentProxy<TComp> FindNext()
    {
        const size_t type_hash = typeid(TComp).hash_code();
        ECSComponentIDSpan span = _ecs->Components( _id );
        for( ; _index < span.size(); ++_index )
        {
            if( ECSRawComponent* pointer = _ecs->ComponentSafe( span[_index], type_hash ) )
            {
                auto proxy = ECSComponentProxy<TComp>( _ecs, span[_index], pointer );
                ++_index;
                return proxy;
            }
        }

        _index = UINT32_MAX;
        return ECSComponentProxy<TComp>( _ecs, ECSComponentID::Null(), nullptr );
    }

    bool IsValid() const { return _index != UINT32_MAX; }
    void Reset() { _index = 0; }

private:
    ECS* _ecs;
    ECSEntityID _id;
    uint32_t _index;
};