#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lobject.h>
#include <lfunc.h>
#include <lstring.h>
#include <ltable.h>
#include <lapi.h>
#include <lgc.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <internal/env/libs/closures.hpp>
#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <internal/roblox/update/helpers/luauhelper.hpp>

inline void lua_check(lua_State* L, int n)
{
    if (lua_gettop(L) < n) {
        luaL_error(L, oxorany("expected at least %d arguments, got %d"), n, lua_gettop(L));
    }
}

namespace Metatable
{
    int isreadonly(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua_pushboolean(L, lua_getreadonly(L, 1));
        return 1;
    }

    int setreadonly(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        lua_setreadonly(L, 1, lua_toboolean(L, 2));
        return 0;
    }

    inline bool checkreadonly(lua_State* L, int idx)
    {
        if (!lua_istable(L, idx)) {
            return false;
        }

        int top = lua_gettop(L);
        lua_pushvalue(L, idx);
        lua_replace(L, 1);

        isreadonly(L);

        bool result = lua_toboolean(L, -1);
        lua_settop(L, top);

        return result;
    }

    inline void setreadonlystate(lua_State* L, int idx, bool readonly)
    {
        if (!lua_istable(L, idx)) {
            return;
        }

        int top = lua_gettop(L);

        lua_pushvalue(L, idx);
        lua_pushboolean(L, readonly ? 1 : 0);

        lua_replace(L, 1);
        lua_replace(L, 2);

        setreadonly(L);

        lua_settop(L, top);
    }

    int getrawmetatable(lua_State* l) {
        lua_check(l, 1);
        luaL_checkany(l, 1);

        if (!lua_getmetatable(l, 1))
            lua_pushnil(l);

        return 1;
    }

    int setrawmetatable(lua_State* l) {
        luaL_checkany(l, 1);
        luaL_checktype(l, 2, LUA_TTABLE);
        lua_setmetatable(l, 1);
        lua_pushvalue(l, 1);
        return 1;
    }

    int getnamecallmethod(lua_State* L)
    {
        lua_check(L, 0);

        lua_rawcheckstack(L, 1);

        if (const auto method = lua_namecallatom(L, nullptr)) {
            luaC_threadbarrier(L);
            lua_pushstring(L, method);
        }
        else
            lua_pushnil(L);

        return 1;;
    }


    int setnamecallmethod(lua_State* L)
    {
        lua_check(L, 1);
        luaL_checktype(L, 1, LUA_TSTRING);

        L->namecall = tsvalue(luaA_toobject(L, 1));

        return 0;
    }

    static bool IsMetamethod(const char* Metamethod)
    {
        if (!Metamethod || !*Metamethod)
            return false;

        static const std::unordered_set<std::string> Allowed = {
            "__namecall",
            "__newindex",
            "__index"
        };

        return Allowed.find(Metamethod) != Allowed.end();
    }

    int hookmetamethod(lua_State* L)
    {
        luaL_checkany(L, 1);
        luaL_checkstring(L, 2);
        luaL_checkany(L, 3);

        if (!lua_getmetatable(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        int Table = lua_gettop(L);
        const char* Method = lua_tostring(L, 2);
        if (!IsMetamethod(Method))
        {
            lua_pushnil(L);
            return 1;
        }

        auto OldReadOnly = lua_getreadonly(L, 1);

        lua_getfield(L, Table, Method);
        lua_pushvalue(L, -1);

        lua_setreadonly(L, Table, false);

        lua_pushvalue(L, 3);
        lua_setfield(L, Table, Method);

        lua_setreadonly(L, Table, OldReadOnly);

        lua_remove(L, Table);

        return 1;
    }

    inline int clonefunction(lua_State* L)
    {
        if (!lua_isfunction(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        Closure* original = lua_toclosure(L, 1);
        if (!original) {
            lua_pushnil(L);
            return 1;
        }

        Closure* clone = nullptr;

        __try {
            if (original->isC) {
                int nups = original->nupvalues;
                if (nups < 0) nups = 0;
                if (nups > 255) nups = 255;

                clone = luaF_newCclosure(L, nups, original->env);
                if (clone) {
                    clone->c.f = original->c.f;
                    clone->c.cont = original->c.cont;
                    clone->c.debugname = original->c.debugname;

                    for (int i = 0; i < nups; i++) {
                        setobj(L, &clone->c.upvals[i], &original->c.upvals[i]);
                    }
                }
            }
            else {
                if (!original->l.p) {
                    lua_pushnil(L);
                    return 1;
                }

                int nups = original->nupvalues;
                if (nups < 0) nups = 0;
                if (nups > 255) nups = 255;

                clone = luaF_newLclosure(L, nups, original->env, original->l.p);
                if (clone) {
                    for (int i = 0; i < nups; i++) {
                        clone->l.uprefs[i] = original->l.uprefs[i];
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            lua_pushnil(L);
            return 1;
        }

        if (!clone) {
            lua_pushnil(L);
            return 1;
        }

        setclvalue(L, L->top, clone);
        L->top++;
        return 1;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        Utils::AddFunction(L, "getrawmetatable", getrawmetatable);
        Utils::AddFunction(L, "setrawmetatable", setrawmetatable);
     	Utils::AddFunction(L, "hookmetamethod", hookmetamethod);
        Utils::AddFunction(L, "clonefunction", clonefunction);
        Utils::AddFunction(L, "getnamecallmethod", getnamecallmethod);
        Utils::AddFunction(L, "setnamecallmethod", setnamecallmethod);
        Utils::AddFunction(L, "setreadonly", setreadonly);
        Utils::AddFunction(L, "isreadonly", isreadonly);
    }
}