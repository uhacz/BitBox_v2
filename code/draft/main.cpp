#include <stdlib.h>
#include <foundation/type.h>
#include <foundation/thread/rw_spin_lock.h>
#include <future> 
#include <assert.h>
#include <random>
#include <windows.h>
#include <array>
#include <atomic>

struct unique_id_t
{
    uint64_t opaque = 0;
    union
    {
        uint64_t user_id : 54;
        uint64_t sys_id : 10;
    };
};

struct unique_id_range_t
{
    uint64_t begin;
    uint64_t end;
    uint64_t current;
    
};

int main()
{
    system( "PAUSE" );
    return 0;
}