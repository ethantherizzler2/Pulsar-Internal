#pragma once

#include <Windows.h>
#include <lstate.h>
#include <string>
#include <map>
#include <algorithm>

#include <cpr/cpr.h>
#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <dependencies/nlohmann/json.hpp>
#include <internal/env/yield/yield.hpp>

namespace Http
{
    enum RequestMethods
    {
        H_GET,
        H_HEAD,
        H_POST,
        H_PUT,
        H_DELETE,
        H_OPTIONS,
        H_PATCH
    };

    inline std::map<std::string, RequestMethods> RequestMethodMap = {
        { "get", H_GET },
        { "head", H_HEAD },
        { "post", H_POST },
        { "put", H_PUT },
        { "delete", H_DELETE },
        { "options", H_OPTIONS },
        { "patch", H_PATCH }
    };

    inline std::string GetStatusPhrase(int code)
    {
        switch (code)
        {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Unknown";
        }
    }

    inline std::string GetHWID()
    {
        HW_PROFILE_INFOA hwProfileInfo;
        if (GetCurrentHwProfileA(&hwProfileInfo))
            return hwProfileInfo.szHwProfileGuid;
        return "Unknown";
    }

    inline std::pair<std::string, std::string> GetGameInfo(lua_State* L)
    {
        std::string GameId = "0";
        std::string PlaceId = "0";

        if (!L) return { GameId, PlaceId };

        int top = lua_gettop(L);

        lua_getglobal(L, "game");
        if (lua_isuserdata(L, -1))
        {
            lua_getfield(L, -1, "GameId");
            if (lua_isstring(L, -1)) {
                GameId = lua_tostring(L, -1);
            }
            else if (lua_isnumber(L, -1)) {
                GameId = std::to_string(static_cast<int>(lua_tonumber(L, -1)));
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "PlaceId");
            if (lua_isstring(L, -1)) {
                PlaceId = lua_tostring(L, -1);
            }
            else if (lua_isnumber(L, -1)) {
                PlaceId = std::to_string(static_cast<int>(lua_tonumber(L, -1)));
            }
            lua_pop(L, 1);
        }

        lua_settop(L, top);

        return { GameId, PlaceId };
    }

    std::string HttpGetSync(const std::string& Url)
    {
        if (Url.find("http://") != 0 && Url.find("https://") != 0) {
            return "";
        }

        cpr::Header Headers;
        Headers.insert({ "User-Agent", "Eclipse" });
        Headers.insert({ "Accept", "*/*" });

        try {
            cpr::Response Result = cpr::Get(
                cpr::Url{ Url },
                Headers,
                cpr::Timeout{ 30000 },
                cpr::VerifySsl{ false }
            );

            if (Result.error.code != cpr::ErrorCode::OK) {
                return "";
            }

            return Result.text;
        }
        catch (...) {
            return "";
        }
    }

