#pragma once

#include <Windows.h>
#include <atomic>
#include <string>
#include <memory>

struct lua_State;

#define REBASE(Address) (Address + reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)))

struct DebuggerResult
{
    std::int32_t Result;
    std::int32_t Unk[4];
};

struct WeakThreadRef
{
    std::atomic<std::int32_t> Refs;
    lua_State* L;
    std::int32_t ThreadRef;
    std::int32_t ObjectId;
    std::int32_t Unk1;
    std::int32_t Unk2;

    WeakThreadRef(lua_State* L) : Refs(0), L(L), ThreadRef(0), ObjectId(0), Unk1(0), Unk2(0) {}
};

namespace Offsets
{
    const uintptr_t Print = REBASE(0x1737F90);
    const uintptr_t TaskDefer = REBASE(0x171E4B0);
    const uintptr_t RawScheduler = REBASE(0x8057E48);
    const uintptr_t OpcodeLookupTable = REBASE(0x5C077F0);
    const uintptr_t ScriptContextResume = REBASE(0x164A920);
    const uintptr_t LuaVmLoad = REBASE(0x15bCC00);
    const uintptr_t SetProtoCapabilities = REBASE(0x3346AC0); // not updated
    const uintptr_t JobsPointer = REBASE(0x8058020); // not updated
    const uintptr_t GetCurrentThreadId = REBASE(0x159b590);
    const uintptr_t TaskSchedulerTargetFps = REBASE(0x74D5644);
    const uintptr_t GetLuaState = REBASE(0x157D660); // 
    const uintptr_t GetGlobalState = REBASE(0x1663bd0);
    const uintptr_t IdentityPtr = REBASE(0x7515D98);
    const uintptr_t GetIdentityStruct = REBASE(0x9940);
    const uintptr_t KTable = REBASE(0x74d56d0);
    const uintptr_t GetProperty = REBASE(0x1214880);
    const uintptr_t PushInstance = REBASE(0x1657600);
    const uintptr_t PushInstance2 = REBASE(0x1657650);

    namespace DataModel
    {
        const uintptr_t FpsCap = 0x1b0;
        const uintptr_t PlaceId = 0x198;
        const uintptr_t Children = 0x70;
        const uintptr_t GameLoaded = 0x5F8;
        const uintptr_t ScriptContext = 0x3F0;
        const uintptr_t FakeDataModelToDataModel = 0x1C0;

        const uintptr_t FakeDataModelPointer = REBASE(0x7FA1988);
    }

    namespace Signals
    {
        const uintptr_t FireProximityPrompt = REBASE(0x1E1E6C0);
        const uintptr_t FireMouseClick = REBASE(0x1DA2010);
        const uintptr_t FireRightMouseClick = REBASE(0x1DA21B0);
        const uintptr_t FireMouseHoverEnter = REBASE(0x1DA35B0);
        const uintptr_t FireMouseHoverLeave = REBASE(0x1DA3750);
        const uintptr_t FireTouchInterest = REBASE(0x2160B20);
    }

    namespace ReplicateSignal {
        const uintptr_t Register = REBASE(0x3739470);
        const uintptr_t CastArgs = REBASE(0x1585DD0);
        const uintptr_t VariantCastInt64 = REBASE(0xADE8E0);
        //  const uintptr_t VariantCastInt = REBASE(0x17AA710); // not updated
        const uintptr_t VariantCastFloat = REBASE(0x4004870);
    }

    namespace hyperion
    {
        const uintptr_t Bitmap = REBASE(0x106e0b8);
        const uintptr_t ControlFlowGuard = REBASE(0x41b840);
        const uintptr_t Ic = REBASE(0x41b6b0);
    }

    namespace Luau
    {
        const uintptr_t LuaD_Throw = REBASE(0x3730EF0);
        const uintptr_t Luau_Execute = REBASE(0x3745BC0);
        const uintptr_t LuaO_NilObject = REBASE(0x572CEB8);
        const uintptr_t LuaH_DummyNode = REBASE(0x572C8A8);
    }

    namespace Scripts
    {
        const uintptr_t JobEnd = 0x1D8;
        const uintptr_t JobId = 0x138;
        const uintptr_t JobStart = 0x1D0;
        const uintptr_t Job_Name = 0x18;
        const uintptr_t LocalScriptByteCode = 0x1A8;
        const uintptr_t ModuleScriptByteCode = 0x150;
        const uintptr_t weak_thread_node = 0x180;
        const uintptr_t weak_thread_ref = 0x8;
        const uintptr_t weak_thread_ref_live = 0x20;
        const uintptr_t weak_thread_ref_live_thread = 0x8;
    }

