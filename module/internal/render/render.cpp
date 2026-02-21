#include "render.hpp"
#include <internal/utils.hpp>
#include <internal/roblox/update/offsets.hpp>
#include <string>
#include <vector>
#include <iostream>

ID3D11Device* rbx::render::device = nullptr;
ID3D11DeviceContext* rbx::render::device_context = nullptr;
IDXGISwapChain* rbx::render::swap_chain = nullptr;
ID3D11RenderTargetView* rbx::render::render_target_view = nullptr;
rbx::render::present_fn rbx::render::original_present = nullptr;
rbx::render::resize_buffers_fn rbx::render::original_resize_buffers = nullptr;
WNDPROC rbx::render::original_wnd_proc = nullptr;
HWND rbx::render::our_window = nullptr;
bool rbx::render::is_init = false, rbx::render::draw = false;
float rbx::render::dpi_scale = 1.f;

bool IsRenderJob(uintptr_t Job) {
    __try {
        uintptr_t name_base = Job + Offsets::Scripts::Job_Name;
        const char* name_cstr = nullptr;

        name_cstr = *reinterpret_cast<const char**>(name_base);

        if (name_cstr && !IsBadReadPtr(name_cstr, 9)) {
            if (strcmp(name_cstr, "RenderJob") == 0) return true;
        }

        name_cstr = reinterpret_cast<const char*>(name_base);
        if (name_cstr && strcmp(name_cstr, "RenderJob") == 0) return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {} // if your using deni injector w seh !
    return false;
}

bool SafeReadPtr(uintptr_t address, uintptr_t& out) {
    __try {
        out = *reinterpret_cast<uintptr_t*>(address);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void rbx::render::render_hooks() {
    Roblox::Print(1, "Starting render_hooks...");

    uintptr_t SchedulerPtr = 0;
    if (!SafeReadPtr(Offsets::RawScheduler, SchedulerPtr) || !SchedulerPtr) {
        Roblox::Print(3, "CRASH PREVENTED: Failed to read RawScheduler");
        return;
    }

    uintptr_t JobsStart = 0;
    uintptr_t JobsEnd = 0;

    if (!SafeReadPtr(SchedulerPtr + Offsets::Scripts::JobStart, JobsStart) ||
        !SafeReadPtr(SchedulerPtr + Offsets::Scripts::JobStart + sizeof(void*), JobsEnd)) {
        Roblox::Print(3, "CRASH SAVE: Failed to read Job pointers");
        return;
    }

    uintptr_t render_view = 0;
    for (uintptr_t i = JobsStart; i < JobsEnd; i += 0x10) {
        uintptr_t Job = 0;
        if (!SafeReadPtr(i, Job) || !Job) continue;

        if (IsRenderJob(Job)) {
            if (SafeReadPtr(Job + Offsets::render::renderview, render_view)) {
                break;
            }
        }
    }

    if (!render_view) {
        Roblox::Print(3, "Error: RenderView not found");
        return;
    }

    uintptr_t rbx_device = 0;
    if (!SafeReadPtr(render_view + Offsets::render::DeviceD3D11, rbx_device) || !rbx_device) {
        Roblox::Print(3, "Error: Invalid DeviceD3D11 pointer");
        return;
    }

    // From here on, we are touching the COM objects (Swapchain)
    // which usually require standard HRESULT checks rather than SEH
    swap_chain = *reinterpret_cast<IDXGISwapChain**>(rbx_device + Offsets::render::Swapchain);
    if (!swap_chain) {
        Roblox::Print(3, "Error: SwapChain is null");
        return;
    }

    DXGI_SWAP_CHAIN_DESC desc;
    if (FAILED(swap_chain->GetDesc(&desc))) {
        Roblox::Print(3, "Error: GetDesc failed");
        return;
    }

    our_window = desc.OutputWindow;

    if (FAILED(swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&device))) {
        Roblox::Print(3, "Error: GetDevice failed");
        return;
    }

    device->GetImmediateContext(&device_context);

    // VTable Swap
    void** OriginalVTable = *reinterpret_cast<void***>(swap_chain);
    void** ShadowVTable = new void* [18];
    memcpy(ShadowVTable, OriginalVTable, sizeof(void*) * 18);

    original_present = (present_fn)OriginalVTable[8];
    ShadowVTable[8] = (void*)&present_h;

    original_resize_buffers = (resize_buffers_fn)OriginalVTable[13];
    ShadowVTable[13] = (void*)&resize_buffers_h;

    DWORD old;
    if (VirtualProtect(swap_chain, sizeof(void*), PAGE_READWRITE, &old)) {
        *reinterpret_cast<void***>(swap_chain) = ShadowVTable;
        VirtualProtect(swap_chain, sizeof(void*), old, &old);
        Roblox::Print(1, "SUCCESS: VTable Hooked");
    }
    else {
        Roblox::Print(3, "Error: VirtualProtect failed");
    }

    original_wnd_proc = (WNDPROC)SetWindowLongPtrW(our_window, GWLP_WNDPROC, (LONG_PTR)wnd_proc_h);
    if (original_wnd_proc) {
        Roblox::Print(1, "SUCCESS: WndProc Hooked");
    }
}

HRESULT WINAPI rbx::render::present_h(IDXGISwapChain* swapchain, UINT sync, UINT flags) {
    if (!is_init) {
        is_init = imgui_init();
        if (is_init) Roblox::Print(3, "ImGui initialized");
    }

    if (!render_target_view && device) {
        ID3D11Texture2D* back_buffer = nullptr;
        if (SUCCEEDED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer))) {
            device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
            back_buffer->Release();
        }
    }

    if (is_init && render_target_view && device_context) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (draw) c_gui::draw(&draw);

        ImGui::Render();
        device_context->OMSetRenderTargets(1, &render_target_view, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return original_present(swapchain, sync, flags);
}

HRESULT WINAPI rbx::render::resize_buffers_h(IDXGISwapChain* swapchain, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT new_format, UINT flags) {
    if (render_target_view) {
        render_target_view->Release();
        render_target_view = nullptr;
    }

    return original_resize_buffers(swapchain, buffer_count, width, height, new_format, flags);
}

LRESULT CALLBACK rbx::render::wnd_proc_h(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_KEYDOWN) {
        if (wparam == VK_INSERT || wparam == VK_RSHIFT || wparam == VK_HOME)
            draw = !draw;
    }
    else if (msg == WM_DPICHANGED) {
        dpi_scale = LOWORD(wparam) / 96.f;
    }

    extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    if (draw && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wparam, lparam))
        return true;

    if (draw) {
        switch (msg) {
        case WM_MOUSEWHEEL:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_KEYUP:
        case WM_CHAR:
            return true;
        }
    }

    return CallWindowProc(original_wnd_proc, hWnd, msg, wparam, lparam);
}

bool rbx::render::imgui_init() {
    if (!our_window || !device || !device_context) return false;
    ImGui::CreateContext();
    if (!ImGui_ImplWin32_Init(our_window)) return false;
    if (!ImGui_ImplDX11_Init(device, device_context)) return false;
    return true;
}

void rbx::render::init() {
    render_hooks();

}
