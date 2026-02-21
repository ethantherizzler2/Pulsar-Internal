#pragma once
#include <lstate.h>
#include <lualib.h>
#include <lapi.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <internal/utils.hpp>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")


struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    int port = 443;
    bool secure = true;
};

static bool ParseWebSocketUrl(const std::string& url, ParsedUrl& out) {
    std::wstring wurl(url.begin(), url.end());

    bool secure = false;
    size_t pos = 0;

    if (wurl.rfind(L"wss://", 0) == 0) {
        secure = true;
        pos = 6;
    }
    else if (wurl.rfind(L"ws://", 0) == 0) {
        secure = false;
        pos = 5;
    }
    else {
        return false;
    }

    size_t slash = wurl.find(L'/', pos);
    std::wstring hostport = (slash == std::wstring::npos)
        ? wurl.substr(pos)
        : wurl.substr(pos, slash - pos);

    std::wstring path = (slash == std::wstring::npos)
        ? L"/"
        : wurl.substr(slash);

    int port = secure ? 443 : 80;

    size_t colon = hostport.find(L':');
    std::wstring host = hostport;
    if (colon != std::wstring::npos) {
        host = hostport.substr(0, colon);
        port = _wtoi(hostport.substr(colon + 1).c_str());
    }

    out.host = host;
    out.path = path;
    out.port = port;
    out.secure = secure;
    return true;
}

namespace Websocket {

    class exploit_websocket {
    public:
        lua_State* th = nullptr;
        bool connected = false;

        std::thread pollThread;
        std::atomic<bool> running = false;

        int onMessageRef = 0;
        int onCloseRef = 0;
        int threadRef = 0;

        // WinHTTP handles
        HINTERNET hSession = nullptr;
        HINTERNET hConnect = nullptr;
        HINTERNET hRequest = nullptr;
        HINTERNET hWebSocket = nullptr;

        bool connectWinHttp(const std::string& url) {
            ParsedUrl u{};
            if (!ParseWebSocketUrl(url, u)) {
                Logger::errorf("[WS] URL parse failed: %s", url.c_str());
                return false;
            }

            hSession = WinHttpOpen(L"WSClient/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0);

            if (!hSession) {
                Logger::errorf("[WS] WinHttpOpen failed: %lu", GetLastError());
                return false;
            }

            // Force TLS 1.2
            DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
            if (!WinHttpSetOption(hSession,
                WINHTTP_OPTION_SECURE_PROTOCOLS,
                &protocols,
                sizeof(protocols))) {
                Logger::errorf("[WS] Set TLS1.2 failed: %lu", GetLastError());
            }

            hConnect = WinHttpConnect(hSession, u.host.c_str(), u.port, 0);
            if (!hConnect) {
                Logger::errorf("[WS] WinHttpConnect failed: %lu", GetLastError());
                return false;
            }

            DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;

            hRequest = WinHttpOpenRequest(
                hConnect,
                L"GET",
                u.path.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags
            );

            if (!hRequest) {
                Logger::errorf("[WS] WinHttpOpenRequest failed: %lu", GetLastError());
                return false;
            }

            // 🔥 Ignore cert errors (TESTING ONLY)
            if (u.secure) {
                DWORD secFlags =
                    SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                    SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                    SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;

                if (!WinHttpSetOption(hRequest,
                    WINHTTP_OPTION_SECURITY_FLAGS,
                    &secFlags,
                    sizeof(secFlags))) {
                    Logger::errorf("[WS] Set SECURITY_FLAGS failed: %lu", GetLastError());
                }
            }

            if (!WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0,
                0, 0)) {
                Logger::errorf("[WS] WinHttpSendRequest failed: %lu", GetLastError());
                return false;
            }

            if (!WinHttpReceiveResponse(hRequest, nullptr)) {
                Logger::errorf("[WS] WinHttpReceiveResponse failed: %lu", GetLastError());
                return false;
            }

            hWebSocket = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
            if (!hWebSocket) {
            Logger::errorf("[WS] WebSocket upgrade failed: %lu", GetLastError());
                return false;
            }

        Logger::errorf("[WS] WebSocket connected OK (WinHTTP)");

            return true;
        }


