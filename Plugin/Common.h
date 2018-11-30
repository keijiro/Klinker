#pragma once

#define NOMINMAX

#include "DeckLinkAPI_h.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

namespace klinker
{
    inline void ShouldOK(HRESULT result)
    {
        assert(result == S_OK);
    }

    inline void DebugLog(const char* message)
    {
        #if defined(_DEBUG)

        static int count = 0;

        if (count == 0)
        {
            AllocConsole();
            FILE* pConsole;
            freopen_s(&pConsole, "CONOUT$", "wb", stdout);
        }

        std::printf("Klinker (%04d): %s\n", count++, message);

        #endif
    }
}
