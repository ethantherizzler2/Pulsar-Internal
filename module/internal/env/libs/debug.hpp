#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lgc.h>
#include <lualib.h>
#include <lobject.h>
#include <lfunc.h>
#include <lstring.h>
#include <cstring>
#include <algorithm>
#include <internal/utils.hpp>
#include <internal/globals.hpp>

namespace Debug
{
    Closure* header_get_function(lua_State* L, bool allowCclosure = false, bool popcl = true)
    {
        luaL_checkany(L, 1);

        if (!(lua_isfunction(L, 1) || lua_isnumber(L, 1)))
        {
            luaL_argerror(L, 1, "function or number");
        }

        int level = 0;
        if (lua_isnumber(L, 1))
        {
            level = lua_tointeger(L, 1);

            if (level <= 0)
            {
                luaL_argerror(L, 1, "level out of range");
            }
        }
        else if (lua_isfunction(L, 1))
        {
            level = -lua_gettop(L);
        }

        lua_Debug ar;
        if (!lua_getinfo(L, level, "f", &ar))
        {
            luaL_argerror(L, 1, "invalid level");
        }

        if (!lua_isfunction(L, -1))
            luaL_argerror(L, 1, "level does not point to a function");
        if (lua_iscfunction(L, -1) && !allowCclosure)
            luaL_argerror(L, 1, "level points to c function");

        if (!allowCclosure && lua_iscfunction(L, -1))
        {
            luaL_argerror(L, 1, "luau function expected");
        }

        auto function = clvalue(luaA_toobject(L, -1));
        if (popcl) lua_pop(L, 1);

        return function;
    }

    inline int dbg_getupvalue(lua_State* L)
    {
        if (!lua_isfunction(L, 1) || !lua_isnumber(L, 2)) {
            luaL_error(L, oxorany("expected function, index"));
            return 0;
        }

        Closure* cl = lua_toclosure(L, 1);
        if (!cl || cl->isC) {
            luaL_error(L, oxorany("C closures are not supported"));
            return 0;
        }

        int idx = (int)lua_tointeger(L, 2);
        if (idx < 1 || idx > cl->nupvalues) {
            luaL_error(L, oxorany("upvalue index out of range"));
            return 0;
        }

        setobj(L, L->top, &cl->l.uprefs[idx - 1]);
        L->top++;
        return 1;
    }

