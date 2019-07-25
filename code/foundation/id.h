#pragma once

#include "type.h"

template< typename Tbase, u32 Ibits = 16 >
struct ID
{
    using id_type = ID<Tbase, Ibits>;
    using base_type = Tbase;

    static constexpr Tbase NULL_ID = 0;
    static constexpr Tbase BASE_BITS = sizeof( Tbase ) * 8;
    static constexpr Tbase NUM_IBITS = Ibits;
    //SYS_STATIC_ASSERT( NUM_IBITS < BASE_BITS );
    union
    {
        Tbase hash;
        struct
        {
            Tbase index : NUM_IBITS;
            Tbase generation : BASE_BITS - NUM_IBITS;
        };
    };

    ID() = default;
    explicit ID( base_type h )
        : hash( h ) {}

    bool operator == ( const base_type other ) const { return hash == other; }
    bool operator <  ( const base_type other ) const { return hash < other; }
    bool operator >  ( const base_type other ) const { return hash > other; }
    bool operator == ( const id_type other ) const { return hash == other.hash; }
    bool operator <  ( const id_type other ) const { return hash < other.hash; }
    bool operator >  ( const id_type other ) const { return hash > other.hash; }

    bool IsNull() const { return hash == 0; }
};

template< typename Tid, typename Tbase = Tid::base_type >
inline Tbase ToHash( const Tid id ) { return id.hash; }

template< typename Tid, typename Tbase = Tid::base_type  >
inline Tid ToID( const Tbase hash )
{
    Tid id;
    id.hash = hash;
    return id;
}



#define BX_DEFINE_ID( name, base_type, index_bits ) \
    struct name : ID<base_type, index_bits> \
    { \
        using base_struct = ID<base_type, index_bits>; \
        name() {} \
        explicit name( base_type h ) : base_struct( h ) {} \
        \
        static name Null() { return name( 0 ); } \
    }