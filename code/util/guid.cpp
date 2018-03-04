#include "guid.h"

#include <combaseapi.h>

guid_t GenerateGUID()
{
    union
    {
        guid_t my;
        GUID os;
    };
    CoCreateGuid( &os );
    return my;
}
