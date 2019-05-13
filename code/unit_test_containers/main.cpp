#include <3rd_party/googletest/include/gtest/gtest.h>
#include <stdlib.h>
#include <memory/memory_plugin.h>

int main( int argc, char **argv ) 
{
    BXMemoryStartUp();

    ::testing::InitGoogleTest( &argc, argv );
    int ret = RUN_ALL_TESTS();

    system( "PAUSE" );

    BXMemoryShutDown();
    return ret;
}