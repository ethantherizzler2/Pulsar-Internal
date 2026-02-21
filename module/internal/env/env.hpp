#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lgc.h>
#include <lualib.h>
#include <lobject.h>
#include <lapi.h>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <map>

#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <luau/compiler.h>

namespace Environment
{
    extern std::vector<Closure*> function_array;
    static std::map<Closure*, lua_CFunction> cfunction_map;
    static std::map<Closure*, Closure*> newcclosure_map;

    void SetupEnvironment(lua_State* L);
}