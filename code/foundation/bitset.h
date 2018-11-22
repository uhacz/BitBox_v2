#pragma once

#include "containers.h"
#include <intrin.h>

namespace bitset
{
#define BITSET_TEMPLATE_ARGS uint32_t SIZE
#define BITSET_T bitset_t<SIZE>

    namespace _internal
    {
        struct bit_address_t
        {
            uint32_t element;
            uint32_t bit;
            
            uint32_t mask() const { return 1 << bit; }
        };

        template< BITSET_TEMPLATE_ARGS >
        inline bit_address_t compute_bit_address( uint32_t index )
        {
            bit_address_t addr;
            addr.element = index >> BITSET_T::DIV_SHIFT;
            addr.bit = index & BITSET_T::MOD_MASK;
            return addr;
        }

        template< typename T >
        FORCE_INLINE uint8_t bitscan_forward( uint32_t* index, T mask ) { return 0; }

        template<>
        FORCE_INLINE uint8_t bitscan_forward<uint32_t>( uint32_t* index, uint32_t mask )
        {
            return _BitScanForward( (unsigned long*)index, mask );
        }

        template<>
        FORCE_INLINE uint8_t bitscan_forward<uint64_t>( uint32_t* index, uint64_t mask )
        {
            return _BitScanForward64( (unsigned long*)index, mask );
        }

        template< typename T >
        FORCE_INLINE uint32_t population( T mask ) { return 0; }

        template<>
        FORCE_INLINE uint32_t population( uint32_t mask ) { return __popcnt( mask ); }
        template<>
        FORCE_INLINE uint32_t population( uint64_t mask ) { return __popcnt64( mask ); }

    }

    template< BITSET_TEMPLATE_ARGS > void clear_all( BITSET_T& bs )
    {
        for( uint32_t i = 0; i < SIZE; ++i )
            bs.bits[i] = 0;
    }
    template< BITSET_TEMPLATE_ARGS > void clear( BITSET_T& bs, uint32_t index )
    {
        const _internal::bit_address_t bit_addr = _internal::compute_bit_address( index );
        bs.bits[bit_addr.element] &= ~(bit_addr.mask());
    }
    template< BITSET_TEMPLATE_ARGS > void set( BITSET_T& bs, uint32_t index )
    {
        const _internal::bit_address_t bit_addr = _internal::compute_bit_address( index );
        bs.bits[bit_addr.element] |= (bit_addr.mask());
    }
    template< BITSET_TEMPLATE_ARGS > uint32_t get( const BITSET_T& bs, uint32_t index )
    {
        const _internal::bit_address_t bit_addr = _internal::compute_bit_address( index );
        return bs.bits[bit_addr.element] & bit_addr.mask();
    }

    template< BITSET_TEMPLATE_ARGS > bool is_any_set( const BITSET_T& bs )
    {
        for( uint32_t i = 0; i < SIZE; ++i )
        {
            if( bs.bits[i] )
                return true;
        }
        return false;
    }

    template< BITSET_TEMPLATE_ARGS > uint32_t population( const BITSET_T& bs )
    {
        uint32_t result = 0;
        for( uint32_t i = 0; i < SIZE; ++i )
            result += _internal::population( bs.bits[i] );
    }


    template< BITSET_TEMPLATE_ARGS > uint32_t find_next_set( BITSET_T& bs, uint32_t begin )
    {
        uint32_t offset = begin;

    }

}