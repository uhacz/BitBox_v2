#include <3rd_party/googletest/include/gtest/gtest.h>
#include <foundation/c_array.h>
#include <memory/memory.h>



using c_array_type = c_array_t<u32>;

TEST( c_array_t, reserve )
{
    c_array_type* arr = nullptr;
    c_array::reserve( arr, 10, BXDefaultAllocator() );

    EXPECT_EQ( arr->capacity, 10 );
    EXPECT_EQ( arr->size, 0 );

    c_array::destroy( arr );
}

TEST( c_array_t, resize )
{
    c_array_type* arr = nullptr;
    c_array::resize( arr, 10, BXDefaultAllocator(), (u32)6 );

    EXPECT_EQ( arr->capacity, 10 );
    EXPECT_EQ( arr->size, 10 );

    for( u32 i = 0; i < 10; ++i )
        EXPECT_EQ( arr->data[i], 6 );

    c_array::resize( arr, 15, BXDefaultAllocator(), (u32)9 );
    EXPECT_EQ( arr->size, 15 );
    
    for( u32 i = 0; i < 10; ++i )
        EXPECT_EQ( arr->data[i], 6 );

    for( u32 i = 10; i < 15; ++i )
        EXPECT_EQ( arr->data[i], 9 );

    c_array::destroy( arr );
}

TEST( c_array_t, push_back )
{
    c_array_type* arr = nullptr;
    c_array::reserve( arr, 5, BXDefaultAllocator() );

    for( u32 i = 0; i < 13; ++i )
        c_array::push_back( arr, i + 1 );

    for( u32 i = 0; i < 13; ++i )
        EXPECT_EQ( arr->data[i], i + 1 );

    for( u32 i = 0; i < 9; ++i )
        c_array::push_back( arr, (i+2) * 10 );

    for( u32 i = 0; i < 13; ++i )
        EXPECT_EQ( arr->data[i], i + 1 );

    for( u32 i = 0; i < 9; ++i )
        EXPECT_EQ( arr->data[i+13], (i + 2) * 10 );


    c_array::destroy( arr );
}