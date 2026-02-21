#include <internal/env/env.hpp>

#include "libs/signals.hpp"
#include "libs/debug.hpp"
#include "libs/cache.hpp"
#include "libs/http.hpp"
#include "libs/closures.hpp"
#include "libs/crypt.hpp"
#include "libs/filesys.hpp"
#include "libs/miscellaneous.hpp"
#include "libs/instance.hpp"
#include "libs/script.hpp"
#include "libs/system.hpp"
#include "libs/metatable.hpp"
#include "libs/console.hpp"
#include "libs/websocket.hpp"
#include <../internal/render/overlay.hpp>
#include <internal/globals.hpp>

#include <internal/roblox/task_scheduler/scheduler.hpp>

namespace Environment {
    std::vector<Closure*> function_array;
}

lua_CFunction OriginalIndex;
lua_CFunction OriginalNamecall;

std::vector<const char*> UnsafeFunction = {
    "TestService.Run", "TestService", "Run",
    "OpenVideosFolder", "OpenScreenshotsFolder", "GetRobuxBalance", "PerformPurchase",
    "PromptBundlePurchase", "PromptNativePurchase", "PromptProductPurchase", "PromptPurchase",
    "PromptThirdPartyPurchase", "Publish", "GetMessageId", "OpenBrowserWindow", "RequestInternal",
    "ExecuteJavaScript", "ToggleRecording", "TakeScreenshot", "HttpRequestAsync", "GetLast",
    "SendCommand", "GetAsync", "GetAsyncFullUrl", "RequestAsync", "MakeRequest",
    "AddCoreScriptLocal", "SaveScriptProfilingData", "GetUserSubscriptionDetailsInternalAsync",
    "GetUserSubscriptionStatusAsync", "PerformBulkPurchase", "PerformCancelSubscription",
    "PerformPurchaseV2", "PerformSubscriptionPurchase", "PerformSubscriptionPurchaseV2",
    "PrepareCollectiblesPurchase", "PromptBulkPurchase", "PromptCancelSubscription",
    "PromptCollectiblesPurchase", "PromptGamePassPurchase", "PromptNativePurchaseWithLocalPlayer",
    "PromptPremiumPurchase", "PromptRobloxPurchase", "PromptSubscriptionPurchase",
    "ReportAbuse", "ReportAbuseV3", "ReturnToJavaScript", "OpenNativeOverlay",
    "OpenWeChatAuthWindow", "EmitHybridEvent", "OpenUrl", "PostAsync", "PostAsyncFullUrl",
    "RequestLimitedAsync", "Load", "CaptureScreenshot", "CreatePostAsync", "DeleteCapture",
    "DeleteCapturesAsync", "GetCaptureFilePathAsync", "SaveCaptureToExternalStorage",
    "SaveCapturesToExternalStorageAsync", "GetCaptureUploadDataAsync", "RetrieveCaptures",
    "SaveScreenshotCapture", "Call", "GetProtocolMethodRequestMessageId",
    "GetProtocolMethodResponseMessageId", "PublishProtocolMethodRequest",
    "PublishProtocolMethodResponse", "Subscribe", "SubscribeToProtocolMethodRequest",
    "SubscribeToProtocolMethodResponse", "GetDeviceIntegrityToken", "GetDeviceIntegrityTokenYield",
    "NoPromptCreateOutfit", "NoPromptDeleteOutfit", "NoPromptRenameOutfit", "NoPromptSaveAvatar",
    "NoPromptSaveAvatarThumbnailCustomization", "NoPromptSetFavorite", "NoPromptUpdateOutfit",
    "PerformCreateOutfitWithDescription", "PerformRenameOutfit", "PerformSaveAvatarWithDescription",
    "PerformSetFavorite", "PerformUpdateOutfit", "PromptCreateOutfit", "PromptDeleteOutfit",
    "PromptRenameOutfit", "PromptSaveAvatar", "PromptSetFavorite", "PromptUpdateOutfit"
};

int IndexHook(lua_State* L)
{
    if (L->userdata->Capabilities == MaxCapabilities)
    {
        std::string Key = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
        for (const char* Function : UnsafeFunction)
        {
            if (Key == Function)
            {
				Logger::warn("> malicious function has been blocked");
                Roblox::Print(3, "malicious function has been blocked");
                return 0;
            }
        }

        if (L->userdata->Script.expired())
        {
            if (Key == "HttpGet" || Key == "HttpGetAsync")
            {
                lua_pushcclosure(L, Http::HttpGet, nullptr, 0);
                return 1;
            }
            else if (Key == "GetObjects")
            {
                lua_pushcclosure(L, Http::GetObjects, nullptr, 0);
                return 1;
            }
        }
    }

    return OriginalIndex(L);
}

