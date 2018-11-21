#pragma once

#define NOMINMAX

#include "DeckLinkAPI_h.h"
#include <cassert>
#include <cstdint>

namespace klinker
{
    // Assert function for COM operations
    inline void AssertSuccess(HRESULT result)
    {
        assert(SUCCEEDED(result));
    }
}
