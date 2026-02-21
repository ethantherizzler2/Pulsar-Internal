#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lgc.h>
#include <lobject.h>
#include <lfunc.h>
#include <lstring.h>
#include <lapi.h>
#include <ltable.h>
#include <unordered_set>
#include <unordered_map>
#include <string>

#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <internal/execution/execution.hpp>
#include <internal/roblox/task_scheduler/scheduler.hpp>
#include <internal/roblox/update/helpers/luauhelper.hpp>
#include <internal/env/libs/metatable.hpp>
#include <regex>



namespace ClosureUtils
{
    static void handler_run(lua_State* L, void* ud) {
        luaD_call(L, (StkId)(ud), LUA_MULTRET);
    }

    std::string strip_error_message(const std::string& message) {
        static auto callstack_regex = std::regex(R"(.*"\]:(\d)*: )", std::regex::optimize | std::regex::icase);
        if (std::regex_search(message.begin(), message.end(), callstack_regex)) {
            const auto fixed = std::regex_replace(message, callstack_regex, "");
            return fixed;
        }

        return message;
    }

    int newcc_proxy(lua_State* L) {
        const auto closure = Environment::newcclosure_map.contains(clvalue(L->ci->func)) ? Environment::newcclosure_map.at(clvalue(L->ci->func)) : nullptr;

        if (!closure)
            luaL_error(L, oxorany("unable to find closure"));

        setclvalue(L, L->top, closure);
        L->top++;

        lua_insert(L, 1);

        StkId function = L->base;
        L->ci->flags |= LUA_CALLINFO_HANDLE;

        L->baseCcalls++;
        int status = luaD_pcall(L, handler_run, function, savestack(L, function), 0);
        L->baseCcalls--;

        if (status == LUA_ERRRUN) {
            const auto regexed_error = strip_error_message(lua_tostring(L, -1));
            lua_pop(L, 1);

            lua_pushlstring(L, regexed_error.c_str(), regexed_error.size());
            lua_error(L);
            return 0;
        }

        expandstacklimit(L, L->top);

        if (status == 0 && (L->status == LUA_YIELD || L->status == LUA_BREAK))
            return -1;

        return lua_gettop(L);
    };

    int newcc_continuation(lua_State* L, int Status) {
        if (Status != LUA_OK) {
            const auto regexed_error = strip_error_message(lua_tostring(L, -1));
            lua_pop(L, 1);

            lua_pushlstring(L, regexed_error.c_str(), regexed_error.size());
            lua_error(L);
        }

        return lua_gettop(L);
    };

    int wrap_closure(lua_State* L, int index) {
        luaL_checktype(L, index, LUA_TFUNCTION);

        lua_ref(L, index);
        lua_pushcclosurek(L, newcc_proxy, nullptr, 0, newcc_continuation);
        lua_ref(L, -1);

        Closure* catched = clvalue(index2addr(L, -1));
        Environment::function_array.push_back(catched);

        Environment::newcclosure_map[clvalue(luaA_toobject(L, -1))] = clvalue(luaA_toobject(L, index));

        return 1;
    };  
}

inline bool lua_isCclosure(lua_State* L, int idx)
{
    if (!lua_isfunction(L, idx)) {
        return false;
    }
    return lua_iscfunction(L, idx);
}

inline bool lua_isLclosure(lua_State* L, int idx)
{
    if (!lua_isfunction(L, idx)) {
        return false;
    }
    return !lua_iscfunction(L, idx);
}

enum ClosureType
{
    ROBLOX_C_CLOSURE,
    MODULE_C_CLOSURE,
    MODULE_C_WRAP,
    L_CLOSURE,
    TYPE_UNKNOWN
};

namespace Closures
{
    inline std::unordered_set<void*> executor_cclosures;
    inline std::unordered_set<void*> our_closures;
    inline std::unordered_map<void*, std::string> script_bytecodes;
    inline std::unordered_set<void*> executor_protos;
    inline std::unordered_map<void*, Closure*> newcclosure_map;
    inline std::unordered_map<Closure*, Closure*> hooked_functions;

