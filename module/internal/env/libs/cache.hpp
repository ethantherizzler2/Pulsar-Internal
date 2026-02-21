#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lobject.h>
#include <unordered_map>
#include <unordered_set>

#include <internal/utils.hpp>
#include <internal/globals.hpp>

namespace Cache
{
    int invalidate(lua_State* L) {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        const auto rawUserdata = *static_cast<void**>(lua_touserdata(L, 1));

        lua_pushlightuserdata(L, (void*)Roblox::PushInstance);
        lua_gettable(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, reinterpret_cast<void*>(rawUserdata));
        lua_pushnil(L);
        lua_settable(L, -3);

        return 0;
    }

    int replace(lua_State* L) {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TUSERDATA);

        const auto rawUserdata = *static_cast<void**>(lua_touserdata(L, 1));

        lua_pushlightuserdata(L, (void*)Roblox::PushInstance);
        lua_gettable(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, rawUserdata);
        lua_pushvalue(L, 2);
        lua_settable(L, -3);

        return 0;
    }

    int iscached(lua_State* L) {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        const auto rawUserdata = *static_cast<void**>(lua_touserdata(L, 1));

        lua_pushlightuserdata(L, (void*)Roblox::PushInstance);
        lua_gettable(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, rawUserdata);
        lua_gettable(L, -2);

        lua_pushboolean(L, lua_type(L, -1) != LUA_TNIL);
        return 1;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        lua_newtable(L);

        Utils::RegisterTableAliases(L, invalidate, {
            "invalidate", "Invalidate", "clear", "Clear"
            });

        Utils::RegisterTableAliases(L, iscached, {
            "iscached", "isCached", "IsCached", "is_cached"
            });

        Utils::RegisterTableAliases(L, replace, {
            "replace", "Replace", "set", "Set"
            });

        lua_setglobal(L, "cache");
    }
}