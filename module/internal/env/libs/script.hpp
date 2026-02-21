#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lgc.h>
#include <lualib.h>
#include <string>
#include <sstream>
#include <map>
#include <unordered_set>
#include <dependencies/zstd/include/zstd/zstd.h>
#include <dependencies/zstd/include/zstd/xxhash.h>
#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <internal/env/libs/filesys.hpp>
#include <luacode.h>
#include "signals.hpp"
#include "cache.hpp"
#include <internal/env/helpers/bytecode/bytecode.hpp>
#include <wininet.h>
#include "http.hpp"
#include <internal/roblox/task_scheduler/scheduler.hpp>
#pragma comment(lib, "wininet.lib")
typedef void(__fastcall* PushInstanceWeakPtrT)(lua_State* L, std::weak_ptr<uintptr_t>);

PushInstanceWeakPtrT PushInstanceWeakPtr = (PushInstanceWeakPtrT)Offsets::PushInstance;

namespace Script
{
    inline std::map<void*, std::string> ScriptHashes;

    bool IsValidScriptClass(const char* className)
    {
        if (!className) return false;
        return strcmp(className, "LocalScript") == 0 ||
            strcmp(className, "ModuleScript") == 0 ||
            strcmp(className, "Script") == 0;
    }

    inline std::string Blake2Hash(const char* data, size_t len)
    {
        if (!data || len == 0) return std::string(96, '0');

        uint64_t h[8] = {
            0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
            0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
            0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
            0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
        };

        for (size_t i = 0; i < len; i++) {
            uint64_t val = static_cast<unsigned char>(data[i]);
            h[i % 8] ^= val;
            h[i % 8] = (h[i % 8] << 7) | (h[i % 8] >> 57);
            h[(i + 1) % 8] += val;
        }

        for (int round = 0; round < 12; round++) {
            for (int i = 0; i < 8; i++) {
                h[i] ^= h[(i + 1) % 8];
                h[i] = (h[i] << 13) | (h[i] >> 51);
            }
        }

        char output[97];
        for (int i = 0; i < 6; i++) {
            sprintf_s(output + (i * 16), 17, "%016llx", h[i]);
        }
        output[96] = '\0';

        return std::string(output);
    }

    inline int getscripthash(lua_State* L)
    {
        if (!L || !lua_isuserdata(L, 1)) {
            lua_pushstring(L, std::string(96, '0').c_str());
            return 1;
        }

        int top = lua_gettop(L);

        lua_getfield(L, 1, "ClassName");
        if (!lua_isstring(L, -1)) {
            lua_settop(L, top);
            lua_pushstring(L, std::string(96, '0').c_str());
            return 1;
        }

        const char* cn = lua_tostring(L, -1);
        if (!IsValidScriptClass(cn)) {
            lua_settop(L, top);
            lua_pushstring(L, std::string(96, '0').c_str());
            return 1;
        }

        lua_settop(L, top);

        std::string hashData;

        lua_getfield(L, 1, "Name");
        if (lua_isstring(L, -1)) {
            const char* name = lua_tostring(L, -1);
            if (name) hashData += name;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "Source");
        if (lua_isstring(L, -1)) {
            size_t len;
            const char* source = lua_tolstring(L, -1, &len);
            if (source && len > 0) {
                hashData.append(source, len);
            }
        }
        lua_pop(L, 1);

        void* ptr = *(void**)lua_touserdata(L, 1);
        if (ptr) {
            char ptrStr[32];
            sprintf_s(ptrStr, sizeof(ptrStr), "%p", ptr);
            hashData += ptrStr;
        }

        std::string hash = Blake2Hash(hashData.c_str(), hashData.length());

        if (ptr) {
            ScriptHashes[ptr] = hash;
        }

        lua_pushstring(L, hash.c_str());
        return 1;
    }
    
