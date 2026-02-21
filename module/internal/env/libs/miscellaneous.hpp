#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lgc.h>

#include <internal/utils.hpp>
#include <internal/globals.hpp>

#include "../../../../dependencies/luau/VM/src/lstate.h"
#include "../../../../dependencies/luau/VM/src/lgc.h"
#include "../../../../dependencies/luau/VM/src/lobject.h"
#include "../../../../dependencies/luau/VM/src/lmem.h"
#include <vector>
#include <lmem.h>
#include <internal/roblox/task_scheduler/scheduler.hpp>
#include "metatable.hpp"

namespace Miscellaneous
{
    struct getgc_context
    {
        lua_State* state;
        bool include_tables;
        int index;
        int result_index;
    };

    static bool getgc_visit(void* context, lua_Page* page, GCObject* gco)
    {
        (void)page;

        auto* ctx = static_cast<getgc_context*>(context);

        int tt = gco->gch.tt;
        if (tt == LUA_TFUNCTION || tt == LUA_TUSERDATA || (ctx->include_tables && tt == LUA_TTABLE))
        {
            TValue obj;
            switch (tt)
            {
            case LUA_TFUNCTION:
                setclvalue(ctx->state, &obj, gco2cl(gco));
                break;
            case LUA_TUSERDATA:
                setuvalue(ctx->state, &obj, gco2u(gco));
                break;
            default:
                sethvalue(ctx->state, &obj, gco2h(gco));
                break;
            }

            luaA_pushobject(ctx->state, &obj);
            lua_rawseti(ctx->state, ctx->result_index, ++ctx->index);
        }

        return false;
    }

    int identifyexecutor(lua_State* L)
    {
        lua_pushstring(L, "Pulsar");
        lua_pushstring(L, "1.0.0");
        return 2;
    }

    int getexecutorname(lua_State* L)
    {
        lua_pushstring(L, "Pulsar");
        return 1;
    }

    int checkcaller(lua_State* L)
    {
        lua_pushboolean((lua_State*)L, true);
        return 0;
	}

    int getgenv(lua_State* L)
    {
        if (SharedVariables::ExploitThread == L) {
            lua_pushvalue(L, LUA_GLOBALSINDEX);
            return 1;
        }

        lua_rawcheckstack(L, 1);
        luaC_threadbarrier(L);
        luaC_threadbarrier(SharedVariables::ExploitThread);
        lua_pushvalue(SharedVariables::ExploitThread, LUA_GLOBALSINDEX);
        lua_xmove(SharedVariables::ExploitThread, L, 1);

        return 1;
    }

    int getrenv(lua_State* L)
    {
        lua_check(L, 0);

        uintptr_t ScriptContext = TaskScheduler::GetScriptContext(SharedVariables::LastDataModel);
        if (!ScriptContext)
            return false;

        lua_State* roblox_state = TaskScheduler::GetLuaStateForInstance(ScriptContext);

        LuaTable* clone = luaH_clone(L, roblox_state->gt);

        lua_rawcheckstack(L, 1);
        luaC_threadbarrier(L);
        luaC_threadbarrier(roblox_state);

        L->top->value.p = clone;
        L->top->tt = LUA_TTABLE;
        L->top++;

        lua_rawgeti(L, LUA_REGISTRYINDEX, 2);
        lua_setfield(L, -2, ("_G"));
        lua_rawgeti(L, LUA_REGISTRYINDEX, 4);
        lua_setfield(L, -2, ("shared"));
        return 1;
    }