    inline int dbg_getupvalues(lua_State* L)
    {
        if (!lua_isfunction(L, 1)) {
            luaL_error(L, oxorany("expected function"));
            return 0;
        }

        Closure* cl = lua_toclosure(L, 1);
        if (!cl || cl->isC) {
            luaL_error(L, oxorany("C closures are not supported"));
            return 0;
        }

        lua_newtable(L);
        for (int i = 0; i < cl->nupvalues; ++i) {
            setobj(L, L->top, &cl->l.uprefs[i]);
            L->top++;
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }

    int dbg_setupvalue(lua_State* L)
    {
        const auto function = header_get_function(L, false, false);
        const auto idx = lua_tointeger(L, 2);

        if (function->nupvalues <= 0)
        {
            luaL_argerror(L, 1, "function has no upvalues");
        }

        if (!(idx >= 1 && idx <= function->nupvalues))
        {
            luaL_argerror(L, 2, "index out of range");
        }

        lua_pushvalue(L, 3);
        lua_setupvalue(L, -2, idx);
        return 1;
    }

    int dbg_getconstants(lua_State* L) {
        luaL_checkany(L, 1);

        if (lua_iscfunction(L, -1))
        {
            luaL_argerror(L, 1, oxorany("Expected a Lua Closure!"));
        }

        if (lua_type(L, 1) != LUA_TNUMBER && lua_type(L, 1) != LUA_TFUNCTION)
            luaL_argerror(L, 1, oxorany("Expected <number> or <function>"));

        if (lua_type(L, 1) == LUA_TNUMBER) {
            lua_newtable(L);
            return 1;
        }

        const auto cl = clvalue(luaA_toobject(L, 1));

        lua_newtable(L);

        for (auto i = 0; i < cl->l.p->sizek; i++) {
            const auto constant = &cl->l.p->k[i];

            if (constant->tt == LUA_TTABLE || constant->tt == LUA_TNIL || constant->tt == LUA_TFUNCTION) {
                lua_pushnil(L);
                lua_rawseti(L, -2, i + 1);
                continue;
            }

            TValue* i_o = (L->top);
            i_o->value = constant->value;
            i_o->tt = constant->tt;
            incr_top(L);

            lua_rawseti(L, -2, i + 1);
        }

        return 1;
    }

    int dbg_getconstant(lua_State* L) {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        luaL_checktype(L, 2, LUA_TNUMBER);

        if (lua_iscfunction(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        int Idx = lua_tonumber(L, 2);
        Proto* p = clvalue(luaA_toobject(L, 1))->l.p;
        if (!p) {
            lua_pushnil(L);
            return 1;
        }

        if (Idx < 1 || Idx > p->sizek) {
            lua_pushnil(L);
            return 1;
        }

        TValue* KTable = p->k;
        if (!KTable) {
            lua_pushnil(L);
            return 1;
        }

        TValue* tval = &(KTable[Idx - 1]);
        if (!tval) {
            lua_pushnil(L);
            return 1;
        }

        if (tval->tt == LUA_TFUNCTION) {
            lua_pushnil(L);
            return 1;
        }

        TValue* i_o = (L->top);
        i_o->value = tval->value;
        i_o->tt = tval->tt;
        L->top++;

        return 1;
    }

    int dbg_setconstant(lua_State* rl) {

        luaL_checktype(rl, 1, LUA_TFUNCTION);

        const auto closure = *reinterpret_cast<Closure**>(index2addr(rl, 1));

        if (!closure->isC) {
            const std::uint32_t index = lua_tonumber(rl, 2) - 1;
            const auto constant = &closure->l.p->k[index];

            setobj(rl, constant, index2addr(rl, 3));
        }

        return 0;
    }

    inline int dbg_getproto(lua_State* L)
    {
		return 0; // return nil for now
    }

    inline int dbg_getprotos(lua_State* L)
    {
		return 0; // return nil for now
    }

    static int dbg_getinfo(lua_State* L)
    {
        if (lua_isfunction(L, 1) == false && lua_isnumber(L, 1) == false) {
            luaL_argerror(L, 1, "function or number expected");
            return 0;
        }

        intptr_t level{};
        if (lua_isfunction(L, 1))
            level = -lua_gettop(L);

        lua_Debug ar;
        if (!lua_getinfo(L, level, "sluanf", &ar))
            luaL_argerror(L, 1, "invalid level");

        lua_newtable(L);

        lua_pushvalue(L, 1);
        lua_setfield(L, -2, "func");

        lua_pushinteger(L, ar.nupvals);
        lua_setfield(L, -2, "nups");

        lua_pushstring(L, ar.source);
        lua_setfield(L, -2, "source");

        lua_pushstring(L, ar.short_src);
        lua_setfield(L, -2, "short_src");

        lua_pushinteger(L, ar.currentline);
        lua_setfield(L, -2, "currentline");

        lua_pushstring(L, ar.what);
        lua_setfield(L, -2, "what");

        lua_pushinteger(L, ar.linedefined);
        lua_setfield(L, -2, "linedefined");

        lua_pushinteger(L, ar.isvararg);
        lua_setfield(L, -2, "is_vararg");

        lua_pushinteger(L, ar.nparams);
        lua_setfield(L, -2, "numparams");

        lua_pushstring(L, ar.name);
        lua_setfield(L, -2, "name");

        if (lua_isfunction(L, 1) && lua_isLfunction(L, 1)) {
            Closure* cl = clvalue(luaA_toobject(L, 1));

            lua_pushinteger(L, cl->l.p->sizep);
            lua_setfield(L, -2, "sizep");
        }

        return 1;
    }

    inline int dbg_info(lua_State* L)
    {
        if (!lua_isfunction(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        const char* what = luaL_optstring(L, 2, "sln");

        Closure* cl = lua_toclosure(L, 1);
        if (!cl) {
            lua_pushnil(L);
            return 1;
        }

        lua_newtable(L);

        if (strchr(what, 'n')) {
            if (cl->isC) {
                lua_pushstring(L, cl->c.debugname ? cl->c.debugname : "");
            }
            else {
                Proto* p = cl->l.p;
                lua_pushstring(L, p && p->debugname ? getstr(p->debugname) : "");
            }
            lua_setfield(L, -2, "name");

            lua_pushinteger(L, cl->nupvalues);
            lua_setfield(L, -2, "nups");
        }

        if (strchr(what, 's')) {
            if (!cl->isC && cl->l.p) {
                Proto* p = cl->l.p;
                lua_pushstring(L, p->source ? getstr(p->source) : "");
                lua_setfield(L, -2, "source");

                lua_pushinteger(L, p->linedefined);
                lua_setfield(L, -2, "linedefined");
            }
            else {
                lua_pushstring(L, "[C]");
                lua_setfield(L, -2, "source");

                lua_pushinteger(L, -1);
                lua_setfield(L, -2, "linedefined");
            }
        }

        if (strchr(what, 'a')) {
            if (!cl->isC && cl->l.p) {
                Proto* p = cl->l.p;
                lua_pushinteger(L, p->numparams);
                lua_setfield(L, -2, "numparams");

                lua_pushboolean(L, p->is_vararg);
                lua_setfield(L, -2, "is_vararg");
            }
        }

        return 1;
    }

    static int dbg_getstack(lua_State* ls)
    {
        luaL_checkany(ls, 1);

        if (!(lua_isfunction(ls, 1) || lua_isnumber(ls, 1)))
        {
            luaL_argerror(ls, 1, "getstack error: function or number");
        }

        int level = 0;
        if (lua_isnumber(ls, 1))
        {
            level = lua_tointeger(ls, 1);

            if (level <= 0)
            {
                luaL_argerror(ls, 1, "getstack error: level out of range");
            }
        }
        else if (lua_isfunction(ls, 1))
        {
            level = -lua_gettop(ls);
        }

        lua_Debug ar;
        if (!lua_getinfo(ls, level, "f", &ar))
        {
            luaL_argerror(ls, 1, "getstack error: invalid level");
        }

        if (!lua_isfunction(ls, -1))
        {
            luaL_argerror(ls, 1, "getstack error: level does not point to a function");
        }

        if (lua_iscfunction(ls, -1))
        {
            luaL_argerror(ls, 1, "getstack error: luau function expected");
        }

        lua_pop(ls, 1);

        auto ci = ls->ci[-level];

        if (lua_isnumber(ls, 2))
        {
            const auto idx = lua_tointeger(ls, 2) - 1;

            if (idx >= cast_int(ci.top - ci.base) || idx < 0)
            {
                luaL_argerror(ls, 2, "getstack error: index out of range");
            }

            auto val = ci.base + idx;
            luaC_threadbarrier(ls) luaA_pushobject(ls, val);
        }
        else
        {
            int idx = 0;
            lua_newtable(ls);

            for (auto val = ci.base; val < ci.top; val++)
            {
                lua_pushinteger(ls, idx++ + 1);

                luaC_threadbarrier(ls) luaA_pushobject(ls, val);

                lua_settable(ls, -3);
            }
        }

        return 1;
    }

    static int dbg_setstack(lua_State* ls)
    {
        luaL_checkany(ls, 1);

        if (!(lua_isfunction(ls, 1) || lua_isnumber(ls, 1)))
        {
            luaL_argerror(ls, 1, ("setstack error: function or number"));
        }

        int level = 0;
        if (lua_isnumber(ls, 1))
        {
            level = lua_tointeger(ls, 1);

            if (level <= 0)
            {
                luaL_argerror(ls, 1, "setstack error: level out of range");
            }
        }
        else if (lua_isfunction(ls, 1))
        {
            level = -lua_gettop(ls);
        }

        lua_Debug ar;
        if (!lua_getinfo(ls, level, "f", &ar))
        {
            luaL_argerror(ls, 1, "setstac error: invalid level");
        }

        if (!lua_isfunction(ls, -1))
        {
            luaL_argerror(ls, 1, "setstack error: level does not point to a function");
        }

        if (lua_iscfunction(ls, -1))
        {
            luaL_argerror(ls, 1, "setstack error: luau function expected");
        }

        lua_pop(ls, 1);

        luaL_checkany(ls, 3);

        auto ci = ls->ci[-level];

        const auto idx = luaL_checkinteger(ls, 2) - 1;
        if (idx >= cast_int(ci.top - ci.base) || idx < 0)
        {
            luaL_argerror(ls, 2, "setstack error: index out of range");
        }

        if ((ci.base + idx)->tt != luaA_toobject(ls, 3)->tt)
        {
            luaL_argerror(ls, 3, "setstack error: new value type does not match previous value type");
        }

        setobj2s(ls, (ci.base + idx), luaA_toobject(ls, 3));
        return 0;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        lua_newtable(L);

        Utils::RegisterTableAliases(L, dbg_getupvalue, {
            "getupvalue", "getupval", "get_upvalue", "get_upval"
            });

        Utils::RegisterTableAliases(L, dbg_getupvalues, {
            "getupvalues", "getupvals", "get_upvalues", "get_upvals"
            });

        Utils::RegisterTableAliases(L, dbg_setupvalue, {
            "setupvalue", "setupval", "set_upvalue", "set_upval"
            });

        Utils::RegisterTableAliases(L, dbg_getconstant, {
            "getconstant", "getconst", "get_constant", "get_const"
            });

        Utils::RegisterTableAliases(L, dbg_getconstants, {
            "getconstants", "getconsts", "get_constants", "get_consts"
            });

        Utils::RegisterTableAliases(L, dbg_setconstant, {
            "setconstant", "setconst", "set_constant", "set_const"
            });

       Utils::RegisterTableAliases(L, dbg_getproto, {
            "getproto", "get_proto"
            });

     Utils::RegisterTableAliases(L, dbg_getprotos, {
          "getprotos", "get_protos"
          });

        Utils::RegisterTableAliases(L, dbg_getinfo, {
            "getinfo", "get_info"
            });

        Utils::RegisterTableAliases(L, dbg_info, {
            "info", "Info"
            });

        Utils::RegisterTableAliases(L, dbg_getstack, {
            "getstack", "get_stack"
            });

        Utils::RegisterTableAliases(L, dbg_setstack, {
            "setstack", "set_stack"
            });

        lua_setglobal(L, "debug");
    }
}