    int HttpGet(lua_State* L)
    {
        std::string Url;

        if (lua_isstring(L, 2)) {
            Url = lua_tostring(L, 2);
        }
        else if (lua_isstring(L, 1)) {
            Url = lua_tostring(L, 1);
        }
        else {
            lua_pushnil(L);
            lua_pushstring(L, "HttpGet: expected URL as string");
            return 2;
        }

        if (Url.find("http://") != 0 && Url.find("https://") != 0) {
            lua_pushnil(L);
            lua_pushstring(L, "HttpGet: invalid protocol (expected http:// or https://)");
            return 2;
        }

        auto [GameId, PlaceId] = GetGameInfo(L);
        std::string HWID = GetHWID();

        nlohmann::json SessionIdJson;
        SessionIdJson["GameId"] = GameId;
        SessionIdJson["PlaceId"] = PlaceId;

        cpr::Header Headers;
        Headers.insert({ "User-Agent", "Eclipse" });
        Headers.insert({ "Roblox-Session-Id", SessionIdJson.dump() });
        Headers.insert({ "Roblox-Place-Id", PlaceId });
        Headers.insert({ "Roblox-Game-Id", GameId });
        Headers.insert({ "Exploit-Guid", HWID });
        Headers.insert({ "Accept", "*/*" });

        return Yielding::YieldExecution(L, [Url, Headers]() -> std::function<int(lua_State*)>
            {
                cpr::Response Result;

                try {
                    Result = cpr::Get(
                        cpr::Url{ Url },
                        Headers,
                        cpr::Timeout{ 30000 },
                        cpr::VerifySsl{ false }
                    );
                }
                catch (const std::exception& ex) {
                    std::string ErrorString = std::string("HttpGet failed: ") + ex.what();
                    return [ErrorString](lua_State* L) -> int {
                        lua_pushnil(L);
                        lua_pushstring(L, ErrorString.c_str());
                        return 2;
                        };
                }
                catch (...) {
                    return [](lua_State* L) -> int {
                        lua_pushnil(L);
                        lua_pushstring(L, "HttpGet failed: unknown exception");
                        return 2;
                        };
                }

                return [Result](lua_State* L) -> int {
                    if (Result.error.code != cpr::ErrorCode::OK) {
                        lua_pushnil(L);
                        lua_pushstring(L, ("HttpGet failed: " + Result.error.message).c_str());
                        return 2;
                    }

                    if (Result.text.empty()) {
                        lua_pushnil(L);
                        lua_pushstring(L, "HttpGet: empty response");
                        return 2;
                    }

                    lua_pushlstring(L, Result.text.data(), Result.text.size());
                    return 1;
                    };
            });
    }

    int HttpPost(lua_State* L)
    {
        std::string Url;
        std::string Body;
        std::string ContentType = "application/json";

        if (lua_isstring(L, 1)) {
            Url = lua_tostring(L, 1);
        }
        else if (lua_isuserdata(L, 1)) {
            luaL_checkstring(L, 2);
            Url = lua_tostring(L, 2);
            if (lua_isstring(L, 3))
                Body = lua_tostring(L, 3);
            if (lua_isstring(L, 4))
                ContentType = lua_tostring(L, 4);
        }
        else {
            luaL_argerror(L, 1, "Invalid argument");
            return 0;
        }

        if (lua_isstring(L, 2) && lua_gettop(L) >= 2 && lua_isstring(L, 1)) {
            Body = lua_tostring(L, 2);
            if (lua_isstring(L, 3))
                ContentType = lua_tostring(L, 3);
        }

        if (Url.find("http://") != 0 && Url.find("https://") != 0) {
            luaL_argerror(L, 1, "Invalid protocol (expected 'http://' or 'https://')");
            return 0;
        }

        auto [GameId, PlaceId] = GetGameInfo(L);
        std::string HWID = GetHWID();

        nlohmann::json SessionIdJson;
        SessionIdJson["GameId"] = GameId;
        SessionIdJson["PlaceId"] = PlaceId;

        cpr::Header Headers;
        Headers.insert({ "User-Agent", "Eclipse" });
        Headers.insert({ "Content-Type", ContentType });
        Headers.insert({ "Roblox-Session-Id", SessionIdJson.dump() });
        Headers.insert({ "Accept", "*/*" });

        return Yielding::YieldExecution(L, [Url, Body, Headers]() -> std::function<int(lua_State*)>
            {
                cpr::Response Result;
                try {
                    Result = cpr::Post(cpr::Url{ Url }, cpr::Body{ Body }, Headers, cpr::Timeout{ 30000 });
                }
                catch (const std::exception& ex) {
                    std::string ErrorString = std::string("HttpPost failed: ") + ex.what();
                    return [ErrorString](lua_State* L) -> int {
                        luaL_error(L, ErrorString.c_str());
                        return 0;
                        };
                }

                return [Result](lua_State* L) -> int {
                    if (Result.error.code != cpr::ErrorCode::OK) {
                        std::string ErrorString = "HttpPost failed: " + Result.error.message;
                        luaL_error(L, ErrorString.c_str());
                        return 0;
                    }

                    lua_pushlstring(L, Result.text.data(), Result.text.size());
                    return 1;
                    };
            });
    }

