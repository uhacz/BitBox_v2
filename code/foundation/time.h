#pragma once

#include "type.h"

struct BXTime
{
    static uint64_t GlobalTimeMS();
    static uint64_t GlobalTimeUS();

    static void Sleep( uint64_t ms );

    static inline double ToSeconds( uint64_t ms )
    {
        return double( ms ) * 0.000001;
    }
};

struct BXTimeQuery
{
    uint64_t tick_start = 0;
    uint64_t tick_stop = 0;
    uint64_t duration_US = 0;

    static BXTimeQuery Begin();
    static void End( BXTimeQuery* tq );

    bool IsRunning() const { return tick_start != 0; }

    inline uint64_t DurationMS() const
    {
        return duration_US / 1000;
    }

    inline double DurationS()
    {
        return BXTime::ToSeconds( duration_US );
    }
};


