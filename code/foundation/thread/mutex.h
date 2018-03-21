#pragma once

#include <mutex>

using mutex_t = std::mutex;


template< typename T >
struct scope_lock_t
{
    scope_lock_t( T& lock )
        :_lock(lock)
    {
        _lock.lock();
    }
    ~scope_lock_t()
    {
        _lock.unlock();
    }

    T& _lock;
};

using scope_mutex_t = scope_lock_t<mutex_t>;