    static ClosureType GetClosureType(Closure* cl)
    {
        if (!cl) return TYPE_UNKNOWN;

        if (!cl->isC)
            return L_CLOSURE;

        if (newcclosure_map.find((void*)cl) != newcclosure_map.end())
            return MODULE_C_WRAP;

        if (executor_cclosures.find((void*)cl->c.f) != executor_cclosures.end())
            return MODULE_C_CLOSURE;

        return ROBLOX_C_CLOSURE;
    }

    int newcclosure(lua_State* L)
    {
        lua_check(L, 1);
        luaL_checktype(L, 1, LUA_TFUNCTION);

        if (!lua_iscfunction(L, 1))
            return ClosureUtils::wrap_closure(L, 1);

        lua_pushnil(L);
        return 1;
    }

    inline int hookfunction(lua_State* L)
    {
        Roblox::Print(1, "Hookfunction has been disabled");
        return 0;
    }


    inline int restorefunction(lua_State* L)
    {
        if (!lua_isfunction(L, 1)) {
            lua_pushvalue(L, 1);
            return 1;
        }

        Closure* target = lua_toclosure(L, 1);
        if (!target) {
            lua_pushvalue(L, 1);
            return 1;
        }

        auto it = hooked_functions.find(target);
        if (it == hooked_functions.end()) {
            lua_pushvalue(L, 1);
            return 1;
        }

        Closure* original = it->second;
        if (!original) {
            lua_pushvalue(L, 1);
            return 1;
        }

        __try {
            if (target->isC && original->isC) {
                InterlockedExchangePointer((PVOID*)&target->c.f, (PVOID)original->c.f);
                target->c.cont = original->c.cont;
                target->c.debugname = original->c.debugname;
                target->env = original->env;

                for (int i = 0; i < target->nupvalues && i < original->nupvalues; i++) {
                    setobj(L, &target->c.upvals[i], &original->c.upvals[i]);
                }
            }
            else if (!target->isC && !original->isC) {
                if (original->l.p) {
                    InterlockedExchangePointer((PVOID*)&target->l.p, (PVOID)original->l.p);
                    target->env = original->env;

                    for (int i = 0; i < target->nupvalues && i < original->nupvalues; i++) {
                        setobj(L, &target->l.uprefs[i], &original->l.uprefs[i]);
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }

        hooked_functions.erase(it);

        lua_pushvalue(L, 1);
        return 1;
    }

    inline bool IsLuauBytecode(const char* data, size_t len)
    {
        if (len < 4) return false;

        if (len > 0 && (unsigned char)data[0] <= 10) {

            if (len > 8) {
                int nonPrintable = 0;
                for (size_t i = 0; i < (len < 100 ? len : 100); i++) {
                    unsigned char c = (unsigned char)data[i];
                    if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                        nonPrintable++;
                    }
                }

                if (nonPrintable > (len < 100 ? len : 100) / 5) {
                    return true;
                }
            }
        }

        return false;
    }

    inline int loadstring(lua_State* L)
    {
        if (!L || !lua_isstring(L, 1)) {
            lua_pushnil(L);
            lua_pushstring(L, oxorany("argument #1 must be a string"));
            return 2;
        }

        size_t source_len;
        const char* source = lua_tolstring(L, 1, &source_len);

        if (!source || source_len == 0) {
            lua_pushnil(L);
            lua_pushstring(L, oxorany("empty source"));
            return 2;
        }

        std::string source_str(source, source_len);
        std::string actual_code = source_str;

        if (source_str.find("http://") == 0 || source_str.find("https://") == 0) {
            actual_code = Http::HttpGetSync(source_str);

            if (actual_code.empty()) {
                lua_pushnil(L);
                lua_pushstring(L, oxorany("failed to download script from URL"));
                return 2;
            }
        }

        if (IsLuauBytecode(actual_code.c_str(), actual_code.size())) {
            lua_pushnil(L);
            lua_pushstring(L, oxorany("cannot load binary data :)"));
            return 2;
        }

        const char* chunk_name_raw = luaL_optstring(L, 2, "loadstring");
        std::string chunk_name = std::string("@") + chunk_name_raw; 

        std::string bytecode = Execution::CompileScript(actual_code);

        if (bytecode.empty()) {
            lua_pushnil(L);
            lua_pushstring(L, oxorany("compilation failed"));
            return 2;
        }

        int result = luau_load(L, chunk_name.c_str(), bytecode.data(), bytecode.size(), 0);

        if (result != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_pushnil(L);
            lua_pushstring(L, err ? err : oxorany("failed to load bytecode"));
            return 2;
        }

        Closure* func = lua_toclosure(L, -1);

        if (!func) {
            lua_pushnil(L);
            lua_pushstring(L, oxorany("failed to get closure"));
            return 2;
        }

        if (!func->isC && func->l.p) {
            TaskScheduler::SetProtoCapabilities(func->l.p, &MaxCapabilities);

            std::function<void(Proto*)> set_nested_caps = [&](Proto* p) {
                if (!p) return;

                TaskScheduler::SetProtoCapabilities(p, &MaxCapabilities);
                Closures::executor_protos.insert((void*)p);

                for (int i = 0; i < p->sizep; i++) {
                    if (p->p[i]) {
                        set_nested_caps(p->p[i]);
                    }
                }
                };

            set_nested_caps(func->l.p);
        }

        Closures::our_closures.insert((void*)func);
        Closures::script_bytecodes[(void*)func] = bytecode;

        lua_setsafeenv(L, -1, false);

        return 1;
    }


    auto is_executor_closure(lua_State* rl) -> int
    {
        if (lua_type(rl, 1) != LUA_TFUNCTION) { lua_pushboolean(rl, false); return 1; }

        Closure* closure = clvalue(index2addr(rl, 1));
        bool value = false;

        if (lua_isLfunction(rl, 1))
        {
            value = closure->l.p->linedefined;
        }
        else
        {
            for (int i = 0; i < Environment::function_array.size(); i++)
            {
                if (Environment::function_array[i]->c.f == closure->c.f)
                {
                    value = true;
                    break;
                }
            }
        }

        lua_pushboolean(rl, value);
        return 1;
    }

    inline int iscclosure(lua_State* L)
    {
        if (!L || !lua_isfunction(L, 1)) {
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_pushboolean(L, lua_iscfunction(L, 1) ? 1 : 0);
        return 1;
    }

    inline int islclosure(lua_State* L)
    {
        if (!L || !lua_isfunction(L, 1)) {
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_pushboolean(L, lua_iscfunction(L, 1) ? 0 : 1);
        return 1;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        Utils::AddFunction(L, "loadstring", loadstring);
        Utils::AddFunction(L, "isexecutorclosure", is_executor_closure);
        Utils::AddFunction(L, "isourclosure", is_executor_closure);
        Utils::AddFunction(L, "checkclosure", is_executor_closure);
        Utils::AddFunction(L, "iscclosure", iscclosure);
        Utils::AddFunction(L, "islclosure", islclosure);
        Utils::AddFunction(L, "newcclosure", newcclosure);
        Utils::AddFunction(L, "hookfunction", hookfunction);
        Utils::AddFunction(L, "hookfunc", hookfunction);
        Utils::AddFunction(L, "replaceclosure", hookfunction);
        Utils::AddFunction(L, "restorefunction", restorefunction);

        executor_cclosures.insert((void*)loadstring);
        executor_cclosures.insert((void*)is_executor_closure);
        executor_cclosures.insert((void*)iscclosure);
        executor_cclosures.insert((void*)islclosure);
        executor_cclosures.insert((void*)newcclosure);
        executor_cclosures.insert((void*)hookfunction);
        executor_cclosures.insert((void*)restorefunction);
    }
}