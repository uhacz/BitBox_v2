#include "time.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

static inline i64 herz()
{
    LARGE_INTEGER tf;
    QueryPerformanceFrequency( &tf );
    return tf.QuadPart;
}

u64 BXTime::GlobalTimeMS()
{
    static u64 clockPerSec = herz();
    LARGE_INTEGER ct;
    QueryPerformanceCounter( &ct );

    return unsigned( ct.QuadPart / ( clockPerSec / 1000 ) );
}

u64 BXTime::GlobalTimeUS()
{
    static u64 clockPerSec = herz();
    static double clockPerUSrcp = 1000000.0 / clockPerSec;
    LARGE_INTEGER ct;
    QueryPerformanceCounter( &ct );

    return (u64)( ct.QuadPart * clockPerUSrcp );
}

void BXTime::Sleep( u64 ms )
{
    ::Sleep( (DWORD)ms );
}

BXTimeQuery BXTimeQuery::Begin()
{
    BXTimeQuery tq;
    LARGE_INTEGER ct;
    QueryPerformanceCounter( &ct );
    tq.tick_start = ct.QuadPart;
    return tq;
}

void BXTimeQuery::End( BXTimeQuery* tq )
{
    LARGE_INTEGER ct;
    BOOL bres = QueryPerformanceCounter( &ct );
    tq->tick_stop = ct.QuadPart;

    LARGE_INTEGER tf;
    bres = QueryPerformanceFrequency( &tf );

    const u64 numTicks = ( (u64)( tq->tick_stop - tq->tick_start ) );
    const double durationUS = double( numTicks * 1000000 ) / (double)tf.QuadPart;
    tq->duration_US = (u64)durationUS;
}
