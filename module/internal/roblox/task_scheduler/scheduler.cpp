#include <internal/globals.hpp>
#include <internal/execution/execution.hpp>
#include <internal/env/env.hpp>
#include <internal/roblox/task_scheduler/scheduler.hpp>
#include <internal/env/yield/yield.hpp>
#include <bit>

namespace
{
    std::mutex ExecutionMutex;
}


void TaskScheduler::SetProtoCapabilities(Proto* Proto, uintptr_t* Capabilities)
{
    if (!Proto)
        return;

    Proto->userdata = Capabilities;
    for (int i = 0; i < Proto->sizep; ++i)
        SetProtoCapabilities(Proto->p[i], Capabilities);
}

void TaskScheduler::SetIdentity(lua_State* L, int Level, uintptr_t Capabilities)
{
    if (!L || !L->userdata)
        return;

    L->userdata->Identity = Level;
    L->userdata->Capabilities = Capabilities;

    std::uintptr_t identity_struct = Roblox::GetIdentityStructure(*reinterpret_cast<std::uintptr_t*>(Offsets::IdentityPtr));
    if (!identity_struct) return;

    *reinterpret_cast<std::int32_t*>(identity_struct) = Level;
    *reinterpret_cast<std::uintptr_t*>(identity_struct + 0x20) = Capabilities;
}

void TaskScheduler::SetThreadCapabilities(lua_State* L, int Level, uintptr_t Capabilities)
{
    if (!L || !L->userdata)
        return;

    L->userdata->Identity = Level;
    L->userdata->Capabilities = Capabilities;
}

uintptr_t TaskScheduler::GetDataModel()
{
    uintptr_t FakeDataModel = *reinterpret_cast<uintptr_t*>(Offsets::DataModel::FakeDataModelPointer);
    if (!FakeDataModel)
        return 0;
    uintptr_t DataModel = *reinterpret_cast<uintptr_t*>(FakeDataModel + Offsets::DataModel::FakeDataModelToDataModel);
    return DataModel;
}


uintptr_t TaskScheduler::GetScriptContext(uintptr_t DataModel)
{
    if (!DataModel)
        return 0;

    uintptr_t Children = *reinterpret_cast<uintptr_t*>(DataModel + Offsets::DataModel::Children);
    if (!Children)
        return 0;

    uintptr_t ScriptContext = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(Children) + Offsets::DataModel::ScriptContext);
    return ScriptContext;
}
inline auto Lua = (lua_State * (__fastcall*)(uint64_t, uint64_t*, uint64_t*))Offsets::ExtraSpace::luastate;

lua_State* TaskScheduler::GetLuaStateForInstance(uintptr_t Instance)
{
    *reinterpret_cast<BOOLEAN*>(Instance + Offsets::ExtraSpace::RequireBypass) = TRUE;

    uint64_t Null = 0;
    return Lua(Instance, &Null, &Null);
}


void TaskScheduler::ProcessExecutionQueue()
{
    if (!SharedVariables::ExploitThread || SharedVariables::ExecutionRequests.empty())
        return;

    std::lock_guard<std::mutex> lock(ExecutionMutex);

    if (!SharedVariables::ExecutionRequests.empty())
    {
        try
        {
            Execution::ExecuteScript(SharedVariables::ExploitThread, SharedVariables::ExecutionRequests.front());
        }
        catch (...)
        {

        }

        SharedVariables::ExecutionRequests.erase(SharedVariables::ExecutionRequests.begin());
    }
}

int ScriptsHandler(lua_State* L)
{
    TaskScheduler::ProcessExecutionQueue();
    Yielding::RunYield();
    return 0;
}

void SetupExecution(lua_State* L)
{
    if (!L)
        return;

    lua_getglobal(L, "game");
    lua_getfield(L, -1, "GetService");
    lua_pushvalue(L, -2);
    lua_pushstring(L, "RunService");
    lua_pcall(L, 2, 1, 0);

    lua_getfield(L, -1, "RenderStepped");
    lua_getfield(L, -1, "Connect");
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, ScriptsHandler, nullptr, 0);
    lua_pcall(L, 2, 0, 0);

    lua_pop(L, 2);
}

bool TaskScheduler::SetupExploit()
{
    if (!SharedVariables::LastDataModel)
        return false;

    uintptr_t ScriptContext = TaskScheduler::GetScriptContext(SharedVariables::LastDataModel);
    if (!ScriptContext)
        return false;

    lua_State* RobloxState = TaskScheduler::GetLuaStateForInstance(ScriptContext);
    if (!RobloxState)
        return false;

    SharedVariables::ExploitThread = lua_newthread(RobloxState);
    if (!SharedVariables::ExploitThread)
        return false;

    lua_pop(RobloxState, 1);

    TaskScheduler::SetThreadCapabilities(SharedVariables::ExploitThread, 8, MaxCapabilities);
    luaL_sandboxthread(SharedVariables::ExploitThread);
    Environment::SetupEnvironment(SharedVariables::ExploitThread);
    SetupExecution(SharedVariables::ExploitThread);

    return true;
}

void TaskScheduler::RequestExecution(std::string Script)
{
    if (Script.empty())
        return;

    std::lock_guard<std::mutex> lock(ExecutionMutex);
    SharedVariables::ExecutionRequests.push_back(Script);
}