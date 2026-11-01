// Frame generation, step 1 (experimental, off by default: [EDAA] fg_present_test=1).
//
// DLSS Frame Generation (Streamline) only works on a D3D12 or Vulkan swap chain,
// and the game presents with D3D11. This step only checks the presentation path,
// without any frame generation yet:
//   - the game, ReShade (effects, overlay), 3DMigoto/EDHM and EDAA keep their
//     D3D11 swap chain untouched;
//   - at reshade_present (after ReShade's effects and overlay) the final image is
//     copied into a texture shared with D3D12 (D3D11 creates it, D3D12 opens it,
//     a shared fence orders the work, as in xess.cpp);
//   - a D3D12 flip-model swap chain on a child window covering the game's client
//     area presents that copy.
// The DXGI factory comes from the system dxgi.dll (not ReShade's dxgi.dll proxy),
// so ReShade does not attach a second effect runtime to this swap chain.

#include "events.hpp"
#include "state.hpp"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <Windows.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace reshade::api;

namespace edaa
{
    namespace
    {
        constexpr UINT buffer_count = 3;

        struct presenter
        {
            bool failed = false; // stop trying after an error (logged once)
            ID3D11Device* device11 = nullptr; // the game's device the resources belong to
            ComPtr<ID3D11Device5> device11_5;
            ComPtr<ID3D12Device> device12;
            ComPtr<ID3D12CommandQueue> queue;
            ComPtr<ID3D12CommandAllocator> allocators[buffer_count];
            uint64_t allocator_values[buffer_count] = {};
            ComPtr<ID3D12GraphicsCommandList> list;
            ComPtr<ID3D12Fence> fence12;
            ComPtr<ID3D11Fence> fence11;
            HANDLE fence_event = nullptr;
            uint64_t fence_value = 0; // last value signalled by either side
            uint64_t copy_done = 0; // D3D12 finished reading the shared texture up to this value

            // Shared texture (D3D11 writes, D3D12 reads).
            ComPtr<ID3D11Texture2D> shared11;
            ComPtr<ID3D12Resource> shared12;
            uint32_t width = 0, height = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

            // Child window and its swap chain.
            HWND parent = nullptr;
            std::atomic<HWND> child{ nullptr };
            std::thread window_thread;
            ComPtr<IDXGIFactory2> factory;
            ComPtr<IDXGISwapChain3> swapchain;
            bool tearing = false;
            uint64_t frames = 0;
        };

        presenter p;
        std::mutex presenter_mutex;


        void log_info(const char* text)
        {
            reshade::log::message(reshade::log::level::info, text);
        }


        bool fail(const char* what, HRESULT hr)
        {
            if (SUCCEEDED(hr)) {
                return false;
            }

            char msg[192];
            snprintf(msg, sizeof(msg), "EDAA FG test: %s (0x%08lX), presentation test stopped", what, static_cast<unsigned long>(hr));
            reshade::log::message(reshade::log::level::error, msg);
            p.failed = true;
            return true;
        }


        LRESULT CALLBACK child_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            switch (msg) {
                case WM_NCHITTEST:
                    return HTTRANSPARENT; // mouse input belongs to the game window
                case WM_MOUSEACTIVATE:
                    return MA_NOACTIVATE;
                case WM_ERASEBKGND:
                    return 1;
                default:
                    return DefWindowProcW(hwnd, msg, wparam, lparam);
            }
        }


        // The child window lives on its own thread with its own message loop, so
        // the game's window thread never waits for the render thread.
        bool create_child(HWND parent, uint32_t width, uint32_t height)
        {
            HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);

            p.window_thread = std::thread([parent, width, height, ready]() {
                const HINSTANCE instance = GetModuleHandleW(nullptr);
                WNDCLASSEXW wc = { sizeof(wc) };
                wc.lpfnWndProc = child_proc;
                wc.hInstance = instance;
                wc.lpszClassName = L"EDAA_FG_Present";
                RegisterClassExW(&wc);

                const HWND hwnd = CreateWindowExW(WS_EX_NOPARENTNOTIFY, wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS,
                    0, 0, static_cast<int>(width), static_cast<int>(height), parent, nullptr, instance, nullptr);
                p.child = hwnd;
                SetEvent(ready);

                if (hwnd == nullptr) {
                    return;
                }

                MSG msg;

                while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            });