    namespace ExtraSpace
    {
        const uintptr_t PropertyDescriptor = 0x5B8;
        const uintptr_t ClassDescriptorToClassName = 0x8;
        const uintptr_t ClassDescriptor = 0x18;
        const uintptr_t Identity = 0x30;
        const uintptr_t Capabilities = 0x48;
        const uintptr_t RequireBypass = 0x970;
        const uintptr_t InstanceToLuaState = 0x258;
        const uintptr_t ScriptContextToResume = 0x848;
    }

    namespace render {
        const uintptr_t renderview = 0x218;
        const uintptr_t DeviceD3D11 = 0x8;
        const uintptr_t VisualEngine = 0x10;
        const uintptr_t Swapchain = 0xD0;

    }
}

namespace Roblox
{
    inline auto TaskDefer = (int(__fastcall*)(lua_State*))Offsets::TaskDefer;
    inline auto Print = (uintptr_t(__fastcall*)(int, const char*, ...))Offsets::Print;
    inline auto Luau_Execute = (void(__fastcall*)(lua_State*))Offsets::Luau::Luau_Execute;

    inline auto GetProperty = (uintptr_t * (__thiscall*)(uintptr_t, uintptr_t*))Offsets::GetProperty;

    inline auto ScriptContextResume = (int(__fastcall*)(int64_t, DebuggerResult*, WeakThreadRef**, int32_t, bool, const char*))Offsets::ScriptContextResume;
    using TLuaVM__Load = int(__fastcall*)(lua_State*, void*, const char*, int);
    inline auto LuaVM__Load = (TLuaVM__Load)Offsets::LuaVmLoad;

    using TPushInstance = void(__fastcall*)(lua_State* state, void* instance);
    inline auto PushInstancescript = (TPushInstance)Offsets::PushInstance;
    inline auto PushInstance = (uintptr_t * (__fastcall*)(lua_State*, uintptr_t))Offsets::PushInstance;
    inline auto PushInstance2 = (uintptr_t * (__fastcall*)(lua_State*, std::shared_ptr<uintptr_t*>))Offsets::PushInstance2;

    inline auto IdentityPtr = reinterpret_cast<void* (__fastcall*)(void*)>(Offsets::IdentityPtr);
    inline auto GetIdentityStructure = (uintptr_t(__fastcall*)(uintptr_t))Offsets::GetIdentityStruct;

    inline auto Uintptr_TPushInstance = (uintptr_t * (__fastcall*)(lua_State*, uintptr_t))Offsets::PushInstance;

    inline auto SCResume = (int(__fastcall*)(int64_t, DebuggerResult*, WeakThreadRef**, int32_t, bool, const char*))Offsets::ScriptContextResume;

    using fireclick_t = void(__fastcall*)(void* clickDetector, float distance, void* player);
    inline auto FireMouseClick = reinterpret_cast<fireclick_t>(Offsets::Signals::FireMouseClick);
    inline auto FireRightMouseClick = reinterpret_cast<fireclick_t>(Offsets::Signals::FireRightMouseClick);

    using firehover_t = void(__fastcall*)(void* clickDetector, void* player);
    inline auto FireMouseHover = reinterpret_cast<firehover_t>(Offsets::Signals::FireMouseHoverEnter);
    inline auto FireMouseHoverLeave = reinterpret_cast<firehover_t>(Offsets::Signals::FireMouseHoverLeave);

    inline auto FireProximityPrompt = (void(__fastcall*)(uintptr_t))Offsets::Signals::FireProximityPrompt;

    inline auto FireTouchInterest = (void(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, bool, bool))Offsets::Signals::FireTouchInterest;

    inline std::string RequestBytecode(uintptr_t scriptPtr) {
        uintptr_t bytecodePtr = *reinterpret_cast<uintptr_t*>(scriptPtr + Offsets::Scripts::LocalScriptByteCode);

        if (!bytecodePtr) {
            bytecodePtr = *reinterpret_cast<uintptr_t*>(scriptPtr + Offsets::Scripts::ModuleScriptByteCode);
        }

        if (!bytecodePtr) return "Failed to get bytecode pointer";

        uintptr_t stringStruct = bytecodePtr + 0x10;

        size_t len = *reinterpret_cast<size_t*>(stringStruct + 0x10);
        if (len == 0) return "";

        char* data;
        if (*reinterpret_cast<size_t*>(stringStruct + 0x18) > 0xf) {
            data = *reinterpret_cast<char**>(stringStruct);
        }
        else {
            data = reinterpret_cast<char*>(stringStruct);
        }

        return std::string(data, len);
    }
}