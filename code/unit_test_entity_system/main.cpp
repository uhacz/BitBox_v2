#include <stdlib.h>
#include <3rd_party/googletest/include/gtest/gtest.h>

#include <memory/memory_plugin.h>
#include <memory/memory.h>

#include <foundation/string_util.h>
#include <entity/entity_system.h>

struct TestCompA
{
    int32_t signed_int;
    uint32_t unsigned_int;
    float floating_point;
};

struct TestCompB
{
    ECS_NON_POD_COMPONENT( TestCompB );
    string_t str;
};

class ECSTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        BXMemoryStartUp();

        BXIAllocator* allocator = BXDefaultAllocator();
        _ecs = ECS::StartUp( allocator );

        RegisterComponent<TestCompA>( _ecs, "CompA" );
        RegisterComponentNoPOD<TestCompB>( _ecs, "CompB" );

        CreateComponents();
    }

    void TearDown() override
    {
        DestroyComponents();
        _ecs->Update();

        ECS::ShutDown( &_ecs );

        BXMemoryShutDown();
    }

public:
    static constexpr uint32_t N_COMP = 100;

    ECS* _ecs = nullptr;

    std::vector<ECSComponentID> compA_id;
    std::vector<ECSComponentID> compB_id;

    void CreateComponents()
    {
        for( uint32_t i = 0; i < N_COMP; ++i )
        {
            compA_id.push_back( CreateComponent<TestCompA>( _ecs ) );
            compB_id.push_back( CreateComponent<TestCompB>( _ecs ) );
        }
    }
    void DestroyComponents()
    {
        for( uint32_t i = 0; i < N_COMP; ++i )
        {
            _ecs->MarkForDestroy( compA_id[i] );
            _ecs->MarkForDestroy( compB_id[i] );
        }
    }
};

TEST_F( ECSTest, CreateEntity )
{
    ECSEntityID id = _ecs->CreateEntity();
    EXPECT_TRUE( _ecs->IsAlive( id ) );

    _ecs->MarkForDestroy( id );
    EXPECT_FALSE( _ecs->IsAlive( id ) );

    _ecs->Update();
}

TEST_F( ECSTest, CreateComponent )
{
    ECSEntityID id1 = _ecs->CreateEntity();
    ECSEntityID id2 = _ecs->CreateEntity();

    for( uint32_t i = 0; i < N_COMP; ++i )
    {
        EXPECT_TRUE( _ecs->IsAlive( compA_id[i] ) );
        EXPECT_TRUE( _ecs->IsAlive( compB_id[i] ) );
    }
}

TEST_F( ECSTest, LinkComponent )
{
    ECSEntityID id1 = _ecs->CreateEntity();
    ECSEntityID id2 = _ecs->CreateEntity();

    // Owner
    _ecs->Link( id1, &compA_id[0], N_COMP / 2 );
    _ecs->Link( id2, &compB_id[0], N_COMP / 2 );

    _ecs->Link( id1, &compB_id[N_COMP / 2], N_COMP );
    _ecs->Link( id2, &compA_id[N_COMP / 2], N_COMP );

    for( uint32_t i = 0; i < N_COMP / 2; ++i )
    {
        ECSEntityID ownerA = _ecs->Owner( compA_id[i] );
        ECSEntityID ownerB = _ecs->Owner( compB_id[i] );

        EXPECT_TRUE( ownerA == id1 );
        EXPECT_TRUE( ownerB == id2 );
    }
    for( uint32_t i = N_COMP / 2; i < N_COMP; ++i )
    {
        ECSEntityID ownerA = _ecs->Owner( compA_id[i] );
        ECSEntityID ownerB = _ecs->Owner( compB_id[i] );

        EXPECT_TRUE( ownerA == id2 );
        EXPECT_TRUE( ownerB == id1 );
    }

    ECSComponentIDSpan compA_span = _ecs->Components( id1 );
    for( uint32_t i = 0; i < N_COMP / 2; ++i )
    {
        auto found_it = std::find( compA_span.begin(), compA_span.end(), compA_id[i] );
        EXPECT_TRUE( *found_it == compA_id[i] );
    }
    for( uint32_t i = N_COMP / 2; i < N_COMP; ++i )
    {
        auto found_it = std::find( compA_span.begin(), compA_span.end(), compB_id[i] );
        EXPECT_TRUE( *found_it == compB_id[i] );
    }

    _ecs->Unlink( &compA_id[0], (uint32_t)compA_id.size() );

    compA_span = _ecs->Components( id1 );
    ECSComponentIDSpan compB_span = _ecs->Components( id2 );
    for( uint32_t i = 0; i < N_COMP / 2; ++i )
    {
        auto found_it = std::find( compA_span.begin(), compA_span.end(), compA_id[i] );
        EXPECT_TRUE( found_it == compA_span.end() );
    }
    for( uint32_t i = N_COMP / 2; i < N_COMP; ++i )
    {
        auto found_it = std::find( compB_span.begin(), compB_span.end(), compA_id[i] );
        EXPECT_TRUE( found_it == compB_span.end() );
    }
}

TEST_F( ECSTest, LookupComponent )
{
    for( uint32_t i = 0; i < N_COMP; ++i )
    {
        ECSRawComponent* pointer = _ecs->Component( compA_id[i] );
        ECSComponentID idA = _ecs->Lookup( pointer );
        EXPECT_TRUE( idA == compA_id[i] );
    }
}

TEST_F( ECSTest, ComponentsOfType )
{
    array_span_t<TestCompA*> spanA = Components<TestCompA>( _ecs );
    array_span_t<TestCompB*> spanB = Components<TestCompB>( _ecs );
    
    for( TestCompA* a : spanA )
    {
        ECSComponentID id = _ecs->Lookup( a );
        auto found_it = std::find( compA_id.begin(), compA_id.end(), id );
        EXPECT_TRUE( *found_it == id );
    }

    for( TestCompB* b : spanB )
    {
        ECSComponentID id = _ecs->Lookup( b );
        auto found_it = std::find( compB_id.begin(), compB_id.end(), id );
        EXPECT_TRUE( *found_it == id );
    }

}
int main( int argc, char **argv )
{
    ::testing::InitGoogleTest( &argc, argv );
    int ret = RUN_ALL_TESTS();

    system( "PAUSE" );
    return ret;
}