            WaitForSingleObject(ready, 5000);
            CloseHandle(ready);

            if (p.child.load() == nullptr) {
                log_info("EDAA FG test: child window creation failed, presentation test stopped");
                p.failed = true;
                return false;
            }

            return true;
        }


        // System dxgi.dll: ReShade's proxy would add a second effect runtime.
        bool create_factory()
        {
            wchar_t path[MAX_PATH];
            const UINT len = GetSystemDirectoryW(path, MAX_PATH);
            const std::wstring dll = std::wstring(path, len) + L"\\dxgi.dll";
            const HMODULE module = LoadLibraryExW(dll.c_str(), nullptr, 0);
            using create_t = HRESULT(WINAPI*)(UINT, REFIID, void**);
            const auto create = module != nullptr ? reinterpret_cast<create_t>(GetProcAddress(module, "CreateDXGIFactory2")) : nullptr;

            if (create == nullptr) {
                log_info("EDAA FG test: CreateDXGIFactory2 not found in the system dxgi.dll, presentation test stopped");
                p.failed = true;
                return false;
            }

            if (fail("cannot create a DXGI factory", create(0, IID_PPV_ARGS(&p.factory)))) {
                return false;
            }

            ComPtr<IDXGIFactory5> factory5;
            BOOL allow = FALSE;

            if (SUCCEEDED(p.factory.As(&factory5)) &&
                SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow)))) {
                p.tearing = allow != FALSE;
            }

            return true;
        }


        bool create_device(ID3D11Device* device)
        {
            if (fail("Windows 10 1703 or newer needed (ID3D11Device5)", device->QueryInterface(IID_PPV_ARGS(&p.device11_5)))) {
                return false;
            }

            ComPtr<IDXGIDevice> dxgi;
            ComPtr<IDXGIAdapter> adapter;

            if (fail("cannot get the game's GPU", device->QueryInterface(IID_PPV_ARGS(&dxgi))) ||
                fail("cannot get the game's GPU", dxgi->GetAdapter(&adapter))) {
                return false;
            }

            const HMODULE d3d12 = LoadLibraryW(L"d3d12.dll");
            const auto create = d3d12 != nullptr ? reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(GetProcAddress(d3d12, "D3D12CreateDevice")) : nullptr;

            if (create == nullptr) {
                log_info("EDAA FG test: d3d12.dll not available, presentation test stopped");
                p.failed = true;
                return false;
            }

            if (fail("cannot create a D3D12 device on the game's GPU", create(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&p.device12)))) {
                return false;
            }

            D3D12_COMMAND_QUEUE_DESC queue = {};
            queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

            if (fail("D3D12 queue creation failed", p.device12->CreateCommandQueue(&queue, IID_PPV_ARGS(&p.queue)))) {
                return false;
            }

            for (ComPtr<ID3D12CommandAllocator>& allocator : p.allocators) {
                if (fail("D3D12 allocator creation failed", p.device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) {
                    return false;
                }
            }

            if (fail("D3D12 command list creation failed",
                    p.device12->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, p.allocators[0].Get(), nullptr, IID_PPV_ARGS(&p.list)))) {
                return false;
            }

            p.list->Close();

            HANDLE handle = nullptr;

            if (fail("shared fence creation failed", p.device12->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&p.fence12))) ||
                fail("shared fence creation failed", p.device12->CreateSharedHandle(p.fence12.Get(), nullptr, GENERIC_ALL, nullptr, &handle))) {
                return false;
            }

            const HRESULT opened = p.device11_5->OpenSharedFence(handle, IID_PPV_ARGS(&p.fence11));
            CloseHandle(handle);

            if (fail("D3D11 cannot open the shared fence", opened)) {
                return false;
            }

            p.fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            p.device11 = device;
            return true;
        }


        void wait_for(uint64_t value)
        {
            if (p.fence12->GetCompletedValue() < value) {
                p.fence12->SetEventOnCompletion(value, p.fence_event);
                WaitForSingleObject(p.fence_event, 1000);
            }
        }


        // Shared texture and swap chain for this size and format.
        bool resize(uint32_t width, uint32_t height, DXGI_FORMAT format)
        {
            if (p.shared11 && p.width == width && p.height == height && p.format == format) {
                return true;
            }

            wait_for(p.fence_value); // nothing in flight reads the old resources
            p.shared12.Reset();
            p.shared11.Reset();

            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

            if (fail("cannot create the shared texture", p.device11_5->CreateTexture2D(&desc, nullptr, &p.shared11))) {
                return false;
            }

            ComPtr<IDXGIResource1> resource;
            HANDLE handle = nullptr;

            if (fail("cannot share the texture", p.shared11.As(&resource)) ||
                fail("cannot share the texture", resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &handle))) {
                return false;
            }

            const HRESULT opened = p.device12->OpenSharedHandle(handle, IID_PPV_ARGS(&p.shared12));
            CloseHandle(handle);

            if (fail("D3D12 cannot open the shared texture", opened)) {
                return false;
            }

            const HWND child = p.child.load();
            SetWindowPos(child, HWND_TOP, 0, 0, static_cast<int>(width), static_cast<int>(height), SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);

            const UINT flags = p.tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

            if (p.swapchain) {
                if (fail("swap chain resize failed", p.swapchain->ResizeBuffers(buffer_count, width, height, format, flags))) {
                    return false;
                }
            }
            else {
                DXGI_SWAP_CHAIN_DESC1 sc = {};
                sc.Width = width;
                sc.Height = height;
                sc.Format = format;
                sc.SampleDesc.Count = 1;
                sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                sc.BufferCount = buffer_count;
                sc.Scaling = DXGI_SCALING_STRETCH;
                sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                sc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
                sc.Flags = flags;
                ComPtr<IDXGISwapChain1> swapchain1;

                if (fail("D3D12 swap chain creation failed", p.factory->CreateSwapChainForHwnd(p.queue.Get(), child, &sc, nullptr, nullptr, &swapchain1)) ||
                    fail("D3D12 swap chain has no IDXGISwapChain3", swapchain1.As(&p.swapchain))) {
                    return false;
                }

                p.factory->MakeWindowAssociation(child, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
            }

            p.width = width;
            p.height = height;
            p.format = format;
            char msg[160];
            snprintf(msg, sizeof(msg), "EDAA FG test: presenting %ux%u through a D3D12 flip swap chain (tearing %s)", width, height, p.tearing ? "on" : "off");
            log_info(msg);
            return true;
        }


        DXGI_FORMAT plain_format(DXGI_FORMAT format)
        {
            switch (format) {
                case DXGI_FORMAT_R8G8B8A8_TYPELESS:
                case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                    return DXGI_FORMAT_R8G8B8A8_UNORM;
                case DXGI_FORMAT_B8G8R8A8_TYPELESS:
                case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                    return DXGI_FORMAT_B8G8R8A8_UNORM;
                default:
                    return format;
            }
        }


        void present_frame(effect_runtime* runtime)
        {
            auto* const device = reinterpret_cast<ID3D11Device*>(runtime->get_device()->get_native());
            auto* const ctx = reinterpret_cast<ID3D11DeviceContext*>(runtime->get_command_queue()->get_immediate_command_list()->get_native());
            auto* const back_buffer = reinterpret_cast<ID3D11Texture2D*>(runtime->get_current_back_buffer().handle);

            if (device == nullptr || ctx == nullptr || back_buffer == nullptr) {
                return;
            }

            if (p.device11 != nullptr && p.device11 != device) {
                return; // another device (should not happen): leave it alone
            }

            D3D11_TEXTURE2D_DESC bb = {};
            back_buffer->GetDesc(&bb);

            if (p.device12 == nullptr) {
                p.parent = static_cast<HWND>(runtime->get_hwnd());

                if (!create_factory() || !create_device(device) || !create_child(p.parent, bb.Width, bb.Height)) {
                    return;
                }
            }

            if (!resize(bb.Width, bb.Height, plain_format(bb.Format))) {
                return;
            }

            ComPtr<ID3D11DeviceContext4> ctx4;

            if (fail("Windows 10 1703 or newer needed (ID3D11DeviceContext4)", ctx->QueryInterface(IID_PPV_ARGS(&ctx4)))) {
                return;
            }

            // D3D11: wait until D3D12 read the previous copy, copy, signal.
            ctx4->Wait(p.fence11.Get(), p.copy_done);
            ctx->CopyResource(p.shared11.Get(), back_buffer);
            const uint64_t ready = ++p.fence_value;
            ctx4->Signal(p.fence11.Get(), ready);
            ctx->Flush();

            // D3D12: wait for the copy, copy into the swap chain's buffer, present.
            const UINT index = p.swapchain->GetCurrentBackBufferIndex();
            wait_for(p.allocator_values[index]);
            ComPtr<ID3D12Resource> target;

            if (fail("cannot get the D3D12 back buffer", p.swapchain->GetBuffer(index, IID_PPV_ARGS(&target)))) {
                return;
            }

            p.allocators[index]->Reset();
            p.list->Reset(p.allocators[index].Get(), nullptr);

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = target.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            p.list->ResourceBarrier(1, &barrier);
            // The shared texture is in the common state: implicitly promoted to COPY_SOURCE.
            p.list->CopyResource(target.Get(), p.shared12.Get());
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            p.list->ResourceBarrier(1, &barrier);
            p.list->Close();

            p.queue->Wait(p.fence12.Get(), ready);
            ID3D12CommandList* const lists[] = { p.list.Get() };
            p.queue->ExecuteCommandLists(1, lists);
            const uint64_t done = ++p.fence_value;
            p.queue->Signal(p.fence12.Get(), done);
            p.copy_done = done;
            p.allocator_values[index] = done;

            const HRESULT presented = p.swapchain->Present(0, p.tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);

            if (FAILED(presented)) {
                fail("D3D12 present failed", presented);
                return;
            }

            if (++p.frames == 1) {
                log_info("EDAA FG test: first frame presented through D3D12");
            }
        }


        void on_reshade_present_fg(effect_runtime* runtime)
        {
            bool enabled;
            {
                addon_state& s = state();
                const std::lock_guard<std::recursive_mutex> lock(s.mutex);
                enabled = s.config.fg_present_test;
            }

            const std::lock_guard<std::mutex> lock(presenter_mutex);

            if (!enabled || p.failed || runtime->get_device()->get_api() != device_api::d3d11) {
                return;
            }

            try {
                present_frame(runtime);
            }
            catch (...) {
                log_info("EDAA FG test: exception, presentation test stopped");
                p.failed = true;
            }
        }


        void on_destroy_device_fg(device* device)
        {
            const std::lock_guard<std::mutex> lock(presenter_mutex);

            if (p.device11 == nullptr || reinterpret_cast<ID3D11Device*>(device->get_native()) != p.device11) {
                return;
            }

            if (p.fence12) {
                wait_for(p.fence_value);
            }

            const HWND child = p.child.load();

            if (child != nullptr) {
                PostMessageW(child, WM_CLOSE, 0, 0);
                PostThreadMessageW(GetThreadId(p.window_thread.native_handle()), WM_QUIT, 0, 0);
            }

            if (p.window_thread.joinable()) {
                p.window_thread.detach();
            }

            if (p.fence_event != nullptr) {
                CloseHandle(p.fence_event);
            }

            p.swapchain.Reset();
            p.factory.Reset();
            p.shared12.Reset();
            p.shared11.Reset();
            p.list.Reset();

            for (ComPtr<ID3D12CommandAllocator>& allocator : p.allocators) {
                allocator.Reset();
            }

            p.queue.Reset();
            p.fence12.Reset();
            p.fence11.Reset();
            p.device12.Reset();
            p.device11_5.Reset();
            p.device11 = nullptr;
            p.fence_event = nullptr;
            p.child = nullptr;
            p.width = p.height = 0;
            p.frames = 0;
        }
    }


    void register_fg_events()
    {
        reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present_fg);
        reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device_fg);
    }


    void unregister_fg_events()
    {
        reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device_fg);
        reshade::unregister_event<reshade::addon_event::reshade_present>(on_reshade_present_fg);
    }
}
