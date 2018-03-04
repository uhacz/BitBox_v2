#pragma once

struct guid_t
{
    unsigned char data[16];
};

guid_t GenerateGUID();