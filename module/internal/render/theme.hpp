#pragma once
#include <../Dependencies/imgui/imgui.h>

ImFont* MainFont = nullptr;
ImFont* TitleFont = nullptr;
// coded by chat gpt 
namespace theme
{
    inline void apply()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Pulsar neon-white text
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.65f, 1.00f);

        // Dark cosmic backgrounds
        colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.10f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.15f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.18f, 0.95f);

        // Borders
        colors[ImGuiCol_Border] = ImVec4(0.30f, 0.20f, 0.60f, 0.80f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        // Frames / Buttons
        colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.10f, 0.18f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.10f, 0.60f, 0.90f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.15f, 0.70f, 1.00f);

        colors[ImGuiCol_Button] = ImVec4(0.12f, 0.10f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.15f, 0.65f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.20f, 0.75f, 1.00f);

        colors[ImGuiCol_Header] = ImVec4(0.10f, 0.10f, 0.25f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.15f, 0.60f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.20f, 0.75f, 1.00f);

        // Tabs
        colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.08f, 0.15f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.10f, 0.60f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.35f, 0.15f, 0.75f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.05f, 0.05f, 0.12f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.05f, 0.40f, 1.00f);

        // Scrollbars
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.03f, 0.03f, 0.08f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.10f, 0.55f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.15f, 0.70f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.20f, 0.85f, 1.00f);

        // Separators
        colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.10f, 0.60f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.35f, 0.15f, 0.75f, 1.00f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.45f, 0.25f, 0.90f, 1.00f);

        // Titlebar (Pulsar glow)
        colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.05f, 0.60f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.35f, 0.10f, 0.80f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.05f, 0.30f, 1.00f);

        // Other
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 0.10f, 0.65f, 0.80f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);

        // Rounded corners
        style.WindowRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 5.0f;
        style.ScrollbarRounding = 6.0f;
        style.TabRounding = 5.0f;

        // Spacing
        style.WindowPadding = ImVec2(12, 12);
        style.FramePadding = ImVec2(6, 4);
        style.ItemSpacing = ImVec2(8, 6);
    }
}
