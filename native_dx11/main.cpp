#define _CRT_SECURE_NO_WARNINGS
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "glass/backdrop.h"
#include "glass/glass.h"

#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static UINT                    g_ResizeWidth = 0;
static UINT                    g_ResizeHeight = 0;
static bool                    g_SwapChainOccluded = false;
static Glass::Backdrop         g_backdrop;
static Glass::Renderer         g_glass;

static std::atomic<bool> g_busy(false);
static std::atomic<bool> g_resultReady(false);
static std::mutex        g_resultMutex;
static std::string       g_resultMessage;
static bool              g_resultOk = false;

static int  g_target = 0;
static char g_customPath[768] = {};
static bool g_openAsarSheet = false;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static std::wstring ExeDir() {
    wchar_t path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path, n);
    const size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

static std::wstring CorePath() {
    return ExeDir() + L"\\VencordArabicInstallerCore.exe";
}

static std::wstring UserDataDir() {
    wchar_t env[32768] = {};
    DWORD n = GetEnvironmentVariableW(L"VENCORD_USER_DATA_DIR", env, 32768);
    if (n > 0 && n < 32768) return std::wstring(env, n);

    n = GetEnvironmentVariableW(L"APPDATA", env, 32768);
    if (n > 0 && n < 32768) return std::wstring(env, n) + L"\\Vencord";
    return L"Vencord";
}

static std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(), out.data(), count, nullptr, nullptr);
    return out;
}

static std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(), nullptr, 0);
    std::wstring out((size_t)count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(), out.data(), count);
    return out;
}

static std::wstring QuoteArg(const std::wstring& value) {
    std::wstring out = L"\"";
    unsigned backslashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') {
            backslashes++;
            continue;
        }
        if (c == L'\"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'\"');
            backslashes = 0;
            continue;
        }
        out.append(backslashes, L'\\');
        backslashes = 0;
        out.push_back(c);
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'\"');
    return out;
}

static std::string StripAnsi(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size();) {
        if ((unsigned char)input[i] == 0x1b && i + 1 < input.size() && input[i + 1] == '[') {
            i += 2;
            while (i < input.size()) {
                const unsigned char c = (unsigned char)input[i++];
                if (c >= 0x40 && c <= 0x7e) break;
            }
            continue;
        }
        if (input[i] != '\r') out.push_back(input[i]);
        ++i;
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ' || out.back() == '\t')) out.pop_back();
    return out;
}

struct CoreResult {
    DWORD exitCode = 1;
    std::string output;
};

static CoreResult RunCore(const std::vector<std::wstring>& args) {
    CoreResult result;
    const std::wstring core = CorePath();
    if (GetFileAttributesW(core.c_str()) == INVALID_FILE_ATTRIBUTES) {
        result.output = "Internal Vencord installer core was not found.";
        return result;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        result.output = "Failed to create process pipe.";
        return result;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmd = QuoteArg(core);
    for (const std::wstring& arg : args) {
        cmd.push_back(L' ');
        cmd += QuoteArg(arg);
    }
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        core.c_str(), mutableCmd.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, ExeDir().c_str(), &si, &pi
    );
    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        result.output = "Failed to start the Vencord installer core.";
        return result;
    }

    char buf[4096];
    DWORD got = 0;
    while (ReadFile(readPipe, buf, sizeof(buf), &got, nullptr) && got > 0)
        result.output.append(buf, buf + got);

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &result.exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(readPipe);
    result.output = StripAnsi(result.output);
    if (result.output.empty())
        result.output = result.exitCode == 0 ? "Operation completed successfully." : "Operation failed.";
    return result;
}

static std::vector<std::wstring> TargetArgs(int target, const std::string& custom) {
    if (target == 3) {
        if (custom.empty()) return {};
        return { L"--location", Utf8ToWide(custom) };
    }
    static const wchar_t* branches[] = { L"stable", L"ptb", L"canary" };
    return { L"--branch", branches[target < 0 || target > 2 ? 0 : target] };
}

