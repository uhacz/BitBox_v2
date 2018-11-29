#include <3rd_party/googletest/include/gtest/gtest.h>
#include <foundation/containers.h>
#include <foundation/bitset.h>
#include <stdlib.h>

#include <random>

TEST( bitset_t, set_first )
{
    bitset_t<10> bs;
    bitset::set( bs, 0 );
    EXPECT_EQ( bitset::get( bs, 0 ), 1 );
}

TEST( bitset_t, set_last )
{
    bitset_t<65> bs;
    bitset::set( bs, 64 );
    EXPECT_EQ( bitset::get( bs, 64 ), 1 );
}

namespace
{
    template< typename Tbitset >
    uint32_t FillBitsetRandom( Tbitset& bs, uint8_t* values, std::default_random_engine& generator, std::uniform_int_distribution<uint32_t>& distribution )
    {
        const uint32_t num_rolls = distribution( generator );
        for( uint32_t i = 0; i < num_rolls; ++i )
        {
            uint32_t index = distribution( generator );

            bitset::set( bs, index );
            values[index] = 1;
        }

        return num_rolls;
    }
}

static constexpr uint32_t MIN_SIZE = 1 << 4;
static constexpr uint32_t MAX_SIZE = 1 << 10;

TEST( bitset_t, set_random )
{
    std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> size_distr( MIN_SIZE, MAX_SIZE-1 );

    bitset_t<MAX_SIZE> bs;
    uint8_t values[MAX_SIZE] = {};

    FillBitsetRandom( bs, values, generator, size_distr );
    for( uint32_t i = 0; i < MAX_SIZE; ++i )
    {
        EXPECT_EQ( values[i], bitset::get( bs, i ) );
    }
}

TEST( bitset_t, population )
{
    std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> size_distr( MIN_SIZE, MAX_SIZE-1 );

    bitset_t<MAX_SIZE> bs;
    uint8_t values[MAX_SIZE] = {};

    FillBitsetRandom( bs, values, generator, size_distr );

    uint32_t expected_population = 0;
    for( uint32_t i = 0; i < MAX_SIZE; ++i )
        expected_population += values[i];

    EXPECT_EQ( expected_population, bitset::population( bs ) );
}

TEST( bitset_t, find_next_set )
{
    std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> size_distr( MIN_SIZE, MAX_SIZE-1 );

    bitset_t<MAX_SIZE> bs;
    uint8_t values[MAX_SIZE] = {};

    FillBitsetRandom( bs, values, generator, size_distr );

    uint32_t index = bitset::find_next_set( bs, 0 );
    while( index != bs.NUM_BITS )
    {
        EXPECT_TRUE( bitset::get( bs, index ) != 0 );
        index = bitset::find_next_set( bs, index + 1 );
    }
}

TEST( bitset_t, iterator )
{
    std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> size_distr( MIN_SIZE, MAX_SIZE-1 );

    bitset_t<MAX_SIZE> bs;
    uint8_t values[MAX_SIZE] = {};

    FillBitsetRandom( bs, values, generator, size_distr );

    bitset::const_iterator<bitset_t<MAX_SIZE>> it( bs );
    while( it.ok() )
    {
        EXPECT_TRUE( it.get() );
        it.next();
    }
}

TEST( bitset_t, iterator_empty )
{
    bitset_t<MAX_SIZE> bs;
    const uint32_t next_set = bitset::find_next_set( bs, 0 );

    EXPECT_TRUE( bitset::empty( bs ) );
    EXPECT_EQ( bitset::population( bs ), 0 );
    EXPECT_EQ( next_set, bs.NUM_BITS );
}
TEST( bitset_t, iterator_full )
{
    bitset_t<MAX_SIZE> bs;
    bitset::set_all( bs );

    EXPECT_TRUE( bitset::is_any_set( bs ) );
    EXPECT_EQ( bitset::population( bs ), bs.NUM_BITS );

    uint32_t counter = 0;
    bitset::const_iterator<bitset_t<MAX_SIZE>> it( bs );
    while( it.ok() )
    {
        EXPECT_TRUE( it.get() );
        ++counter;
        it.next();
    }

    EXPECT_EQ( counter, bs.NUM_BITS );
}

int main( int argc, char **argv ) 
{
    ::testing::InitGoogleTest( &argc, argv );
    int ret = RUN_ALL_TESTS();

    system( "PAUSE" );
    return ret;
}