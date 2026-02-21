#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include "overlay.hpp"
#include "theme.hpp"
#include <mutex>
#include <vector>
#include <string>
#include "execute.hpp"
#include "internal/roblox/task_scheduler/scheduler.hpp"
#include <internal/Logger.hpp>
#pragma comment(lib, "ws2_32.lib")
#include "../Dependencies/imgui/TextEditor/TextEditor.h"

static bool debuggerOpen = false;
static bool robloxSuspended = false;
static HANDLE hRobloxMainThread = nullptr;
static bool consoleEnabled = false;   // ❗ START DISABLED
static bool consoleWasEnabled = false;
static bool consoleConnected = false;


struct LogEntry {
    std::string text;
    ImVec4 color;
};

static std::vector<LogEntry> g_logs;

void rbx::c_gui::draw(bool* visible)
{
    static bool theme_applied = false;
    if (!theme_applied)
    {
        theme::apply();
        theme_applied = true;
    }

    static TextEditor editor;
    static bool editor_init = false;
    if (!editor_init)
    {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
        editor.SetPalette(TextEditor::GetDarkPalette());
        editor.SetText("print('Hello World')");
        editor_init = true;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Always);

    if (!ImGui::Begin("Pulsar", visible, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }
    ImGui::Dummy(ImVec2(0, 4));

    ImGui::Indent(8);
    if (ImGui::BeginTabBar("##tabs"))
    {
        ImGui::Unindent(8);

        if (ImGui::BeginTabItem("Main"))
        {
            float button_area_height = ImGui::GetFrameHeight() + 10.0f;

            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
            ImGui::BeginChild("##editor_region", ImVec2(0, -button_area_height), true, ImGuiWindowFlags_NoScrollbar);

            editor.Render("##lua_editor", ImVec2(-FLT_MIN, -FLT_MIN), false);

            ImGui::EndChild();
            ImGui::PopStyleVar();

            ImGui::SetCursorPosX(5);

            if (ImGui::Button("Execute", ImVec2(80, 0)))
            {
                std::string script_to_send = editor.GetText();

                std::thread([script_to_send]() {
                    execute_script_async(script_to_send);
                    }).detach();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear", ImVec2(80, 0)))
            {
                editor.SetText("");
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Console"))
        {
            if (ImGui::Button("Ping Module"))
            {
                TaskScheduler::RequestExecution("print(\"> Module Pinged > ENV success\")");
            }

            ImGui::SameLine();

            if (ImGui::Button("Debugger"))
            {
                debuggerOpen = true;
            }

            ImGui::SameLine();

            ImGui::Checkbox("Enable Console", &consoleEnabled);

            if (consoleEnabled)
            {
				MessageBoxA(nullptr, "Hello Console currently doesn't work you have to wait for next update..", "Eclipse", MB_OK | MB_ICONINFORMATION);  

            }
            else
            {
                g_logs.clear(); 
            }

            ImGui::Separator();

            ImGui::BeginChild("##console", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);

            if (consoleEnabled)
            {

            }

            if (g_logs.empty()) {
                ImGui::TextWrapped("> Ready.");
            }
            else {
                for (const auto& log : g_logs) {
                    ImGui::PushStyleColor(ImGuiCol_Text, log.color);
                    ImGui::TextUnformatted(log.text.c_str());
                    ImGui::PopStyleColor();
                }

                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (debuggerOpen)
        {
            ImGui::Begin("Debugger", &debuggerOpen, ImGuiWindowFlags_NoCollapse);

            ImGui::Separator();

            ImVec2 avail = ImGui::GetContentRegionAvail();

            float buttonHeight = ImGui::GetFrameHeightWithSpacing();
            ImVec2 logSize = ImVec2(avail.x, avail.y - buttonHeight - 10.0f); 

            ImGui::BeginChild("##debuglogs", logSize, true,
                ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);

            const auto& logs = Logger::Core::Get().GetLogs();
            for (const auto& entry : logs)
            {
                ImGui::TextWrapped("[%s] [%s] %s",
                    entry.timestamp.c_str(),
                    [&]() -> const char* {
                        switch (entry.level)
                        {
                        case Logger::Level::Info:  return "INFO";
                        case Logger::Level::Warn:  return "WARN";
                        case Logger::Level::Error: return "ERROR";
                        case Logger::Level::Debug: return "DEBUG";
                        default: return "UNKNOWN";
                        }
                    }(),
                        entry.message.c_str());
            }

            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);

            ImGui::EndChild();

            ImGui::Spacing();

            if (hRobloxMainThread != nullptr)
            {
                ImGui::Text("Roblox main thread handle: 0x%p", hRobloxMainThread);

                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 110);
                if (robloxSuspended)
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 50, 50, 255)); 
                else
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 200, 50, 255)); 

                if (ImGui::Button(robloxSuspended ? "Resume Roblox" : "Suspend Roblox", ImVec2(100, 0)))
                {
                    if (!robloxSuspended)
                    {
                        SuspendThread(hRobloxMainThread);
                        robloxSuspended = true;
                        Logger::warn("> Suspended Roblox main thread.");
                    }
                    else
                    {
                        ResumeThread(hRobloxMainThread);
                        robloxSuspended = false;
                        Logger::warn("> Resumed Roblox main thread.");
                    }
                }
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Roblox main thread handle not set!");
            }

            ImGui::End();
        }



        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}