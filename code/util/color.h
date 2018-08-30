#pragma once

#include <foundation/type.h>

struct color32_t
{
    uint32_t value;
    operator uint32_t() { return value; }

    static constexpr color32_t WHITE	() { return { 0xFFFFFFFF }; }
    static constexpr color32_t SILVER	() { return { 0xC0C0C0FF }; }
    static constexpr color32_t GRAY	    () { return { 0x808080FF }; }
    static constexpr color32_t BLACK	() { return { 0x000000FF }; }
    static constexpr color32_t RED	    () { return { 0xFF0000FF }; }
    static constexpr color32_t MAROON	() { return { 0x800000FF }; }
    static constexpr color32_t YELLOW	() { return { 0xFFFF00FF }; }
    static constexpr color32_t OLIVE	() { return { 0x808000FF }; }
    static constexpr color32_t LIME	    () { return { 0x00FF00FF }; }
    static constexpr color32_t GREEN	() { return { 0x008000FF }; }
    static constexpr color32_t AQUA	    () { return { 0x00FFFFFF }; }
    static constexpr color32_t TEAL	    () { return { 0x008080FF }; }
    static constexpr color32_t BLUE	    () { return { 0x0000FFFF }; }
    static constexpr color32_t NAVY	    () { return { 0x000080FF }; }
    static constexpr color32_t FUCHSIA  () { return { 0xFF00FFFF }; }
    static constexpr color32_t PURPLE	() { return { 0x800080FF }; }
};

