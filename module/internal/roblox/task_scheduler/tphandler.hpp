#pragma once
#include <cstdint>

namespace TPHandler
{
    void Tick();

    void Reset();

    bool IsInGame();
    uintptr_t GetBoundDataModel();
}
