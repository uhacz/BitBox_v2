#pragma once

#include <foundation/type.h>

template< typename T, uint32_t INDEX_BITS >
union ECSID
{
    static constexpr uint32_t INDEX_BITS_VALUE = INDEX_BITS;
    T hash = 0;

    struct
    {
        T index : INDEX_BITS;
        T id : (sizeof( T ) * 8 - INDEX_BITS); // aka generation
    };

    operator T () { return hash; }
};

using ECSEntityID = ECSID<uint32_t, 13>;
using ECSComponentID = ECSID<uint32_t, 15>;
