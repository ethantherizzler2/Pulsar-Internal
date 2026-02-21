#include <internal/roblox/task_scheduler/tphandler.hpp>
#include <internal/roblox/task_scheduler/scheduler.hpp>
#include <internal/render/render.hpp>
#include <internal/roblox/task_scheduler/tphandler.hpp>
#include <internal/utils.hpp>
#include <internal/Globals.hpp>
#include <internal/render/render.hpp>
#include <atomic>
#include <thread>
#include <chrono>

namespace
{
    std::atomic<uintptr_t> CurrentDataModel{ 0 };
    std::atomic<int64_t> LastPlaceId{ -1 };
    std::atomic<bool> Initialized{ false };
}

static bool IsFullyInGame(uintptr_t DataModel)
{
    if (!DataModel)
        return false;

    if (!Utils::IsInGame(DataModel))
        return false;

    uintptr_t ScriptContext = TaskScheduler::GetScriptContext(DataModel);
    if (!ScriptContext)
        return false;

    return true;
}

void TPHandler::Tick()
{
    uintptr_t dataModel = TaskScheduler::GetDataModel();

    if (!dataModel)
    {
        CurrentDataModel.store(0, std::memory_order_release);
        Initialized.store(false, std::memory_order_release);
        return;
    }

    uintptr_t last = CurrentDataModel.load(std::memory_order_acquire);

    if (dataModel != last)
    {
        CurrentDataModel.store(dataModel, std::memory_order_release);
        Initialized.store(false, std::memory_order_release);
        return;
    }

    if (Initialized.load(std::memory_order_acquire))
        return;

    if (!IsFullyInGame(dataModel))
        return;


    SharedVariables::LastDataModel = dataModel;

    if (TaskScheduler::SetupExploit())
    {
        Initialized.store(true, std::memory_order_release);
    }
    else
    {
        Initialized.store(false, std::memory_order_release);
    }
}
void TPHandler::Reset()
{
    CurrentDataModel.store(0, std::memory_order_release);
    Initialized.store(false, std::memory_order_release);
}

bool TPHandler::IsInGame()
{
    return Initialized.load(std::memory_order_acquire);
}

uintptr_t TPHandler::GetBoundDataModel()
{
    return CurrentDataModel.load(std::memory_order_acquire);
}