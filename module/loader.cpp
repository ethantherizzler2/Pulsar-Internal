#include <internal/utils.hpp>
#include <internal/roblox/task_scheduler/tphandler.hpp>
#include <internal/render/render.hpp>
#include <internal/roblox/veh.hpp>
#include <internal/globals.hpp>
#include <internal/roblox/task_scheduler/scheduler.hpp>
#include <internal/misc/server/server.hpp>
#include "Proxy.h"
namespace
{
    constexpr auto wait = 500;
    std::atomic<bool> RendererStarted{ false };
}

void MainThread()
{
	MessageBoxA(NULL, "Module Loaded 0", "Success", MB_OK | MB_ICONINFORMATION);
	Logger::info("> Initializing Crash handler");
    AdvancedGuard::Init();
	Logger::info("> Crash handler initialized");
    Logger::info("> Starting Local server");
    server::Initialize();
    Logger::info("> Server Started");
    Logger::info("> Initializing Rendering");
    rbx::render::init();
    Logger::info("> Render initialized");
    RendererStarted.store(true);
    Logger::info("> Module initialized");
    Roblox::Print(1, "Module Loaded");
	MessageBoxA(NULL, "Module Loaded 1", "Success", MB_OK | MB_ICONINFORMATION);  
    while (true)
    {
        TPHandler::Tick();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(wait)
        );
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        std::thread(MainThread).detach();
    }
    return TRUE;
}

extern "C" __declspec(dllexport) int NextHook(int nCode, WPARAM wParam, LPARAM lParam) { return CallNextHookEx(NULL, nCode, wParam, lParam); }