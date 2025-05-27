// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <iostream>
#include <Windows.h>
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <d3d9.h>
#include "vars.h"
#include <tchar.h>
#include "memory.h"
#include <atomic>

// Data
static LPDIRECT3D9         g_pD3D = nullptr;
static LPDIRECT3DDEVICE9     g_pd3dDevice = nullptr;
static bool                g_DeviceLost = false;
static UINT                g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS g_d3dpp = {};
static HWND                g_targetHwnd = nullptr;
static RECT                g_targetRect = { 0, 0, 0, 0 };
static HWND                g_overlayHwnd = nullptr;
static std::atomic<bool>   g_showOverlay(false);
static bool                g_isMenuOpen = false;
static std::atomic<bool>   g_isRunning(true);

// Cheat options
static bool godModeEnabled = false;
static bool espEnabled = false;
static bool unlimitedMagickEnabled = false;
static bool unlimitedStaminaEnabled = false;
static bool noClipEnabled = false;
static bool aimbotEnabled = false;
static bool itemUnlimitedSpawnEnabled = false;

// //Player info
static int playerHealth;
static int playerMana;
static int playerStamina;

// Menu background color
static ImVec4 menuBackgroundColor = ImVec4(0.1f, 0.1f, 0.1f, 0.8f);

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI ImGuiThread(LPVOID lpParam);
DWORD WINAPI Main(HMODULE hModule);

DWORD WINAPI ImGuiThread(LPVOID lpParam)
{
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Overlay", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName, L"Dear ImGui DirectX9 Overlay", WS_POPUP, 100, 100, 300, 200, nullptr, nullptr, wc.hInstance, nullptr);

    if (!hwnd)
    {
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    g_overlayHwnd = hwnd;

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show/Hide window based on initial state
    ::ShowWindow(hwnd, g_showOverlay ? SW_SHOWNOACTIVATE : SW_HIDE);
    ::UpdateWindow(hwnd);
    ::SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    HWND foregroundWindow = nullptr;
    bool firstFrame = true; // Track the first frame

    while (g_isRunning)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                g_isRunning = false;
        }
        if (!g_isRunning)
            break;

        if (g_DeviceLost)
        {
            HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                ResetDevice();
            g_DeviceLost = false;
        }

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        if (g_targetHwnd && g_showOverlay)
        {
            GetWindowRect(g_targetHwnd, &g_targetRect);
            ::SetWindowPos(g_overlayHwnd, HWND_TOPMOST, g_targetRect.left, g_targetRect.top,
                g_targetRect.right - g_targetRect.left, g_targetRect.bottom - g_targetRect.top, 0); // Removed SWP_NOACTIVATE
        }


        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_showOverlay)
        {
            ImVec2 windowSize = ImVec2(300, 200); // Your initial window size
            ImVec2 windowPos;

            if (firstFrame)
            {
                float targetWidth = static_cast<float>(g_targetRect.right - g_targetRect.left);
                float targetHeight = static_cast<float>(g_targetRect.bottom - g_targetRect.top);
                windowPos = ImVec2((targetWidth - windowSize.x) * 0.5f, (targetHeight - windowSize.y) * 0.5f);
                ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
                firstFrame = false;
            }
            else
            {
                ImGui::SetNextWindowPos(ImGui::GetIO().MousePos, ImGuiCond_FirstUseEver); // Keep last position
            }
            ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, menuBackgroundColor); // Set background color
            ImGui::Begin("Game Cheats", &g_isMenuOpen, ImGuiWindowFlags_NoCollapse); // Allow moving by any part

            if (ImGui::BeginCombo("Player Info", nullptr))
            {
                ImGui::Text("Health: %d", playerHealth);
                ImGui::Text("Mana: %d", playerMana);
                ImGui::Text("Stamina: %d", playerStamina);
                ImGui::EndCombo();
            }

            if (ImGui::BeginCombo("Hacks", nullptr))
            {
                ImGui::Checkbox("God Mode", &godModeEnabled);
                ImGui::Checkbox("Unlimited Magick", &unlimitedMagickEnabled);
                ImGui::Checkbox("Unlimited Stamina", &unlimitedStaminaEnabled);
                ImGui::Checkbox("ESP", &espEnabled);
                ImGui::Checkbox("No Clip", &noClipEnabled);
                ImGui::Checkbox("Aimbot", &aimbotEnabled);
                ImGui::Checkbox("Item Unlimited Spawn", &itemUnlimitedSpawnEnabled);
                ImGui::EndCombo();
            }

            ImGui::PopStyleColor();
            ImGui::End();

            // Check for focus loss
            HWND currentForegroundWindow = GetForegroundWindow();
            if (currentForegroundWindow != g_overlayHwnd && g_isMenuOpen)
            {
                g_showOverlay = false;
                ::ShowWindow(g_overlayHwnd, SW_HIDE);
                g_isMenuOpen = false;
            }
        }

        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST)
            g_DeviceLost = true;

        ::Sleep(1);
    }

    // Cleanup ImGui
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // Cleanup D3D
    CleanupDeviceD3D();
    if (::IsWindow(hwnd))
        ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;
    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void initiateHooks() {
    uintptr_t hookAddress = skyrimModuleBase + 0x74452C;
    Hook64((void*)hookAddress, (void*)hk_coords, 15);
}