        void pollMessages() {
            while (running) {
                if (!hWebSocket) {
                    fireClose();
                    break;
                }

                char buffer[4096];
                DWORD bytesRead = 0;
                WINHTTP_WEB_SOCKET_BUFFER_TYPE type;

                auto res = WinHttpWebSocketReceive(
                    hWebSocket,
                    buffer,
                    sizeof(buffer),
                    &bytesRead,
                    &type
                );

                if (res != NO_ERROR) {
                    fireClose();
                    break;
                }

                if (bytesRead > 0 &&
                    (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
                        type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE)) {

                    std::string msg(buffer, bytesRead);
                    fireMessage(msg);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        void fireMessage(const std::string& message) {
            if (!connected || !th) return;

            lua_getref(th, onMessageRef);
            lua_getfield(th, -1, "Fire");
            lua_getref(th, onMessageRef);
            lua_pushlstring(th, message.c_str(), message.size());

            if (lua_pcall(th, 2, 0, 0) != LUA_OK) {
                lua_settop(th, 0);
                return;
            }

            lua_settop(th, 0);
        }

        void fireClose() {
            if (!connected || !th) return;

            connected = false;
            running = false;

            if (hWebSocket) {
                WinHttpWebSocketClose(
                    hWebSocket,
                    WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                    nullptr, 0);
                hWebSocket = nullptr;
            }

            lua_getref(th, onCloseRef);
            lua_getfield(th, -1, "Fire");
            lua_getref(th, onCloseRef);
            if (lua_pcall(th, 1, 0, 0) != LUA_OK) {
                lua_settop(th, 0);
                return;
            }

            lua_settop(th, 0);

            lua_unref(th, onMessageRef);
            lua_unref(th, onCloseRef);
            lua_unref(th, threadRef);
        }

        void send(const std::string& data) {
            if (!hWebSocket) return;

            WinHttpWebSocketSend(
                hWebSocket,
                WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                (PVOID)data.data(),
                (DWORD)data.size()
            );
        }

        int handleIndex(lua_State* ls) {
            if (!ls || !connected) return 0;

            luaL_checktype(ls, 1, LUA_TUSERDATA);
            std::string idx = luaL_checkstring(ls, 2);

            if (idx == "OnMessage") {
                lua_getref(ls, onMessageRef);
                lua_getfield(ls, -1, "Event");
                return 1;
            }
            else if (idx == "OnClose") {
                lua_getref(ls, onCloseRef);
                lua_getfield(ls, -1, "Event");
                return 1;
            }
            else if (idx == "Send") {
                lua_pushvalue(ls, -10003);
                lua_pushcclosure(ls, [](lua_State* L) -> int {
                    exploit_websocket* ws =
                        reinterpret_cast<exploit_websocket*>(lua_touserdata(L, -10003));
                    std::string data = luaL_checkstring(L, 2);
                    if (ws) ws->send(data);
                    return 0;
                    }, "websocketinstance_send", 1);
                return 1;
            }
            else if (idx == "Close") {
                lua_pushvalue(ls, -10003);
                lua_pushcclosure(ls, [](lua_State* L) -> int {
                    exploit_websocket* ws =
                        reinterpret_cast<exploit_websocket*>(lua_touserdata(L, -10003));
                    if (ws) ws->fireClose();
                    return 0;
                    }, "websocketinstance_close", 1);
                return 1;
            }

            return 0;
        }
    };


    int connect(lua_State* ls) {
        luaL_checktype(ls, 1, LUA_TSTRING);
        std::string url = luaL_checkstring(ls, 1);

        auto* ws = (exploit_websocket*)lua_newuserdata(ls, sizeof(exploit_websocket));
        new (ws) exploit_websocket{};

        ws->th = lua_newthread(ls);
        ws->threadRef = lua_ref(ls, -1);
        lua_pop(ls, 1);

        if (!ws->connectWinHttp(url)) {
            lua_pushnil(ls);
            lua_pushstring(ls, "Failed to connect WebSocket (WinHTTP)");
            return 2;
        }

        lua_getglobal(ls, "Instance");
        lua_getfield(ls, -1, "new");
        lua_pushstring(ls, "BindableEvent");
        lua_pcall(ls, 1, 1, 0);
        ws->onMessageRef = lua_ref(ls, -1);
        lua_pop(ls, 2);

        lua_getglobal(ls, "Instance");
        lua_getfield(ls, -1, "new");
        lua_pushstring(ls, "BindableEvent");
        lua_pcall(ls, 1, 1, 0);
        ws->onCloseRef = lua_ref(ls, -1);
        lua_pop(ls, 2);

        ws->connected = true;
        ws->running = true;
        ws->pollThread = std::thread(&exploit_websocket::pollMessages, ws);

        lua_newtable(ls);
        lua_pushstring(ls, "WebSocket");
        lua_setfield(ls, -2, "__type");

        lua_pushvalue(ls, -2);
        lua_pushcclosure(ls, [](lua_State* L) -> int {
            auto* ws =
                reinterpret_cast<exploit_websocket*>(lua_touserdata(L, lua_upvalueindex(1)));
            return ws->handleIndex(L);
            }, "__index", 1);
        lua_setfield(ls, -2, "__index");
        lua_setmetatable(ls, -2);

        return 1;
    }

    void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        lua_newtable(L);

        Utils::RegisterTableAliases(L, connect, {
            "connect"
            });

        lua_setglobal(L, "WebSocket");
    }
}