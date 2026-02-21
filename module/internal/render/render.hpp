
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <../dependencies/imgui/imgui.h>
#include <../dependencies/imgui/backends/imgui_impl_dx11.h>
#include <../dependencies/imgui/backends/imgui_impl_win32.h>
#include "overlay.hpp"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


namespace rbx {
    class render {
    public:
        typedef HRESULT(WINAPI* present_fn)(IDXGISwapChain*, UINT, UINT);
        typedef HRESULT(WINAPI* resize_buffers_fn)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

        static void init();
        static bool imgui_init();

        static HRESULT WINAPI present_h(IDXGISwapChain* swapchain, UINT sync_intervals, UINT flags);
        static HRESULT WINAPI resize_buffers_h(IDXGISwapChain* swapchain, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT new_format, UINT flags);
        static LRESULT CALLBACK wnd_proc_h(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam);

    private:
        static void render_hooks();

        static ID3D11Device* device;
        static ID3D11DeviceContext* device_context;
        static IDXGISwapChain* swap_chain;
        static ID3D11RenderTargetView* render_target_view;

        static present_fn original_present;
        static resize_buffers_fn original_resize_buffers;
        static WNDPROC original_wnd_proc;

        static HWND our_window;
        static bool is_init, draw;
        static float dpi_scale;
    };
}