    int request(lua_State* L)
    {
        if (!L) {
            lua_newtable(L);
            lua_pushboolean(L, 0);
            lua_setfield(L, -2, "Success");
            return 1;
        }

        luaL_checktype(L, 1, LUA_TTABLE);

        lua_getfield(L, 1, "Url");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);

            lua_newtable(L);
            lua_pushboolean(L, 0);
            lua_setfield(L, -2, "Success");
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "StatusCode");
            lua_pushstring(L, "Missing Url");
            lua_setfield(L, -2, "StatusMessage");
            lua_pushstring(L, "");
            lua_setfield(L, -2, "Body");
            lua_newtable(L);
            lua_setfield(L, -2, "Headers");
            lua_newtable(L);
            lua_setfield(L, -2, "Cookies");
            return 1;
        }
        std::string Url = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (Url.find("http://") != 0 && Url.find("https://") != 0) {
            lua_newtable(L);
            lua_pushboolean(L, 0);
            lua_setfield(L, -2, "Success");
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "StatusCode");
            lua_pushstring(L, "Invalid protocol");
            lua_setfield(L, -2, "StatusMessage");
            lua_pushstring(L, "");
            lua_setfield(L, -2, "Body");
            lua_newtable(L);
            lua_setfield(L, -2, "Headers");
            lua_newtable(L);
            lua_setfield(L, -2, "Cookies");
            return 1;
        }

        RequestMethods Method = H_GET;
        lua_getfield(L, 1, "Method");
        if (lua_isstring(L, -1)) {
            std::string MethodStr = lua_tostring(L, -1);
            std::transform(MethodStr.begin(), MethodStr.end(), MethodStr.begin(), ::tolower);
            auto it = RequestMethodMap.find(MethodStr);
            if (it != RequestMethodMap.end()) {
                Method = it->second;
            }
        }
        lua_pop(L, 1);

        cpr::Header Headers;
        lua_getfield(L, 1, "Headers");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                    std::string Key = lua_tostring(L, -2);
                    if (_stricmp(Key.c_str(), "Content-Length") != 0) {
                        Headers[Key] = lua_tostring(L, -1);
                    }
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        cpr::Cookies Cookies;
        lua_getfield(L, 1, "Cookies");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                    Cookies[lua_tostring(L, -2)] = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        std::string Body;
        lua_getfield(L, 1, "Body");
        if (lua_isstring(L, -1)) {
            Body = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        auto [GameId, PlaceId] = GetGameInfo(L);
        std::string HWID = GetHWID();

        nlohmann::json SessionIdJson;
        SessionIdJson["GameId"] = GameId;
        SessionIdJson["PlaceId"] = PlaceId;

        if (Headers.find("User-Agent") == Headers.end())
            Headers["User-Agent"] = "Eclipse";
        if (Headers.find("Roblox-Session-Id") == Headers.end())
            Headers["Roblox-Session-Id"] = SessionIdJson.dump();
        if (Headers.find("Fingerprint") == Headers.end())
            Headers["Fingerprint"] = HWID;

        return Yielding::YieldExecution(L, [=]() -> std::function<int(lua_State*)>
            {
                cpr::Response Response;
                std::string error_msg;
                bool has_error = false;

                try {
                    cpr::Timeout timeout{ 750 };

                    switch (Method) {
                    case H_GET:
                        Response = cpr::Get(cpr::Url{ Url }, Cookies, Headers, timeout);
                        break;
                    case H_HEAD:
                        Response = cpr::Head(cpr::Url{ Url }, Cookies, Headers, timeout);
                        break;
                    case H_POST:
                        Response = cpr::Post(cpr::Url{ Url }, cpr::Body{ Body }, Cookies, Headers, timeout);
                        break;
                    case H_PUT:
                        Response = cpr::Put(cpr::Url{ Url }, cpr::Body{ Body }, Cookies, Headers, timeout);
                        break;
                    case H_DELETE:
                        Response = cpr::Delete(cpr::Url{ Url }, cpr::Body{ Body }, Cookies, Headers, timeout);
                        break;
                    case H_OPTIONS:
                        Response = cpr::Options(cpr::Url{ Url }, cpr::Body{ Body }, Cookies, Headers, timeout);
                        break;
                    case H_PATCH:
                        Response = cpr::Patch(cpr::Url{ Url }, cpr::Body{ Body }, Cookies, Headers, timeout);
                        break;
                    }
                }
                catch (const std::exception& ex) {
                    has_error = true;
                    error_msg = ex.what();
                }
                catch (...) {
                    has_error = true;
                    error_msg = "Unknown error";
                }

                return [Response, has_error, error_msg](lua_State* L) -> int {
                    lua_newtable(L);

                    if (has_error) {
                        lua_pushboolean(L, 0);
                        lua_setfield(L, -2, "Success");
                        lua_pushinteger(L, 0);
                        lua_setfield(L, -2, "StatusCode");
                        lua_pushstring(L, error_msg.c_str());
                        lua_setfield(L, -2, "StatusMessage");
                        lua_pushstring(L, "");
                        lua_setfield(L, -2, "Body");
                        lua_newtable(L);
                        lua_setfield(L, -2, "Headers");
                        lua_newtable(L);
                        lua_setfield(L, -2, "Cookies");
                        return 1;
                    }

                    lua_pushboolean(L, Response.status_code >= 200 && Response.status_code < 300 ? 1 : 0);
                    lua_setfield(L, -2, "Success");

                    lua_pushinteger(L, Response.status_code);
                    lua_setfield(L, -2, "StatusCode");

                    lua_pushstring(L, GetStatusPhrase(Response.status_code).c_str());
                    lua_setfield(L, -2, "StatusMessage");

                    lua_newtable(L);
                    for (const auto& Header : Response.header) {
                        lua_pushstring(L, Header.second.c_str());
                        lua_setfield(L, -2, Header.first.c_str());
                    }
                    lua_setfield(L, -2, "Headers");

                    lua_newtable(L);
                    for (const auto& Cookie : Response.cookies.map_) {
                        lua_pushstring(L, Cookie.second.c_str());
                        lua_setfield(L, -2, Cookie.first.c_str());
                    }
                    lua_setfield(L, -2, "Cookies");

                    lua_pushlstring(L, Response.text.data(), Response.text.size());
                    lua_setfield(L, -2, "Body");

                    return 1;
                    };
            });
    }

    int GetObjects(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TSTRING);

        lua_getglobal(L, "game");
        lua_getfield(L, -1, "GetService");
        lua_pushvalue(L, -2);
        lua_pushstring(L, "InsertService");
        lua_call(L, 2, 1);
        lua_remove(L, -2);

        lua_getfield(L, -1, "LoadLocalAsset");

        lua_pushvalue(L, -2);
        lua_pushvalue(L, 2);
        lua_pcall(L, 2, 1, 0);

        if (lua_type(L, -1) == LUA_TSTRING)
            luaL_error(L, lua_tostring(L, -1));

        lua_createtable(L, 1, 0);
        lua_pushvalue(L, -2);
        lua_rawseti(L, -2, 1);

        lua_remove(L, -3);
        lua_remove(L, -2);

        return 1;
    }

    void RegisterLibrary(lua_State* L)
    {
        Utils::AddFunction(L, "request", request);
        Utils::AddFunction(L, "http_request", request);
        Utils::AddFunction(L, "syn_request", request);

        lua_newtable(L);
        Utils::AddTableFunction(L, "request", request);
        lua_setglobal(L, "http");
    }
}