#pragma once
#include <cstdint>
#include <Windows.h>
#include <string>
#include <vector>

template <typename T>
inline T rebase_hyp(uintptr_t rva) {
    return rva != NULL ? (T)(rva + reinterpret_cast<uintptr_t>(GetModuleHandleA("RobloxPlayerBeta.dll"))) : (T)(NULL);
};

__forceinline bool CheckMemory(uintptr_t address) {
    if (address < 0x10000 || address > 0x7FFFFFFFFFFF) {
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) == 0) {
        return false;
    }

    if (mbi.Protect & PAGE_NOACCESS || mbi.State != MEM_COMMIT) {
        return false;
    }

    return true;
}

template<typename T>
__forceinline static bool is_ptr_valid(T* tValue) {
    const auto ptr = reinterpret_cast<const void*>(const_cast<const T*>(tValue));
    auto buffer = MEMORY_BASIC_INFORMATION{};

    if (const auto read = VirtualQuery(ptr, &buffer, sizeof(buffer)); read != 0 && sizeof(buffer) != read) {
    }
    else if (read == 0) {
        return false;
    }

    if (buffer.RegionSize < sizeof(T)) {
        return false;
    }

    if (buffer.State & MEM_FREE == MEM_FREE) {
        return false;
    }

    auto valid_prot = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

    if (buffer.Protect & valid_prot) {
        return true;
    }
    if (buffer.Protect & (PAGE_GUARD | PAGE_NOACCESS)) {
        return false;
    }

    return true;
}

namespace rbx::hyperion {
    static void add_to_cfg(void* address) {
        if (address == nullptr)
            return;

        const auto Current = *reinterpret_cast<uint8_t*>(*rebase_hyp<uintptr_t*>(0x41b840) + ((uintptr_t)address >> 0x13));
        if (Current != 0xFF)
        {
            *reinterpret_cast<uint8_t*>(*rebase_hyp<uintptr_t*>(0x41b840) + ((uintptr_t)address >> 0x13)) = 0xFF;
        }
    }
}