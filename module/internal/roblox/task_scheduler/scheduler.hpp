#pragma once

#include <windows.h>
#include <lstate.h>
#include <lualib.h>
#include <string>
#include <vector>
#include <mutex>

namespace TaskScheduler
{
    struct UserdataOverlay
    {
        int identity;
        char pad[0x20 - sizeof(int)];
        std::uintptr_t capabilities;
    };

    bool SetupExploit();
    void RequestExecution(std::string Script);
    void SetProtoCapabilities(Proto* Proto, uintptr_t* Capabilities);
    void SetIdentity(lua_State* L, int Level, uintptr_t Capabilities);
    void SetThreadCapabilities(lua_State* L, int Level, uintptr_t Capabilities);
    uintptr_t GetDataModel();
    uintptr_t GetScriptContext(uintptr_t DataModel);
    lua_State* GetLuaStateForInstance(uintptr_t Instance);

    void ProcessExecutionQueue();
}