#pragma once
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>

namespace AdvancedGuard {
    uintptr_t ModuleBase = 0;
    uintptr_t ModuleEnd = 0;

    LONG WINAPI AdvancedHandler(PEXCEPTION_POINTERS ExceptionInfo) {
        PEXCEPTION_RECORD Rec = ExceptionInfo->ExceptionRecord;
        PCONTEXT Context = ExceptionInfo->ContextRecord;

        if (Rec->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
            return EXCEPTION_CONTINUE_SEARCH;

        uintptr_t FaultAddress = (uintptr_t)Rec->ExceptionAddress;
        bool isOurCrash = (FaultAddress >= ModuleBase && FaultAddress <= ModuleEnd);

        if (isOurCrash) {
            uintptr_t TargetAddr = Rec->ExceptionInformation[1];
            int Mode = (int)Rec->ExceptionInformation[0]; // 0 = Read, 1 = Write

            char errorBuffer[512];
            sprintf_s(errorBuffer,
                "Reason: VIOLATION\n\n"
                "Operation: %s\n"
                "Exception at: 0x%p\n"
                "Attempted to %s: 0x%p\n\n"
                "The instruction will be skipped to prevent a crash.",
                (Mode == 0 ? "READ VIOLATION" : "WRITE VIOLATION"),
                (void*)FaultAddress,
                (Mode == 0 ? "read" : "write to"),
                (void*)TargetAddr);

            MessageBoxA(NULL, errorBuffer, "Roblox", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);

#ifdef _M_X64
            Context->Rip += 3;
#else
            Context->Eip += 3;
#endif
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }

    inline void Init() {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPCVOID)Init, &mbi, sizeof(mbi))) {
            ModuleBase = (uintptr_t)mbi.AllocationBase;
            ModuleEnd = ModuleBase + 0x500000;
        }

        AddVectoredExceptionHandler(1, AdvancedHandler);
    }
}