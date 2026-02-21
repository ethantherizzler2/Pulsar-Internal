#pragma once

#include <Windows.h>
#include <lstate.h>
#include <map>

#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <internal/roblox/update/offsets.hpp>

namespace Interactions
{
    void checkIsA(lua_State* L, const char* idk) {
        lua_getfield(L, 1, "IsA");
        lua_pushvalue(L, 1);
        lua_pushstring(L, idk);
        lua_call(L, 2, 1);
    }

    int firetouchinterest(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TUSERDATA);
        luaL_argexpected(L, lua_isboolean(L, 3) || lua_isnumber(L, 3), 3, ("Bool or Number"));

        if (std::string(luaL_typename(L, 1)) != "Instance")
            luaL_typeerror(L, 1, "Instance");

        if (std::string(luaL_typename(L, 2)) != "Instance")
            luaL_typeerror(L, 2, "Instance");

        checkIsA(L, "BasePart");
        const bool bp_1 = lua_toboolean(L, -1);
        lua_pop(L, 1);

        checkIsA(L, "BasePart");
        const bool bp_2 = lua_toboolean(L, -1);
        lua_pop(L, 1);

        luaL_argexpected(L, bp_1, 1, "BasePart");
        luaL_argexpected(L, bp_2, 2, "BasePart");

        const uintptr_t basePart1 = *static_cast<uintptr_t*>(lua_touserdata(L, 1));
        const uintptr_t basePart2 = *static_cast<uintptr_t*>(lua_touserdata(L, 2));

        const uintptr_t Primitive1 = *reinterpret_cast<uintptr_t*>(basePart1 + Offsets::DataModel::Primitive);
        const uintptr_t Primitive2 = *reinterpret_cast<uintptr_t*>(basePart2 + Offsets::DataModel::Primitive);

        const uintptr_t Overlap1 = *reinterpret_cast<uintptr_t*>(Primitive1 + Offsets::DataModel::Overlap);
        const uintptr_t Overlap2 = *reinterpret_cast<uintptr_t*>(Primitive2 + Offsets::DataModel::Overlap);

        if (Overlap1 != Overlap2 || !Primitive1 || !Primitive2)
            luaL_error(L, oxorany("firetouchinterest errored: this is not supposed to happen if did please report this to us!"));

        const int Fire = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : lua_tointeger(L, 3);

        Roblox::FireTouchInterest(Overlap2, Primitive1, Primitive2, Fire, 1);
        return 0;
    }

    int fireproximityprompt(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        lua_getglobal(L, "typeof");
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        const bool inst = (strcmp(lua_tolstring(L, -1, 0), "Instance") == 0);
        lua_pop(L, 1);

        if (!inst)
            luaL_argerror(L, 1, ("Expected an Instance"));

        lua_getfield(L, 1, "IsA");
        lua_pushvalue(L, 1);
        lua_pushstring(L, "ProximityPrompt");
        lua_call(L, 2, 1);
        const bool expected = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (!expected)
            luaL_argerror(L, 1, ("Expected a ProximityPrompt"));

        reinterpret_cast<int(__thiscall*)(std::uintptr_t)>(Roblox::FireProximityPrompt)(*reinterpret_cast<std::uintptr_t*>(lua_touserdata(L, 1)));
        return 0;
    }

    int fireclickdetector(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        auto FireDistance = luaL_optnumber(L, 2, 0);
        auto EventName = luaL_optstring(L, 3, "MouseClick");

        lua_getglobal(L, "game");
        lua_getfield(L, -1, "GetService");
        lua_insert(L, -2);
        lua_pushstring(L, "Players");
        lua_pcall(L, 2, 1, 0);
        lua_getfield(L, -1, "LocalPlayer");

        if (strcmp(EventName, "MouseClick") == 0)
        {
            Roblox::FireMouseClick(*static_cast<void**>(lua_touserdata(L, 1)), FireDistance, *static_cast<void**>(lua_touserdata(L, -1)));
        }
        else if (strcmp(EventName, "RightMouseClick") == 0)
        {
            Roblox::FireRightMouseClick(*static_cast<void**>(lua_touserdata(L, 1)), FireDistance, *static_cast<void**>(lua_touserdata(L, -1)));
        }
        else if (strcmp(EventName, "MouseHoverEnter") == 0)
        {
            Roblox::FireMouseHover(*static_cast<void**>(lua_touserdata(L, 1)), *static_cast<void**>(lua_touserdata(L, -1)));
        }
        else if (strcmp(EventName, "MouseHoverLeave") == 0)
        {
            Roblox::FireMouseHoverLeave(*static_cast<void**>(lua_touserdata(L, 1)), *static_cast<void**>(lua_touserdata(L, -1)));
        }

        return 0;
    }

    inline int setproximitypromptduration(lua_State* L)
    {
        if (!lua_isuserdata(L, 1) || !lua_isnumber(L, 2)) {
            return 0;
        }

        lua_getfield(L, 1, "ClassName");
        const char* cn = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
        lua_pop(L, 1);

        if (!cn || strcmp(cn, "ProximityPrompt") != 0) {
            return 0;
        }

        double duration = lua_tonumber(L, 2);
        lua_pushnumber(L, duration);
        lua_setfield(L, 1, "HoldDuration");

        return 0;
    }

    inline int getproximitypromptduration(lua_State* L)
    {
        if (!lua_isuserdata(L, 1)) {
            lua_pushnumber(L, 0);
            return 1;
        }

        lua_getfield(L, 1, "ClassName");
        const char* cn = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
        lua_pop(L, 1);

        if (!cn || strcmp(cn, "ProximityPrompt") != 0) {
            lua_pushnumber(L, 0);
            return 1;
        }

        lua_getfield(L, 1, "HoldDuration");
        if (!lua_isnumber(L, -1)) {
            lua_pop(L, 1);
            lua_pushnumber(L, 0);
            return 1;
        }

        return 1;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        Utils::AddFunction(L, "fireproximityprompt", fireproximityprompt);
        Utils::AddFunction(L, "fireclickdetector", fireclickdetector);
        Utils::AddFunction(L, "firetouchinterest", firetouchinterest);

        Utils::AddFunction(L, "setproximitypromptduration", setproximitypromptduration);
        Utils::AddFunction(L, "getproximitypromptduration", getproximitypromptduration);
    }
}