int NamecallHook(lua_State* L)
{
    if (L->userdata->Capabilities == MaxCapabilities)
    {
        std::string Key = L->namecall->data;
        for (const char* Function : UnsafeFunction)
        {
            if (Key == Function)
            {
                Roblox::Print(3, "malicious function has been blocked");
                return 0;
            }
        }

        if (L->userdata->Script.expired())
        {
            if (Key == "HttpGet" || Key == "HttpGetAsync")
            {
                return Http::HttpGet(L);
            }
            else if (Key == "GetObjects")
            {
                return Http::GetObjects(L);
            }
        }
    }

    return OriginalNamecall(L);
}

void InitializeHooks(lua_State* L)
{
    int OriginalTop = lua_gettop(L);

    lua_getglobal(L, "game");
    luaL_getmetafield(L, -1, "__index");
    if (lua_type(L, -1) == LUA_TFUNCTION || lua_type(L, -1) == LUA_TLIGHTUSERDATA)
    {
        Closure* IndexClosure = clvalue(luaA_toobject(L, -1));
        OriginalIndex = IndexClosure->c.f;
        IndexClosure->c.f = IndexHook;
    }
    lua_pop(L, 1);

    luaL_getmetafield(L, -1, "__namecall");
    if (lua_type(L, -1) == LUA_TFUNCTION || lua_type(L, -1) == LUA_TLIGHTUSERDATA)
    {
        Closure* NamecallClosure = clvalue(luaA_toobject(L, -1));
        OriginalNamecall = NamecallClosure->c.f;
        NamecallClosure->c.f = NamecallHook;
    }
    lua_pop(L, 1);

    lua_settop(L, OriginalTop);
}


static void SafeRegisterLibrary(lua_State* L, void(*RegisterFunc)(lua_State*))
{
    if (!L || !RegisterFunc) return;

    __try {
        RegisterFunc(L);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void Environment::SetupEnvironment(lua_State* L)
{
    if (!L) return;

    luaL_sandboxthread(L);

    SafeRegisterLibrary(L, Cache::RegisterLibrary);        Logger::info("> Registered Cache library");
    SafeRegisterLibrary(L, Closures::RegisterLibrary);     Logger::info("> Registered Closures library");
    SafeRegisterLibrary(L, ConsoleLib::RegisterLibrary);   Logger::info("> Registered ConsoleLib library");
    SafeRegisterLibrary(L, Crypt::RegisterLibrary);        Logger::info("> Registered Crypt library");
    SafeRegisterLibrary(L, Filesystem::RegisterLibrary);   Logger::info("> Registered Filesystem library");
    SafeRegisterLibrary(L, Http::RegisterLibrary);         Logger::info("> Registered Http library");
    SafeRegisterLibrary(L, Instance::RegisterLibrary);     Logger::info("> Registered Instance library");
    SafeRegisterLibrary(L, Miscellaneous::RegisterLibrary);Logger::info("> Registered Miscellaneous library");
    SafeRegisterLibrary(L, Script::RegisterLibrary);       Logger::info("> Registered Script library");
    SafeRegisterLibrary(L, System::RegisterLibrary);       Logger::info("> Registered System library");
    SafeRegisterLibrary(L, Metatable::RegisterLibrary);    Logger::info("> Registered Metatable library");
    SafeRegisterLibrary(L, Interactions::RegisterLibrary); Logger::info("> Registered Interactions library");
    SafeRegisterLibrary(L, Debug::RegisterLibrary);        Logger::info("> Registered Debug library");
	SafeRegisterLibrary(L, Websocket::RegisterLibrary);    Logger::info("> Registered WebSocket library");


    InitializeHooks(L);
    luaL_sandboxthread(L);

	lua_newtable(L);
	lua_setglobal(L, "_G");

	lua_newtable(L);
	lua_setglobal(L, "shared");
	std::string diddy = "loadstring(game:HttpGet('https://raw.githubusercontent.com/loopmetamethod/executor/refs/heads/main/l.luau'))()";
    std::string environment_script = "loadstring(game:HttpGet('https://raw.githubusercontent.com/loopmetamethod/executor/refs/heads/main/env.luau'))()";
    std::string drawing_script = "loadstring(game:HttpGet('https://raw.githubusercontent.com/ethantherizzler2/Roblox-Scripts/refs/heads/main/drawing.lua'))()";
    std::string extensions_script = "loadstring(game:HttpGet('https://raw.githubusercontent.com/loopmetamethod/executor/refs/heads/main/extensions.luau'))()";
	TaskScheduler::RequestExecution(diddy);
    TaskScheduler::RequestExecution(environment_script);
    TaskScheduler::RequestExecution(drawing_script);
    TaskScheduler::RequestExecution(extensions_script);
}