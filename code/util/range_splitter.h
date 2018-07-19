#pragma once

#include <foundation/type.h>
#include <foundation/debug.h>


struct RangeSplitter
{
private:
    RangeSplitter( uint32_t ne, uint32_t gs ): nElements( ne ), grabSize(gs), grabbedElements(0) {}
public:

    static inline RangeSplitter SplitByTask( uint32_t nElements, uint32_t nTasks )   { return RangeSplitter( nElements, 1 + ( (nElements-1) / nTasks ) ); }
    static inline RangeSplitter SplitByGrab( uint32_t nElements, uint32_t grabSize ) { return RangeSplitter( nElements, grabSize ); }

    inline uint32_t ElementsLeft() const { return nElements - grabbedElements; }
    inline uint32_t NextGrab()
    {
        const uint32_t eleft = ElementsLeft();
        const uint32_t grab = ( eleft < grabSize ) ? eleft : grabSize;
        grabbedElements += grab;
        SYS_ASSERT( grabbedElements <= nElements );
        return grab;
    }

    const uint32_t nElements;
    const uint32_t grabSize;
    uint32_t grabbedElements;
};
