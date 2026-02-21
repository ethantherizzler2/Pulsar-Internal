#pragma once

#include <Windows.h>
#include <lstate.h>
#include <string>
#include <vector>
#include <lualib.h>
#include "globals.hpp"

namespace Environment {
	extern std::vector<Closure*> function_array;
}

namespace Utils
{
	inline void AddFunction(lua_State* L, const char* Name, lua_CFunction Function)
	{
		lua_pushcclosure(L, Function, nullptr, 0);
		Closure* closure = *reinterpret_cast<Closure**>(index2addr(L, -1));
		Environment::function_array.push_back(closure);
		lua_setglobal(L, Name);
	}

	inline void AddTableFunction(lua_State* L, const char* Name, lua_CFunction Function)
	{
		lua_pushcclosure(L, Function, nullptr, 0);
		Closure* closure = *reinterpret_cast<Closure**>(index2addr(L, -1));
		Environment::function_array.push_back(closure);
		lua_setfield(L, -2, Name);
	}

	inline void RegisterAliases(lua_State* L, lua_CFunction Function, const std::vector<const char*>& Aliases)
	{
		for (const char* alias : Aliases)
		{
			lua_pushcclosure(L, Function, nullptr, 0);
			Closure* closure = *reinterpret_cast<Closure**>(index2addr(L, -1));
			Environment::function_array.push_back(closure);
			lua_setglobal(L, alias);
		}
	}

	inline void RegisterTableAliases(lua_State* L, lua_CFunction Function, const std::vector<const char*>& Aliases)
	{
		for (const char* alias : Aliases)
		{
			lua_pushcclosure(L, Function, nullptr, 0);
			Closure* closure = *reinterpret_cast<Closure**>(index2addr(L, -1));
			Environment::function_array.push_back(closure);
			lua_setfield(L, -2, alias);
		}
	}

	inline bool IsInGame(uintptr_t DataModel)
	{
		uintptr_t GameLoaded = *reinterpret_cast<uintptr_t*>(DataModel + Offsets::DataModel::GameLoaded);
		if (GameLoaded != 31)
			return false;

		return true;
	}
}