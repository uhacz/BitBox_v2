#pragma once

#include "type.h"

struct BXTime
{
    static u64 GlobalTimeMS();
    static u64 GlobalTimeUS();

    static void Sleep( u64 ms );

    static inline double ToSeconds( u64 ms )
    {
        return double( ms ) * 0.000001;
    }
};

struct BXTimeQuery
{
    u64 tick_start = 0;
    u64 tick_stop = 0;
    u64 duration_US = 0;

    static BXTimeQuery Begin();
    static void End( BXTimeQuery* tq );

    bool IsRunning() const { return tick_start != 0; }

    inline u64 DurationMS() const
    {
        return duration_US / 1000;
    }

    inline double DurationS()
    {
        return BXTime::ToSeconds( duration_US );
    }
};