static void StartOperation(const wchar_t* operation) {
    if (g_busy.exchange(true)) return;
    const int target = g_target;
    const std::string custom = g_customPath;
    if (target == 3 && custom.empty()) {
        g_busy = false;
        std::lock_guard<std::mutex> lock(g_resultMutex);
        g_resultOk = false;
        g_resultMessage = "Enter a custom Discord install location first.";
        g_resultReady = true;
        return;
    }

    std::thread([operation = std::wstring(operation), target, custom]() {
        std::vector<std::wstring> args = { operation };
        auto targetArgs = TargetArgs(target, custom);
        args.insert(args.end(), targetArgs.begin(), targetArgs.end());
        CoreResult result = RunCore(args);
        {
            std::lock_guard<std::mutex> lock(g_resultMutex);
            g_resultOk = result.exitCode == 0;
            g_resultMessage = result.output;
        }
        g_busy = false;
        g_resultReady = true;
    }).detach();
}

static ImU32 WithAlpha(ImU32 rgb, int alpha) {
    return (rgb & 0x00FFFFFFu) | ((ImU32)std::clamp(alpha, 0, 255) << 24);
}

static bool TintedGlassButton(const char* id, const char* label, ImVec2 size, ImU32 tint, bool disabled = false) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    Glass::Primitive glass{};
    glass.cx = p.x + size.x * 0.5f;
    glass.cy = p.y + size.y * 0.5f;
    glass.hw = size.x * 0.5f;
    glass.hh = size.y * 0.5f;
    glass.corner_radius = 13.0f;
    glass.fade = disabled ? 0.45f : 1.0f;
    glass.elevate = 1.15f;
    glass.material = Glass::Material::Regular;
    g_glass.Submit(glass);

    ImGui::PushID(id);
    ImGui::InvisibleButton("##hit", size);
    const bool hovered = !disabled && ImGui::IsItemHovered();
    const bool active = !disabled && ImGui::IsItemActive();
    const bool clicked = !disabled && ImGui::IsItemDeactivated() && hovered;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), WithAlpha(tint, active ? 92 : (hovered ? 74 : 52)), 13.0f);
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(255,255,255, hovered ? 90 : 54), 13.0f, 0, 1.0f);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    const ImU32 tc = disabled ? IM_COL32(210,210,216,130) : IM_COL32(248,248,250,245);
    dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), tc, label);
    ImGui::PopID();
    return clicked;
}

static void DrawTintedGlassPanel(ImVec2 p, ImVec2 size, ImU32 tint, float radius, Glass::Material material) {
    Glass::Primitive glass{};
    glass.cx = p.x + size.x * 0.5f;
    glass.cy = p.y + size.y * 0.5f;
    glass.hw = size.x * 0.5f;
    glass.hh = size.y * 0.5f;
    glass.corner_radius = radius;
    glass.fade = 1.0f;
    glass.material = material;
    g_glass.Submit(glass);
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), WithAlpha(tint, 26), radius);
}