    int getscriptbytecode(lua_State* L)
    {
        if (!lua_isuserdata(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        std::string ud_t = luaL_typename(L, 1);
        if (ud_t != "Instance") {
            lua_pushnil(L);
            return 1;
        }

        auto script = *reinterpret_cast<std::uintptr_t*>(lua_touserdata(L, 1));
        if (!script) {
            lua_pushnil(L);
            return 1;
        }

        const char* className = *reinterpret_cast<const char**>(
            *reinterpret_cast<uintptr_t*>(script + Offsets::DataModel::ClassDescriptor) +
            Offsets::DataModel::ClassName
            );

        if (!className) {
            lua_pushnil(L);
            return 1;
        }

        std::uintptr_t protected_string = 0;

        if (strcmp(className, "LocalScript") == 0 || strcmp(className, "Script") == 0) {
            protected_string = *reinterpret_cast<std::uintptr_t*>(script + Offsets::Scripts::LocalScriptByteCode);
        }
        else if (strcmp(className, "ModuleScript") == 0) {
            protected_string = *reinterpret_cast<std::uintptr_t*>(script + Offsets::Scripts::ModuleScriptByteCode);
        }
        else {
            lua_pushnil(L);
            return 1;
        }

        if (!protected_string || IsBadReadPtr(reinterpret_cast<const void*>(protected_string), 0x30)) {
            lua_pushnil(L);
            return 1;
        }

        char* data_ptr = *reinterpret_cast<char**>(protected_string + 0x10);
        size_t data_size = *reinterpret_cast<size_t*>(protected_string + 0x20);

        if (data_size <= 15) {
            data_ptr = reinterpret_cast<char*>(protected_string + 0x10);
        }

        if (!data_ptr || data_size == 0 || data_size > 0x10000000) {
            lua_pushnil(L);
            return 1;
        }

        if (IsBadReadPtr(data_ptr, data_size)) {
            lua_pushnil(L);
            return 1;
        }

        std::string compressed_bytecode(data_ptr, data_size);

        if (compressed_bytecode.empty() || compressed_bytecode.size() < 8) {
            lua_pushnil(L);
            return 1;
        }

        std::string decompressed = Bytecode::decompress_bytecode(compressed_bytecode);

        if (decompressed.empty()) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushlstring(L, decompressed.data(), decompressed.size());
        return 1;
    }


    int decompile(lua_State* L) {
        return 0;
    }

    int getcallingscript(lua_State* L) {
        uintptr_t userdata = (uintptr_t)L->userdata;
        __int64 scriptptr = *reinterpret_cast<uintptr_t*>(userdata + 0x50); // if doesnt work try x60

        if (scriptptr > 0)
            Roblox::PushInstancescript(L, reinterpret_cast<__int64*>(userdata + 0x50)); same ehre
        else
            lua_pushnil(L);

        return 1;
    }

    int getrunningscripts(lua_State* L) {
        int threadCount = 0;
        lua_pushvalue(L, LUA_REGISTRYINDEX);
        lua_pushnil(L);
        while (lua_next(L, -2) && threadCount < 1000) {
            if (lua_isthread(L, -1)) {
                threadCount++;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        std::unordered_map<uintptr_t, bool> map;
        map.reserve(threadCount > 0 ? threadCount * 2 : 64);

        lua_pushvalue(L, LUA_REGISTRYINDEX);

        lua_newtable(L);

        lua_pushnil(L);

        unsigned int c = 0u;
        while (lua_next(L, -3))
        {
            if (lua_isthread(L, -1))
            {
                const auto thread = lua_tothread(L, -1);

                if (thread)
                {
                    if (const auto script_ptr = reinterpret_cast<std::uintptr_t>(thread->userdata) + 0x50; *reinterpret_cast<std::uintptr_t*>(script_ptr))
                    {
                        if (map.find(*(uintptr_t*)script_ptr) == map.end())
                        {
                            map.insert({ *(uintptr_t*)script_ptr, true });

                            Roblox::PushInstancescript(L, (void*)script_ptr);

                            lua_rawseti(L, -4, ++c);
                        }
                    }
                }
            }

            lua_pop(L, 1);
        }

        return 1;
    }

    inline int getscriptclosure(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        const auto scriptPtr = *(uintptr_t*)lua_topointer(L, 1);
        std::string scriptCode = Roblox::RequestBytecode(scriptPtr);

        lua_State* L2 = lua_newthread(L);
        luaL_sandboxthread(L2);

        uintptr_t Userdata = (uintptr_t)L2->userdata;
        if (!Userdata) {
            Roblox::Print(3, "Invalid userdata in thread");
            lua_gc(L, LUA_GCRESTART, 0);
            return 0;
        }

        uintptr_t identityPtr = Userdata + Offsets::ExtraSpace::Identity;
        uintptr_t capabilitiesPtr = Userdata + Offsets::ExtraSpace::Capabilities;

        if (identityPtr && capabilitiesPtr) {
            *(uintptr_t*)identityPtr = 8;
            *(int64_t*)capabilitiesPtr = ~0ULL;
        }
        else {
            Roblox::Print(3, "Invalid memory addresses for identity or capabilities");
            lua_gc(L, LUA_GCRESTART, 0);
            return 0;
        }

        lua_pushvalue(L, 1);
        lua_xmove(L, L2, 1);
        lua_setglobal(L2, "script");

        int result = Roblox::LuaVM__Load(L2, &scriptCode, "", 0);

        if (result == LUA_OK) {
            Closure* cl = clvalue(luaA_toobject(L2, -1));

            if (cl) {
                Proto* p = cl->l.p;
                if (p) {
                    uintptr_t maxCaps = ~0ULL;

                    TaskScheduler::SetProtoCapabilities(p, &maxCaps);
                }
            }

            lua_pop(L2, lua_gettop(L2));
            lua_pop(L, lua_gettop(L));

            setclvalue(L, L->top, cl);
            incr_top(L);

            return 1;
        }

        lua_pushnil(L);
        return 1;
    }

    int getscriptfromthread(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TTHREAD);

        lua_State* thread = lua_tothread(L, 1);

        if (!thread || !thread->userdata || thread->userdata->Script.expired()) {
            lua_pushnil(L);
            return 1;
        }

        auto script = thread->userdata->Script.lock();
        PushInstanceWeakPtr(L, script);

        return 1;
    }

    __forceinline int getscripts(lua_State* L) {
        struct instancecontext {
            lua_State* L;
            __int64 n;
        } Context = { L, 0 };

        lua_createtable(L, 0, 0);

        const auto ullOldThreshold = L->global->GCthreshold;
        L->global->GCthreshold = SIZE_MAX;

        luaM_visitgco(L, &Context, [](void* ctx, lua_Page* page, GCObject* gco) -> bool {
            auto gCtx = static_cast<instancecontext*>(ctx);
            const auto type = gco->gch.tt;

            if (isdead(gCtx->L->global, gco))
                return false;

            if (type == LUA_TUSERDATA) {

                TValue* top = gCtx->L->top;
                top->value.p = reinterpret_cast<void*>(gco);
                top->tt = type;
                gCtx->L->top++;

                if (!strcmp(luaL_typename(gCtx->L, -1), "Instance")) {
                    lua_getfield(gCtx->L, -1, "ClassName");

                    const char* inst_class = lua_tolstring(gCtx->L, -1, 0);
                    if (!strcmp(inst_class, "LocalScript") || !strcmp(inst_class, "ModuleScript") ||
                        !strcmp(inst_class, "CoreScript") || !strcmp(inst_class, "Script")) {
                        lua_pop(gCtx->L, 1);
                        gCtx->n++;
                        lua_rawseti(gCtx->L, -2, gCtx->n);
                    }
                    else
                        lua_pop(gCtx->L, 2);

                }
                else {
                    lua_pop(gCtx->L, 1);
                }
            }

            return true;
            });

        L->global->GCthreshold = ullOldThreshold;

        return 1;
    }

    int getloadedmodules1(lua_State* L)
    {
        lua_newtable(L);

        typedef struct {
            lua_State* pLua;
            int itemsFound;
            std::map< uintptr_t, bool > map;
        } GCOContext;

        auto gcCtx = GCOContext{ L, 0 };

        const auto ullOldThreshold = L->global->GCthreshold;
        L->global->GCthreshold = SIZE_MAX;

        luaM_visitgco(L, &gcCtx, [](void* ctx, lua_Page* pPage, GCObject* pGcObj) -> bool {
            const auto pCtx = static_cast<GCOContext*>(ctx);
            const auto ctxL = pCtx->pLua;

            if (isdead(ctxL->global, pGcObj))
                return false;

            if (const auto gcObjType = pGcObj->gch.tt;
                gcObjType == LUA_TFUNCTION) {
                ctxL->top->value.gc = pGcObj;
                ctxL->top->tt = gcObjType;
                ctxL->top++;

                lua_getfenv(ctxL, -1);

                if (!lua_isnil(ctxL, -1)) {
                    lua_getfield(ctxL, -1, "script");

                    if (!lua_isnil(ctxL, -1)) {
                        uintptr_t script_addr = *(uintptr_t*)lua_touserdata(ctxL, -1);

                        std::string class_name = **(std::string**)(*(uintptr_t*)(script_addr + 0x18) + 0x8);

                        if (pCtx->map.find(script_addr) == pCtx->map.end() && class_name == "ModuleScript") {
                            pCtx->map.insert({ script_addr, true });
                            lua_rawseti(ctxL, -4, ++pCtx->itemsFound);
                        }
                        else {
                            lua_pop(ctxL, 1);
                        }
                    }
                    else {
                        lua_pop(ctxL, 1);
                    }
                }

                lua_pop(ctxL, 2);
            }
            return false;
            });

        L->global->GCthreshold = ullOldThreshold;

        return 1;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;
        Utils::AddFunction(L, "getscripthash", getscripthash);
        Utils::AddFunction(L, "getscriptbytecode", getscriptbytecode);
		Utils::AddFunction(L, "getscriptclosure", getscriptclosure);
		Utils::AddFunction(L, "getcallingscript", getcallingscript);
		Utils::AddFunction(L, "getrunningscripts", getrunningscripts);
        Utils::AddFunction(L, "decompile", decompile);
        Utils::AddFunction(L, "dumpstring", getscriptbytecode);
        Utils::AddFunction(L, "getscriptfromthread", getscriptfromthread);
        Utils::AddFunction(L, "getscripts", getscripts);
        Utils::AddFunction(L, "getloadedmodules", getloadedmodules1);
    }

}
