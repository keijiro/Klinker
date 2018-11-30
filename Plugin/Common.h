#pragma once

#define NOMINMAX

#include "DeckLinkAPI_h.h"
#include <cassert>
#include <cstdint>

namespace klinker
{
    inline void ShouldOK(HRESULT result)
    {
        assert(result == S_OK);
    }
}
