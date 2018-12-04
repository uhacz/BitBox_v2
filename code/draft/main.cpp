#include <stdlib.h>
#include <foundation/type.h>
#include <foundation/thread/rw_spin_lock.h>
#include <future> 
#include <assert.h>
#include <random>
#include <windows.h>
#include <array>
#include <atomic>

enum State : uint32_t
{
    EMPTY,
    LOADING,
    READY,
};

struct BufferStateAtomic
{
    std::atomic_uint32_t num_requests{ 0 };
    std::atomic<State> state{ EMPTY };

    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution;

    BufferStateAtomic()
        : distribution( 100, 400 )
    {}

    static State AsyncFunction( BufferStateAtomic* _this )
    {
        //int wait = _this->distribution( _this->generator );
        //Sleep( wait );
        //Sleep( 1000 );
        printf( "=============================>Load complete\n" );

        {
            State expected_state = EMPTY;
            if( !std::atomic_compare_exchange_strong( &_this->state, &expected_state, READY ) )
            {
                assert( expected_state == EMPTY );
                assert( _this->num_requests == 0 );

                printf( "=============================>AsyncClear\n" );
                // clear function
            }
            else
            {
                assert( _this->num_requests > 0 );
            }
        }
        return _this->state;

    }

    void StartLoading( uint32_t thread_index )
    {
        uint32_t counter = ++num_requests;
        printf( "Load: %u (%u)\n", counter, thread_index );
        if( counter == 1 )
        {
            printf( "StartLoading: %u (%u)\n", counter, thread_index );
            std::async( std::launch::async, AsyncFunction, this );
        }

        //const uint32_t counter = ++num_requests;
        //printf( "Load: %u (%u)\n", counter, thread_index );
        //
        //if( counter == 1 )
        //{
        //    printf( "StartLoading: %u (%u)\n", counter, thread_index );
        //    assert( state == EMPTY );
        //    state = LOADING;
        //    std::async( std::launch::async, AsyncFunction, this );
        //}
    }

    void Unload( uint32_t thread_index )
    {
        assert( num_requests > 0 );

        uint32_t value = --num_requests;
        printf( "Unload: %u (%u)\n", value, thread_index );
        if( value == 0 )
        {
            State prev_state = std::atomic_exchange( &state, EMPTY );
            if( prev_state == READY )
            {
                printf( "=============================>Clear\n" );
                //clear buffer
            }
        }
    }
};

struct BufferStateLock
{
    uint32_t num_requests = 0;
    State state = EMPTY;

    rw_spin_lock_t lock;

    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution;

    BufferStateLock()
        : distribution( 100, 400 )
    {}

    static State AsyncFunction( BufferStateLock* _this )
    {
        //int wait = _this->distribution( _this->generator );
        //Sleep( wait );
        static int check = 0;
        assert( check == 0 );

        check = 1;

        printf( "=============================>Load complete\n" );

        check = 0;

        {
            scoped_write_spin_lock_t guard( _this->lock );
            if( _this->state == EMPTY )
            {
                assert( _this->num_requests == 0 );
                // clear function
                printf( "=============================>AsyncClear\n" );
            }
            else
            {
                assert( _this->state == LOADING );
                assert( _this->num_requests > 0 );
                _this->state = READY;
            }
        }

        return _this->state;

    }

    void StartLoading( uint32_t thread_index )
    {
        uint32_t counter = UINT32_MAX;
        {
            scoped_write_spin_lock_t guard( lock );
            counter = ++num_requests;
            if( counter == 1 )
            {
                assert( state == EMPTY );
                state = LOADING;
            }
        }

        printf( "Load: %u (%u)\n", counter, thread_index );

        if( counter == 1 )
        {
            printf( "StartLoading: %u\n", counter );
            std::async( AsyncFunction, this );
        }
    }

    void Unload( uint32_t thread_index )
    {
        uint32_t counter = UINT32_MAX;
        
        scoped_write_spin_lock_t guard( lock );
        assert( num_requests > 0 );
        counter = --num_requests;

        printf( "Unload: %u (%u)\n", num_requests, thread_index );

        if( counter == 0 )
        {
            if( state == READY )
            {
                //clear buffer
                printf( "=============================>Clear\n" );
            }
            state = EMPTY;
        }
    }

};

constexpr uint32_t N_THREADS = 8;
constexpr uint32_t N_ITERATIONS = 32;
template< typename T >
static void ThreadFunction( T* state, uint32_t thread_index )
{
    for( uint32_t i = 0; i < N_ITERATIONS; ++i )
    {
        state->StartLoading( thread_index );
        int wait = state->distribution( state->generator );
        //Sleep( wait );

        state->Unload( thread_index );
    }

    //for( uint32_t i = 0; i < N_ITERATIONS; ++i )
    //{
    //    int wait = state->distribution( state->generator );
    //    Sleep( wait );
    //    
    //}

}

template< typename T >
void Test()
{
    T buff_state;

    std::vector< std::thread> threads;
    threads.reserve( N_THREADS );
    for( uint32_t i = 0; i < N_THREADS; ++i )
    {
        threads.emplace_back( ThreadFunction<T>, &buff_state, i );
    }

    while( !threads.empty() )
    {
        threads.back().join();
        threads.pop_back();
    }

    assert( buff_state.num_requests == 0 );
    assert( buff_state.state == EMPTY );
}

int main()
{
    Test<BufferStateLock>();
    system( "PAUSE" );
    return 0;
}