    int getmenv(lua_State* L)
    {
        if (!lua_isuserdata(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        lua_getfield(L, 1, "ClassName");
        const char* cn = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
        lua_pop(L, 1);

        if (!cn || strcmp(cn, "ModuleScript") != 0) {
            lua_pushnil(L);
            return 1;
        }

        lua_getglobal(L, "require");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            lua_pushnil(L);
            return 1;
        }

        lua_pushvalue(L, 1);
        if (lua_pcall(L, 1, 1, 0) != 0) {
            lua_pop(L, 1);
            lua_pushnil(L);
            return 1;
        }

        lua_getfenv(L, -1);
        if (lua_istable(L, -1)) {
            lua_remove(L, -2);
            return 1;
        }

        lua_pop(L, 2);
        lua_pushnil(L);
        return 1;
    }

    int getgc(lua_State* L)
    {
        bool include_tables = false;
        if (!lua_isnoneornil(L, 1))
            include_tables = lua_toboolean(L, 1);

        lua_gc(L, LUA_GCCOLLECT, 0);

        luaC_threadbarrier(L);

        lua_gc(L, LUA_GCSTOP, 0);

        lua_newtable(L);
        int result_index = lua_absindex(L, -1);

        getgc_context ctx{ L, include_tables, 0, result_index };
        luaM_visitgco(L, &ctx, getgc_visit);

        lua_gc(L, LUA_GCRESTART, 0);

        return 1;
    }
    
    int getactors(lua_State* L)
    {
        lua_newtable(L);
        int idx = 1;

        lua_getglobal(L, "game");
        if (lua_isuserdata(L, -1)) {
            lua_getfield(L, -1, "GetDescendants");
            if (lua_isfunction(L, -1)) {
                lua_pushvalue(L, -2);
                if (lua_pcall(L, 1, 1, 0) == 0 && lua_istable(L, -1)) {
                    lua_pushnil(L);
                    while (lua_next(L, -2)) {
                        if (lua_isuserdata(L, -1)) {
                            lua_getfield(L, -1, "ClassName");
                            if (lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "Actor") == 0) {
                                lua_pop(L, 1);
                                lua_pushvalue(L, -1);
                                lua_rawseti(L, 1, idx++);
                            }
                            else {
                                lua_pop(L, 1);
                            }
                        }
                        lua_pop(L, 1);
                    }
                }
                lua_pop(L, 1);
            }
            else {
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        return 1;
    }

    int getregistry(lua_State* L)
    {
        lua_pushvalue(L, LUA_REGISTRYINDEX);
        return 1;
    }

    int saveinstance(lua_State* L)
    {
        std::string saveinstance = "loadstring(game:HttpGet('https://raw.githubusercontent.com/ethantherizzler2/Roblox-Scripts/refs/heads/main/skid.lua'))()";
		TaskScheduler::RequestExecution(saveinstance);
	}

    inline int getsenv(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        uintptr_t script = *(uintptr_t*)lua_touserdata(L, 1);
        if (!CheckMemory(script))
        {
            lua_newtable(L);
            return 1;
        }

        uintptr_t classDescriptor = *(uintptr_t*)(script + Offsets::ExtraSpace::ClassDescriptor);
        if (!CheckMemory(classDescriptor))
        {
            lua_newtable(L);
            return 1;
        }

        const char* className = *(const char**)(classDescriptor + Offsets::ExtraSpace::ClassDescriptorToClassName);
        if (!className)
        {
            lua_newtable(L);
            return 1;
        }

        if (strcmp(className, "Script") != 0 && strcmp(className, "LocalScript") != 0)
        {
            luaL_argerror(L, 1, "expected Script or LocalScript");
        }

        uintptr_t script_node = *reinterpret_cast<uintptr_t*>(script + Offsets::Scripts::weak_thread_node);
        uintptr_t node_weak_thread_ref = script_node ? *reinterpret_cast<uintptr_t*>(script_node + Offsets::Scripts::weak_thread_ref) : 0;
        uintptr_t live_thread_ref = node_weak_thread_ref ? *reinterpret_cast<uintptr_t*>(node_weak_thread_ref + Offsets::Scripts::weak_thread_ref_live) : 0;
        lua_State* script_thread = live_thread_ref ? *reinterpret_cast<lua_State**>(live_thread_ref + Offsets::Scripts::weak_thread_ref_live_thread) : nullptr;

        if (!script_thread)
        {
            lua_newtable(L);
            return 1;
        }

        lua_pushvalue(script_thread, LUA_GLOBALSINDEX);
        if (lua_isnil(script_thread, -1))
        {
            lua_pop(script_thread, 1);
            lua_newtable(script_thread);
            lua_pushvalue(script_thread, -1);
        }

        lua_xmove(script_thread, L, 1);
        return 1;
    }


    inline int setfpscap(lua_State* L) {
        luaL_checktype(L, 1, LUA_TNUMBER);

        double newCap = lua_tonumber(L, 1);

        if (!newCap || newCap == 0)
            newCap = 5000;

        if (!CheckMemory(Offsets::RawScheduler))
            return 0;

        uintptr_t TaskScheduler = *(uintptr_t*)Offsets::RawScheduler;
        if (!TaskScheduler || !CheckMemory(TaskScheduler) || !CheckMemory(TaskScheduler + Offsets::DataModel::FpsCap))
            return 0;

        static auto lastWriteTime = std::chrono::steady_clock::now();
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastWriteTime).count();
        if (elapsedTime < 5) 
            return 0;

        *(double*)(TaskScheduler + Offsets::DataModel::FpsCap) = 1 / newCap;

        lastWriteTime = currentTime;

        return 0;
    }

    inline int queueonteleport(lua_State* L) {
        luaL_checktype(L, 1, LUA_TSTRING);

        return 0;
    }

    int info(lua_State* L) {
        luaL_checktype(L, 1, LUA_TSTRING);

        std::string data = lua_tostring(L, 1);

        Roblox::Print(1, data.data());
        return 0;
    }

    inline int gettenv(lua_State* L)
    {
        if (!lua_isthread(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        lua_State* thread = lua_tothread(L, 1);
        if (!thread) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushvalue(thread, LUA_GLOBALSINDEX);
        lua_xmove(thread, L, 1);
        return 1;
    }

    inline uintptr_t IdentityToCapabilities(uintptr_t Identity) {
        uintptr_t obfuscated = (Identity ^ 0xABC) & 0xFFFF;
        uintptr_t result = 0;

        struct Pair {
            uintptr_t key;
            uintptr_t value;
        };

        static const Pair mapping[] = {
            { (4 ^ 0xABC) & 0xFFFF,  0x2000000000000003LL },
            { (3 ^ 0xABC) & 0xFFFF,  0x200000000000000BLL },
            { (5 ^ 0xABC) & 0xFFFF,  0x2000000000000001LL },
            { (6 ^ 0xABC) & 0xFFFF,  0x600000000000000BLL },
            { (8 ^ 0xABC) & 0xFFFF,  0x200000000000003FLL },
            { (9 ^ 0xABC) & 0xFFFF,  12LL },
            { (10 ^ 0xABC) & 0xFFFF, 0x6000000000000003LL },
            { (11 ^ 0xABC) & 0xFFFF, 0x2000000000000000LL },
        };

        for (auto& pair : mapping) {
            if (pair.key == obfuscated) {
                result = pair.value;
                break;
            }
        }

        char buffer[256];
        sprintf(buffer, "> Identity: %llu, Obfuscated: 0x%llX, Capabilities: 0x%llX",
            Identity, obfuscated, result);
        Logger::warn(buffer);

        return (result ? result : 0) | 0xFFFFFFFFFFFFFFF0uLL;

    }

    inline int setthreadidentity(lua_State* L)
    {
        lua_check(L, 1);
        luaL_checktype(L, 1, LUA_TNUMBER);

        const int NewIdentity = lua_tointeger(L, 1);

        const int64_t NewCapabilities = IdentityToCapabilities(NewIdentity);

        char buf[128];
        sprintf(buf, "> Setting Identity %d with Capabilities: 0x%llX", NewIdentity, NewCapabilities);
        Logger::warn(buf);
        ;

        TaskScheduler::SetIdentity(L, NewIdentity, NewCapabilities);

        return 0;
    }

    inline int getthreadidentity(lua_State* L)
    {
        lua_check(L, 0);

        lua_pushinteger(L, L->userdata->Identity);
        return 1;
    }

    inline int gethui(lua_State* L)
    {
        lua_getglobal(L, "game");
        lua_getfield(L, -1, "GetService");
        lua_pushvalue(L, -2);
        lua_pushstring(L, "CoreGui");

        if (lua_pcall(L, 2, 1, 0) == 0) {
            lua_remove(L, -2);
            return 1;
        }

        lua_pop(L, 1);
        lua_getfield(L, -1, "GetService");
        lua_pushvalue(L, -2);
        lua_pushstring(L, "Players");

        if (lua_pcall(L, 2, 1, 0) == 0) {
            lua_getfield(L, -1, "LocalPlayer");
            lua_getfield(L, -1, "PlayerGui");
            lua_remove(L, -2);
            lua_remove(L, -2);
            lua_remove(L, -2);
            return 1;
        }

        lua_newtable(L);
        return 1;
    }

    void RegisterLibrary(lua_State* L)
    {
        Utils::AddFunction(L, "identifyexecutor", identifyexecutor);
        Utils::AddFunction(L, "getexecutorname", getexecutorname);
		Utils::AddFunction(L, "setthreadidentity", setthreadidentity); // littel fixes needed here, will do later
		Utils::AddFunction(L, "getthreadidentity", getthreadidentity); // littel fixes needed here, will do later
		Utils::AddFunction(L, "info", info);
		Utils::AddFunction(L, "saveinstance", gethui);
        Utils::AddFunction(L, "getgenv", getgenv);
        Utils::AddFunction(L, "getrenv", getrenv);
        Utils::AddFunction(L, "getmenv", getmenv);
        Utils::AddFunction(L, "gettenv", gettenv);
        Utils::AddFunction(L, "getsenv", getsenv);
        Utils::AddFunction(L, "getgc", getgc);
        Utils::AddFunction(L, "setfpscap", setfpscap);
        Utils::AddFunction(L, "queue_on_teleport", queueonteleport);
        Utils::AddFunction(L, "queueonteleport", queueonteleport);
        Utils::AddFunction(L, "getactors", getactors);
		Utils::AddFunction(L, "checkcaller", checkcaller);
        Utils::AddFunction(L, "getregistry", getregistry);
        Utils::AddFunction(L, "getreg", getregistry);
    }
}