DWORD WINAPI Main(HMODULE hModule)
{
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    std::cout << "DLL Injected. Press END to detach." << std::endl;
    std::cout << "Press INSERT to toggle the overlay menu." << std::endl;

    //initiateHooks();

    g_targetHwnd = FindWindow(NULL, L"Skyrim Special Edition");
    if (!g_targetHwnd)
    {
        std::cerr << "Error: Target window not found." << std::endl;
        if (f) fclose(f);
        FreeConsole();
        FreeLibraryAndExitThread(hModule, 1);
        return 1;
    }
    std::cout << "Local PLayer" << &hookedLocalPlayerBaseAddress << std::endl;
    std::cout << "Local PLayer" << &localPlayerBaseAddress << std::endl;
    std::cout << "Target window found. HWND: " << g_targetHwnd << std::endl;

    HANDLE hImGuiThread = CreateThread(nullptr, 0, ImGuiThread, nullptr, 0, nullptr);

    std::cout << localPlayerPtr->Mana << std::endl;
    while (g_isRunning)
    {
        localPlayerPtr->Mana = 0.0f;

        // Init Hacks here
        if (localPlayerPtr)
        {
            godModeEnabled ? localPlayerPtr->Health = 0.0f: NULL;
            unlimitedMagickEnabled ? localPlayerPtr->Mana = 0.0f: NULL;
            unlimitedStaminaEnabled ? localPlayerPtr->Stamina = 0.0f: NULL;
        }

        // Check if the target window is still valid
        if (!IsWindow(g_targetHwnd))
        {
            std::cout << "Target window closed. Exiting..." << std::endl;
            g_isRunning = false;
            break;
        }

        if (GetAsyncKeyState(VK_INSERT) & 1)
        {
            g_showOverlay = !g_showOverlay;
            if (g_overlayHwnd)
            {
                ::ShowWindow(g_overlayHwnd, g_showOverlay ? SW_SHOWNOACTIVATE : SW_HIDE);
                if (g_showOverlay)
                {
                    ::SetForegroundWindow(g_overlayHwnd); // Focus the overlay
                    g_isMenuOpen = true;
                    ShowCursor(TRUE); // Show the system cursor

                    // Get the ImGui window's rectangle
                    RECT imguiRect;
                    if (::GetWindowRect(g_overlayHwnd, &imguiRect))
                    {
                        // Calculate the center of the ImGui window
                        int centerX = imguiRect.left + (imguiRect.right - imguiRect.left) / 2;
                        int centerY = imguiRect.top + (imguiRect.bottom - imguiRect.top) / 2;

                        // Move the mouse cursor to the center
                        ::SetCursorPos(centerX, centerY);
                    }
                }
                else
                {
                    ::SetForegroundWindow(g_targetHwnd); // Focus the game
                    g_isMenuOpen = false;
                    ShowCursor(FALSE); // Hide the system cursor (default game behavior)
                }
            }
            else if (!g_showOverlay)
            {
                ShowCursor(FALSE); // Ensure cursor is hidden if overlay couldn't be created
            }
        }

        if (GetAsyncKeyState(VK_END))
        {
            std::cout << "END key pressed. Signaling ImGui thread to exit..." << std::endl;
            g_isRunning = false;
            break;
        }
        ::Sleep(10);
    }

    std::cout << "Waiting for ImGui thread to exit..." << std::endl;
    if (hImGuiThread != nullptr)
    {
        WaitForSingleObject(hImGuiThread, INFINITE);
        CloseHandle(hImGuiThread);
    }
    std::cout << "ImGui thread exited." << std::endl;

    if (g_overlayHwnd)
        DestroyWindow(g_overlayHwnd);
    if (f) fclose(f);
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Main, hModule, 0, nullptr));
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}