static void OpenUserDataDirectory() {
    const std::wstring dir = UserDataDir();
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void DrawInstallerUI(HWND hwnd, int cw, int ch, ImFont* body, ImFont* title) {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)cw, (float)ch), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(48, 40));
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground);

    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    DrawTintedGlassPanel(ImVec2(wp.x + 18, wp.y + 18), ImVec2(ws.x - 36, ws.y - 36), IM_COL32(18,20,27,255), 28.0f, Glass::Material::Regular);

    ImGui::PushFont(title);
    const char* heading = "Vencord Arabic Installer";
    const ImVec2 ht = ImGui::CalcTextSize(heading);
    ImGui::SetCursorPosX((ws.x - ht.x) * 0.5f);
    ImGui::TextUnformatted(heading);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 18));
    ImGui::PushFont(body);
    const std::string userDir = WideToUtf8(UserDataDir());
    ImGui::Text("Vencord Arabic will be downloaded to: %s", userDir.c_str());
    ImGui::SameLine();
    if (Glass::Button("Open Directory", false, ImVec2(132, 34))) OpenUserDataDirectory();
    ImGui::TextDisabled("To customise this location, set VENCORD_USER_DATA_DIR and restart the installer");
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Text("Installer UI: native DirectX 11 / liquidDX11");
    ImGui::Text("Core: Vencord Arabic Installer (Go)");

    ImGui::Dummy(ImVec2(0, 12));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 12));

    const ImVec2 warnPos = ImGui::GetCursorScreenPos();
    const float contentW = ImGui::GetContentRegionAvail().x;
    DrawTintedGlassPanel(warnPos, ImVec2(contentW, 84), IM_COL32(254,231,92,255), 14.0f, Glass::Material::Thin);
    ImGui::SetCursorScreenPos(ImVec2(warnPos.x + 18, warnPos.y + 14));
    ImGui::PushTextWrapPos(warnPos.x + contentW - 18);
    ImGui::TextColored(ImVec4(1.0f, 0.94f, 0.55f, 1.0f),
        "This is an unofficial Arabic-focused fork based on Vencord. It is not affiliated with Discord or the official Vencord team.\nSource code and releases: github.com/ShadowUR0. Use client modifications at your own risk.");
    ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(ImVec2(warnPos.x, warnPos.y + 96));

    ImGui::Text("Please select an install to patch");
    ImGui::Dummy(ImVec2(0, 6));
    static const char* targets[] = { "Stable", "PTB", "Canary", "Custom Install Location" };
    g_target = Glass::RadioGroup("discord-target", targets, 4, g_target);

    ImGui::Dummy(ImVec2(0, 4));
    Glass::TextField("Custom Location", g_customPath, (int)sizeof(g_customPath), "The custom location", Glass::Icon::FolderOpen);

    ImGui::Dummy(ImVec2(0, 18));
    const float gap = 10.0f;
    const float bw = (ImGui::GetContentRegionAvail().x - gap * 3.0f) / 4.0f;
    const bool disabled = g_busy.load();
    if (TintedGlassButton("install", "Install", ImVec2(bw, 52), IM_COL32(45,124,70,255), disabled))
        StartOperation(L"--install");
    ImGui::SameLine(0, gap);
    if (TintedGlassButton("repair", "Reinstall / Repair", ImVec2(bw, 52), IM_COL32(88,101,242,255), disabled))
        StartOperation(L"--repair");
    ImGui::SameLine(0, gap);
    if (TintedGlassButton("uninstall", "Uninstall", ImVec2(bw, 52), IM_COL32(236,65,68,255), disabled))
        StartOperation(L"--uninstall");
    ImGui::SameLine(0, gap);
    if (TintedGlassButton("openasar", "OpenAsar", ImVec2(bw, 52), IM_COL32(254,231,92,255), disabled))
        g_openAsarSheet = true;

    if (g_busy.load()) {
        ImGui::Dummy(ImVec2(0, 18));
        Glass::Spinner("core-busy", 12.0f, IM_COL32(255,255,255,235));
        ImGui::SameLine();
        ImGui::TextDisabled("Working... keep Discord fully closed while files are being modified");
    }

    if (g_openAsarSheet) {
        static const char* items[] = { "Install OpenAsar", "Uninstall OpenAsar" };
        Glass::ActionSheetOpen("OpenAsar", items, 2, 1);
        g_openAsarSheet = false;
    }
    const int sheetResult = Glass::DrawActionSheet();
    if (sheetResult == 0) StartOperation(L"--install-openasar");
    if (sheetResult == 1) StartOperation(L"--uninstall-openasar");

    if (g_resultReady.exchange(false)) {
        std::lock_guard<std::mutex> lock(g_resultMutex);
        Glass::AlertOpen(g_resultOk ? "Success" : "Operation failed", g_resultMessage.c_str(), "OK", nullptr, !g_resultOk);
    }
    (void)Glass::DrawAlert();

    ImGui::Dummy(ImVec2(0, 12));
    ImGui::TextDisabled("Liquid glass engine: liquidDX11 by poncipp (MIT)  |  Installer core: GPL-3.0");

    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    ImGui_ImplWin32_EnableDpiAwareness();
    const float dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{0,0}, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance,
                       LoadIcon(nullptr, IDI_APPLICATION), LoadCursor(nullptr, IDC_ARROW),
                       nullptr, nullptr, L"VencordArabicLiquidDX11", nullptr };
    RegisterClassExW(&wc);

    const int winW = (int)(1200 * dpiScale);
    const int winH = (int)(800 * dpiScale);
    RECT desktop{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktop, 0);
    const int x = desktop.left + ((desktop.right - desktop.left) - winW) / 2;
    const int y = desktop.top + ((desktop.bottom - desktop.top) - winH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED,
        wc.lpszClassName,
        L"Vencord Arabic Installer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, winW, winH,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) return 1;

    SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 255, LWA_ALPHA);
    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    if (!CreateDeviceD3D(hwnd)) {
        MessageBoxW(hwnd, L"Direct3D 11 initialisation failed.", L"Vencord Arabic Installer", MB_ICONERROR);
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    SetWindowDisplayAffinity(hwnd, 0x11u);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowRounding = 0.0f;
    style.FrameRounding = 12.0f;
    style.ChildRounding = 14.0f;
    style.PopupRounding = 16.0f;
    style.ScrollbarRounding = 12.0f;
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
    style.ScaleAllSizes(dpiScale);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    const float bodySize = 18.0f * dpiScale;
    const float titleSize = 34.0f * dpiScale;
    ImFont* body = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", bodySize);
    ImFont* title = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", titleSize);
    if (!body) body = io.Fonts->AddFontDefault();
    if (!title) title = body;
    io.FontDefault = body;

    if (!g_glass.Init(g_pd3dDevice, g_pd3dDeviceContext)) {
        MessageBoxW(hwnd, L"liquidDX11 glass renderer initialisation failed.", L"Vencord Arabic Installer", MB_ICONERROR);
        return 1;
    }
    Glass::g = &g_glass;
    if (!g_backdrop.Init(g_pd3dDevice, g_pd3dDeviceContext, hwnd)) {
        MessageBoxW(hwnd, L"Desktop backdrop capture could not be initialised. The current display/session may not support DXGI Desktop Duplication.", L"Vencord Arabic Installer", MB_ICONERROR);
        return 1;
    }

    Glass::SetAccent(0.345f, 0.396f, 0.949f);
    Glass::SetGlobalMaterial(0.95f, 0.62f, 1.08f, 1.0f, 0.72f, 1.0f, 0.95f);
    Glass::SetLiquidFlow(0.0f);
    Glass::GlassEdgeConfig edge{};
    edge.smooth_refraction = 1.0f;
    edge.lens_amount = 0.72f;
    edge.lens_power = 2.15f;
    edge.cursor_glow = 0.10f;
    edge.ambient_rim = 0.62f;
    edge.specular_amt = 1.08f;
    g_glass.SetEdgeConfig(edge);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        g_backdrop.Capture();
        RECT cr{}, wr{};
        GetClientRect(hwnd, &cr);
        GetWindowRect(hwnd, &wr);
        const int cw = cr.right - cr.left;
        const int ch = cr.bottom - cr.top;
        const int originX = wr.left - g_backdrop.originX();
        const int originY = wr.top - g_backdrop.originY();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (io.DeltaTime > 0.05f) io.DeltaTime = 1.0f / 30.0f;

        const ImVec2 cursorLocal(io.MousePos.x - (float)wr.left, io.MousePos.y - (float)wr.top);
        g_glass.BeginFrame(cw, ch, originX, originY, g_backdrop.width(), g_backdrop.height(), cursorLocal);

        DrawInstallerUI(hwnd, cw, ch, body, title);

        ImGui::Render();
        const float clear[4] = { 0, 0, 0, 0 };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        D3D11_VIEWPORT vp{};
        vp.Width = (float)cw;
        vp.Height = (float)ch;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        g_pd3dDeviceContext->RSSetViewports(1, &vp);
        g_glass.Render(g_backdrop.heavySRV(), g_backdrop.softSRV());
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        const HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    g_backdrop.Shutdown();
    g_glass.Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, hInstance);
    return 0;
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel{};
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
        &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
            &featureLevel, &g_pd3dDeviceContext);
    }
    if (FAILED(res)) return false;
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer) {
        g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
        backBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
