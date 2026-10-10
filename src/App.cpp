// src/App.cpp
#include "App.h"

#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "parser/AST.h"
#include "codegen/BytecodeGen.h"
#include "vm/VM.h"

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <memory>

#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ============================================================
// D3D11
// ============================================================
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HWND                     g_hwnd = nullptr;

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
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

    UINT flags = 0;
    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);

    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            flags, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);

    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCCALCSIZE:
        if (wParam == TRUE) return 0;
        break;

    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rc; GetWindowRect(hWnd, &rc);
        const int B = 6;
        bool left = pt.x < rc.left + B, right = pt.x > rc.right - B;
        bool top = pt.y < rc.top + B, bottom = pt.y > rc.bottom - B;
        if (top && left)     return HTTOPLEFT;
        if (top && right)    return HTTOPRIGHT;
        if (bottom && left)  return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left)   return HTLEFT;
        if (right)  return HTRIGHT;
        if (top)    return HTTOP;
        if (bottom) return HTBOTTOM;
        break;
    }

    case WM_CLOSE:
        ::PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================
namespace mayami {

    static ImVec4 g_accent = ImVec4(0.55f, 0.35f, 1.00f, 1.00f);
    static ImVec4 g_accentPink = ImVec4(1.00f, 0.35f, 0.65f, 1.00f);
    static ImVec4 g_textMain = ImVec4(0.90f, 0.90f, 0.94f, 1.00f);
    static ImVec4 g_textDim = ImVec4(0.55f, 0.55f, 0.65f, 1.00f);
    static ImVec4 g_danger = ImVec4(0.85f, 0.30f, 0.35f, 1.00f);
    static ImVec4 g_warning = ImVec4(1.00f, 0.75f, 0.35f, 1.00f);
    static ImVec4 g_success = ImVec4(0.35f, 0.85f, 0.50f, 1.00f);

    // ============================================================
    // Запуск процесса через CreateProcess
    // ============================================================
    static std::string runAndCapture(const std::string& cmdLine) {
        std::string result;

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hRead = nullptr, hWrite = nullptr;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
            return "[ERROR] CreatePipe failed\n";
        }
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi{};

        std::vector<char> buf(cmdLine.begin(), cmdLine.end());
        buf.push_back('\0');

        BOOL ok = CreateProcessA(
            nullptr,
            buf.data(),
            nullptr, nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi
        );

        if (!ok) {
            DWORD err = GetLastError();
            CloseHandle(hRead);
            CloseHandle(hWrite);
            return "[ERROR] CreateProcess failed (code " +
                std::to_string(err) + ")\n";
        }

        CloseHandle(hWrite);

        char buffer[4096];
        DWORD read = 0;
        while (ReadFile(hRead, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
            result.append(buffer, read);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hRead);

        return result;
    }

    static std::string trimEol(std::string s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        return s;
    }

    static std::string getBinDir() {
        char buf[MAX_PATH];
        DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
        if (n > 0) {
            std::string path(buf, n);
            size_t pos = path.find_last_of("\\/");
            if (pos != std::string::npos)
                return path.substr(0, pos);
        }
        return ".";
    }

    static std::string normalizeNewlines(const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (char c : s) {
            if (c != '\r') r.push_back(c);
        }
        return r;
    }

    static long fileSize(const std::string& path) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return -1;
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fclose(f);
        return sz;
    }

    // ============================================================
    App::App() {}
    App::~App() { shutdown(); }

    // ─── Init Win32 ─────────────────────────────────────────
    bool App::initWin32() {
        WNDCLASSEXW wc = {
            sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
            GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
            L"mayami_ide", nullptr
        };
        ::RegisterClassExW(&wc);

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        const int ww = 1360;
        const int wh = 800;
        int wx = (sw - ww) / 2;
        int wy = (sh - wh) / 2;

        HWND hwnd = ::CreateWindowExW(
            0, wc.lpszClassName, L"Mayami IDE",
            WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
            wx, wy, ww, wh,
            nullptr, nullptr, wc.hInstance, nullptr);

        if (!hwnd) return false;

        g_hwnd = hwnd;
        m_hwnd = hwnd;

        int corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

        if (!CreateDeviceD3D(hwnd)) {
            CleanupDeviceD3D();
            ::DestroyWindow(hwnd);
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);
        return true;
    }

    // ─── Init ImGui ─────────────────────────────────────────
    bool App::initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;

        applyTheme();
        applyFonts();

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

        EditorTab tab;
        tab.title = "welcome.mayami";
        tab.path = "";
        tab.editor.SetText(
            "// Welcome to Mayami IDE!\n"
            "\n"
            "fn main() {\n"
            "    x = 42;\n"
            "    name = \"Mayami\";\n"
            "    p(\"Hello, \" + name + \"!\");\n"
            "    p(\"x * 2 =\", x * 2);\n"
            "}\n");
        tab.savedContent = tab.editor.GetText();
        tab.editor.SetPalette(TextEditor::GetDarkPalette());
        tab.editor.SetLanguage(TextEditor::Language::Cpp());
        m_tabs.push_back(std::move(tab));
        m_activeTab = 0;

        m_projectRoot = "D:/Minecraft/mayami";
        if (FileSystem::exists(m_projectRoot)) {
            m_fileTreeRoot = FileSystem::scanDirectory(m_projectRoot);
            log("Project loaded: " + m_projectRoot);
        }

        log("Mayami IDE v0.5.0 initialized");
        log("F5 = Build+Run | Ctrl+B = Build | Ctrl+R = Run | Ctrl+F5 = Quick Run");
        log("Ctrl+Shift+F5 = Toggle DEBUG mode");
        return true;
    }

    int App::run() {
        if (!initWin32()) return 1;
        if (!initImGui()) { shutdown(); return 2; }
        mainLoop();
        shutdown();
        return 0;
    }

    // ─── Main loop ──────────────────────────────────────────
    void App::mainLoop() {
        bool done = false;
        while (!done) {
            MSG msg;
            while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
                if (msg.message == WM_QUIT) done = true;
            }
            if (done) break;

            if (!::IsWindow(g_hwnd)) { done = true; break; }

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            drawUI();

            ImGui::Render();

            ImVec4 bg = ImVec4(0.051f, 0.051f, 0.078f, 1.0f);
            const float clear[4] = { bg.x, bg.y, bg.z, 1.0f };

            if (g_mainRenderTargetView && g_pd3dDeviceContext && g_pSwapChain) {
                g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
                g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                g_pSwapChain->Present(1, 0);
            }
        }
    }

    // ★ F5 = Build+Run, Ctrl+Shift+F5 = toggle debug
    void App::drawUI() {
        ImGuiIO& io = ImGui::GetIO();

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false)) {
            m_showTerminalWindow = !m_showTerminalWindow;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
            if (io.KeyCtrl && io.KeyShift) {
                // ★ Ctrl+Shift+F5 — переключить debug
                m_debugMode = !m_debugMode;
                log(m_debugMode ? "=== DEBUG MODE ON ===" : "=== DEBUG MODE OFF ===");
            }
            else if (io.KeyCtrl) {
                runCode();
            }
            else {
                buildExe();
                if (!m_lastBuiltExe.empty() && fileSize(m_lastBuiltExe) > 0) {
                    runExe();
                }
            }
        }

        if (io.KeyCtrl && !io.KeyShift) {
            if (ImGui::IsKeyPressed(ImGuiKey_S, false)) saveFile();
            else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) openFile();
            else if (ImGui::IsKeyPressed(ImGuiKey_N, false)) newFile();
            else if (ImGui::IsKeyPressed(ImGuiKey_B, false)) buildExe();
            else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) runExe();
            else if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
                if (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size()) {
                    m_tabs.erase(m_tabs.begin() + m_activeTab);
                    if (m_activeTab >= (int)m_tabs.size())
                        m_activeTab = (int)m_tabs.size() - 1;
                }
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
                if (!m_tabs.empty())
                    m_activeTab = (m_activeTab + 1) % (int)m_tabs.size();
            }
        }

        drawTitleBar();
        drawMenuBar();
        drawFileTree();
        drawEditor();
        drawStatusBar();

        drawTerminalWindow();
        drawAboutModal();
        drawNewFileDialog();
        drawNewFolderDialog();
    }

    // ─── Title bar ──────────────────────────────────────────
    void App::drawTitleBar() {
        ImGuiIO& io = ImGui::GetIO();
        const float barHeight = m_titleBarHeight;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, barHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 2));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f, 0.086f, 0.133f, 1.0f));

        ImGui::Begin("##TitleBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImVec2 ws = ImGui::GetWindowSize();
            dl->AddRectFilledMultiColor(
                ImVec2(wp.x, wp.y + ws.y - 2),
                ImVec2(wp.x + ws.x, wp.y + ws.y),
                ImGui::ColorConvertFloat4ToU32(g_accent),
                ImGui::ColorConvertFloat4ToU32(g_accentPink),
                ImGui::ColorConvertFloat4ToU32(g_accentPink),
                ImGui::ColorConvertFloat4ToU32(g_accent));
        }

        const float btnSize = 36.0f;
        const float spacing = 6.0f;
        float totalW = btnSize * 3 + spacing * 2;
        float startX = io.DisplaySize.x - totalW - 10.0f;

        ImGui::SetCursorPosX(startX);
        ImGui::SetCursorPosY((barHeight - btnSize) * 0.5f);

        auto iconBtn = [&](const char* id, const char* glyph, ImVec4 hoverCol) -> bool {
            ImGui::PushID(id);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, hoverCol);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            if (m_fontIcon) ImGui::PushFont(m_fontIcon);
            bool clicked = ImGui::Button(glyph, ImVec2(btnSize, btnSize));
            if (m_fontIcon) ImGui::PopFont();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
            ImGui::PopID();
            return clicked;
            };

        if (iconBtn("min", "\xE2\x80\x94", ImVec4(0.85f, 0.75f, 0.30f, 1.0f)))
            ::ShowWindow(g_hwnd, SW_MINIMIZE);

        ImGui::SameLine(0, spacing);
        if (iconBtn("max", "\xE2\x96\xA2", ImVec4(0.30f, 0.60f, 0.85f, 1.0f))) {
            if (::IsZoomed(g_hwnd)) ::ShowWindow(g_hwnd, SW_RESTORE);
            else                    ::ShowWindow(g_hwnd, SW_MAXIMIZE);
        }

        ImGui::SameLine(0, spacing);
        if (iconBtn("close", "\xE2\x9C\x95", ImVec4(0.85f, 0.20f, 0.25f, 1.0f)))
            ::PostMessageW(g_hwnd, WM_CLOSE, 0, 0);

        {
            const char* title = "Mayami IDE";
            float tw = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPosX((io.DisplaySize.x - tw) * 0.5f);
            ImGui::SetCursorPosY((barHeight - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::TextColored(g_textMain, "%s", title);
        }

        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##drag", ImVec2(startX, barHeight));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 d = io.MouseDelta;
            RECT rc; GetWindowRect(g_hwnd, &rc);
            SetWindowPos(g_hwnd, nullptr, rc.left + (int)d.x, rc.top + (int)d.y,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        ImGui::End();
    }

    // ─── Menu bar ───────────────────────────────────────────
    void App::drawMenuBar() {
        ImGuiIO& io = ImGui::GetIO();
        float y = m_titleBarHeight;

        ImGui::SetNextWindowPos(ImVec2(0, y));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, m_menuBarHeight));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 2));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.16f, 1.0f));

        ImGui::Begin("##MenuBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N"))          newFile();
                if (ImGui::MenuItem("Open File...", "Ctrl+O")) openFile();
                if (ImGui::MenuItem("Open Project..."))        openProject();
                if (ImGui::MenuItem("Save", "Ctrl+S"))         saveFile();
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4"))
                    ::PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                ImGui::MenuItem("Undo", "Ctrl+Z");
                ImGui::MenuItem("Redo", "Ctrl+Y");
                ImGui::Separator();
                ImGui::MenuItem("Cut", "Ctrl+X");
                ImGui::MenuItem("Copy", "Ctrl+C");
                ImGui::MenuItem("Paste", "Ctrl+V");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Terminal", "Ctrl+`", m_showTerminalWindow))
                    m_showTerminalWindow = !m_showTerminalWindow;
                ImGui::Separator();
                if (ImGui::MenuItem("Dark Theme", nullptr, m_theme == Theme::Dark)) {
                    m_theme = Theme::Dark; applyTheme();
                }
                if (ImGui::MenuItem("Light Theme", nullptr, m_theme == Theme::Light)) {
                    m_theme = Theme::Light; applyTheme();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Run")) {
                if (ImGui::MenuItem("Build + Run EXE", "F5")) {
                    buildExe();
                    if (!m_lastBuiltExe.empty() && fileSize(m_lastBuiltExe) > 0) {
                        runExe();
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Build EXE", "Ctrl+B"))              buildExe();
                if (ImGui::MenuItem("Run EXE", "Ctrl+R"))                runExe();
                if (ImGui::MenuItem("Quick Run (in-memory)", "Ctrl+F5")) runCode();
                ImGui::Separator();
                if (ImGui::MenuItem("Toggle DEBUG", "Ctrl+Shift+F5", m_debugMode)) {
                    m_debugMode = !m_debugMode;
                    log(m_debugMode ? "=== DEBUG MODE ON ===" : "=== DEBUG MODE OFF ===");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Build Log"))  clearBuildLog();
                if (ImGui::MenuItem("Clear Output Log")) clearOutputLog();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About"))  m_showAbout = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    // ─── File tree ──────────────────────────────────────────
    static void DrawFileNode(const FileEntry& entry,
        const std::string& selectedPath,
        std::string& outSelected,
        bool& outOpen)
    {
        ImGui::PushID(entry.path.c_str());

        if (entry.isDirectory) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                ImGuiTreeNodeFlags_SpanAvailWidth;
            if (entry.path == selectedPath)
                flags |= ImGuiTreeNodeFlags_Selected;

            bool open = ImGui::TreeNodeEx(entry.name.c_str(), flags);
            if (ImGui::IsItemClicked())
                outSelected = entry.path;

            if (open) {
                for (const auto& child : entry.children)
                    DrawFileNode(child, selectedPath, outSelected, outOpen);
                ImGui::TreePop();
            }
        }
        else {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanAvailWidth;
            if (entry.path == selectedPath)
                flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(entry.name.c_str(), flags);
            if (ImGui::IsItemClicked()) {
                outSelected = entry.path;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    outOpen = true;
            }
        }

        ImGui::PopID();
    }

    void App::drawFileTree() {
        ImGuiIO& io = ImGui::GetIO();
        float y = m_titleBarHeight + m_menuBarHeight;
        float h = io.DisplaySize.y - y - m_statusBarHeight;

        ImGui::SetNextWindowPos(ImVec2(0, y));
        ImGui::SetNextWindowSize(ImVec2(m_sidePanelWidth, h));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("##FileTree", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextColored(g_textDim, "EXPLORER");
        ImGui::SameLine(ImGui::GetWindowWidth() - 110);

        if (ImGui::SmallButton("+F")) {
            m_showNewFileDialog = true;
            m_newFileName[0] = 0;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("New File");

        ImGui::SameLine();
        if (ImGui::SmallButton("+D")) {
            m_showNewFolderDialog = true;
            m_newFolderName[0] = 0;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("New Folder");

        ImGui::SameLine();
        if (ImGui::SmallButton("R")) {
            if (!m_projectRoot.empty())
                m_fileTreeRoot = FileSystem::scanDirectory(m_projectRoot);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh");

        ImGui::Separator();

        if (m_projectRoot.empty()) {
            ImGui::TextColored(g_textDim, "No project opened");
            ImGui::Spacing();
            if (ImGui::Button("Open Project...", ImVec2(-1, 0)))
                openProject();
        }
        else {
            ImGui::TextColored(g_accent, "%s",
                FileSystem::getFileName(m_projectRoot).c_str());
            ImGui::Spacing();

            std::string newSelected = m_selectedPath;
            bool openRequested = false;

            DrawFileNode(m_fileTreeRoot, m_selectedPath, newSelected, openRequested);

            if (newSelected != m_selectedPath)
                m_selectedPath = newSelected;

            if (openRequested && !m_selectedPath.empty() &&
                !FileSystem::isDirectory(m_selectedPath)) {
                openFileInEditor(m_selectedPath);
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    // ─── Open file in editor ────────────────────────────────
    void App::openFileInEditor(const std::string& path) {
        for (auto& tab : m_tabs) {
            if (tab.path == path) return;
        }

        std::string content = FileSystem::readFile(path);

        EditorTab tab;
        tab.title = FileSystem::getFileName(path);
        tab.path = path;
        tab.editor.SetText(content);
        tab.savedContent = content;
        tab.editor.SetPalette(m_theme == Theme::Dark
            ? TextEditor::GetDarkPalette()
            : TextEditor::GetLightPalette());
        tab.editor.SetLanguage(TextEditor::Language::Cpp());

        m_tabs.push_back(std::move(tab));
        m_activeTab = (int)m_tabs.size() - 1;

        clearOutputLog();
        log("Opened: " + path);
    }

    // ─── Editor ─────────────────────────────────────────────
    void App::drawEditor() {
        ImGuiIO& io = ImGui::GetIO();
        float y = m_titleBarHeight + m_menuBarHeight;
        float x = m_sidePanelWidth;
        float w = io.DisplaySize.x - x;
        float h = io.DisplaySize.y - y - m_statusBarHeight;

        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(w, h));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("##EditorArea", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        if (ImGui::BeginTabBar("##EditorTabs",
            ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {

            for (size_t i = 0; i < m_tabs.size(); ++i) {
                auto& tab = m_tabs[i];
                bool open = true;

                ImGuiTabItemFlags flags = 0;
                if (tab.modified) flags |= ImGuiTabItemFlags_UnsavedDocument;

                if (ImGui::BeginTabItem(tab.title.c_str(), &open, flags)) {
                    m_activeTab = (int)i;

                    std::string current = normalizeNewlines(tab.editor.GetText());
                    std::string saved = normalizeNewlines(tab.savedContent);
                    tab.modified = (current != saved);

                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    std::string editorId = "##editor_" + std::to_string(i);
                    tab.editor.Render(editorId.c_str(), avail, false);
                    ImGui::EndTabItem();
                }

                if (!open) {
                    m_tabs.erase(m_tabs.begin() + i);
                    --i;
                    if (m_activeTab >= (int)m_tabs.size())
                        m_activeTab = (int)m_tabs.size() - 1;
                }
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
    }

    // ─── Terminal Window ────────────────────────────────────
    void App::drawTerminalWindow() {
        if (!m_showTerminalWindow) return;

        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(
            ImVec2(io.DisplaySize.x - 80, io.DisplaySize.y - 80),
            ImGuiCond_Appearing);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.10f, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.25f, 0.65f, 1.0f));

        bool open = true;
        if (ImGui::Begin("Terminal##window", &open,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {

            ImGui::TextColored(g_accent, "MAYAMI TERMINAL");
            ImGui::SameLine(0, 20);

            if (ImGui::SmallButton("Clear All")) {
                clearBuildLog();
                clearOutputLog();
            }

            ImGui::SameLine(0, 20);
            ImGui::TextColored(g_textDim,
                "Build: %d  |  Output: %d  |  Debug: %s",
                (int)m_buildLog.size(), (int)m_outputLog.size(),
                m_debugMode ? "ON" : "OFF");

            float closeW = ImGui::CalcTextSize("Close").x + 20;
            ImGui::SameLine(ImGui::GetWindowWidth() - closeW - 16);
            if (ImGui::Button("Close")) {
                m_showTerminalWindow = false;
                open = false;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+`");

            ImGui::Separator();

            if (ImGui::BeginTabBar("##TermTabs", ImGuiTabBarFlags_None)) {

                std::string buildTitle = "BUILD (" + std::to_string(m_buildLog.size()) + ")";
                if (ImGui::BeginTabItem(buildTitle.c_str())) {
                    m_terminalTab = 0;

                    if (ImGui::SmallButton("Clear##build")) clearBuildLog();
                    ImGui::SameLine();
                    ImGui::Checkbox("Auto-scroll##build", &m_autoScrollBuild);
                    ImGui::SameLine();
                    ImGui::TextColored(g_textDim, "Лог компилятора");

                    ImGui::Separator();

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.03f, 0.05f, 1.0f));
                    ImGui::BeginChild("##build_scroll", ImVec2(0, 0), false,
                        ImGuiWindowFlags_HorizontalScrollbar);

                    if (m_buildLog.empty()) {
                        ImGui::TextColored(g_textDim, "F5 = Build+Run | Ctrl+B = Build | Ctrl+R = Run | Ctrl+F5 = Quick");
                    }
                    else {
                        for (const auto& line : m_buildLog) {
                            ImVec4 col = g_textMain;
                            if (line.find("[ERROR]") != std::string::npos) col = g_danger;
                            else if (line.find("[WARN]") != std::string::npos) col = g_warning;
                            else if (line.find("[OK]") != std::string::npos) col = g_success;
                            else if (line.find("[VM]") != std::string::npos) col = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);

                            ImGui::PushStyleColor(ImGuiCol_Text, col);
                            ImGui::TextUnformatted(line.c_str());
                            ImGui::PopStyleColor();
                        }
                    }

                    if (m_autoScrollBuild && !ImGui::IsWindowHovered())
                        ImGui::SetScrollHereY(1.0f);

                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::EndTabItem();
                }

                std::string outputTitle = "OUTPUT (" + std::to_string(m_outputLog.size()) + ")";
                if (ImGui::BeginTabItem(outputTitle.c_str())) {
                    m_terminalTab = 1;

                    if (ImGui::SmallButton("Clear##out")) clearOutputLog();
                    ImGui::SameLine();
                    ImGui::Checkbox("Auto-scroll##out", &m_autoScrollOutput);
                    ImGui::SameLine();
                    ImGui::TextColored(g_textDim, "Вывод программы");

                    ImGui::Separator();

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.03f, 0.05f, 1.0f));
                    ImGui::BeginChild("##output_scroll", ImVec2(0, 0), false,
                        ImGuiWindowFlags_HorizontalScrollbar);

                    if (m_outputLog.empty()) {
                        ImGui::TextColored(g_textDim,
                            "Здесь появится вывод программы (p)");
                    }
                    else {
                        for (const auto& line : m_outputLog) {
                            ImVec4 col = g_textMain;
                            if (line.find("[stderr]") != std::string::npos)
                                col = g_danger;

                            ImGui::PushStyleColor(ImGuiCol_Text, col);
                            ImGui::TextUnformatted(line.c_str());
                            ImGui::PopStyleColor();
                        }
                    }

                    if (m_autoScrollOutput && !ImGui::IsWindowHovered())
                        ImGui::SetScrollHereY(1.0f);

                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);

        if (!open) m_showTerminalWindow = false;
    }

    // ─── Status bar ─────────────────────────────────────────
    void App::drawStatusBar() {
        ImGuiIO& io = ImGui::GetIO();
        float h = m_statusBarHeight;
        float y = io.DisplaySize.y - h;

        ImGui::SetNextWindowPos(ImVec2(0, y));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, h));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 4));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f, 0.086f, 0.133f, 1.0f));

        ImGui::Begin("##StatusBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::TextColored(g_textDim, "Mayami IDE");
        ImGui::SameLine(0, 20);
        ImGui::TextColored(g_textDim, "v0.5.0");

        ImGui::SameLine(0, 20);
        if (m_debugMode)
            ImGui::TextColored(g_warning, "● DEBUG ON");
        else
            ImGui::TextColored(g_textDim, "○ DEBUG OFF (Ctrl+Shift+F5)");

        ImGui::SameLine(0, 20);
        if (!m_lastBuiltExe.empty())
            ImGui::TextColored(g_success, "● EXE ready");
        else
            ImGui::TextColored(g_textDim, "○ EXE not built");

        ImGui::SameLine(0, 20);
        if (m_showTerminalWindow)
            ImGui::TextColored(g_success, "● Terminal open");
        else
            ImGui::TextColored(g_textDim, "○ Terminal closed (Ctrl+`)");

        const char* right = "Mayami | UTF-8 | LF";
        float rw = ImGui::CalcTextSize(right).x;
        ImGui::SetCursorPosX(io.DisplaySize.x - rw - 10);
        ImGui::TextColored(g_textDim, "%s", right);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    // ─── About modal ────────────────────────────────────────
    void App::drawAboutModal() {
        if (m_showAbout) ImGui::OpenPopup("About Mayami IDE");

        if (ImGui::BeginPopupModal("About Mayami IDE", &m_showAbout,
            ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::TextColored(g_accent, "Mayami IDE");
            ImGui::TextColored(g_textDim, "v0.5.0");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextWrapped("A modern IDE for the Mayami programming language.");
            ImGui::TextWrapped("Fast as C++, safe as Rust, simple as Python.");
            ImGui::Spacing();
            ImGui::TextColored(g_textDim, "Built with ImGui + DirectX 11");
            ImGui::TextColored(g_textDim, "F5 = Build+Run  |  Ctrl+B = Build  |  Ctrl+R = Run");
            ImGui::TextColored(g_textDim, "Ctrl+F5 = Quick Run  |  Ctrl+Shift+F5 = Toggle Debug");
            ImGui::Spacing();

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                m_showAbout = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    // ─── New File dialog ────────────────────────────────────
    void App::drawNewFileDialog() {
        if (m_showNewFileDialog) ImGui::OpenPopup("New File##modal");

        if (ImGui::BeginPopupModal("New File##modal", &m_showNewFileDialog,
            ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::Text("Create new file in:");
            ImGui::TextColored(g_textDim, "%s",
                m_projectRoot.empty() ? "(no project)" : m_projectRoot.c_str());
            ImGui::Spacing();
            ImGui::SetNextItemWidth(300);
            bool enter = ImGui::InputText("##newfile_name", m_newFileName,
                sizeof(m_newFileName), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if ((ImGui::Button("Create", ImVec2(120, 0)) || enter)
                && m_newFileName[0] != 0 && !m_projectRoot.empty()) {

                std::string path = FileSystem::joinPath(m_projectRoot, m_newFileName);
                std::string content =
                    "// " + std::string(m_newFileName) + "\n\n"
                    "fn main() {\n    \n}\n";

                if (FileSystem::createFile(path, content)) {
                    logSuccess("Created: " + path);
                    m_fileTreeRoot = FileSystem::scanDirectory(m_projectRoot);
                    openFileInEditor(path);
                }
                else logError("Failed to create: " + path);

                m_showNewFileDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_showNewFileDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    // ─── New Folder dialog ──────────────────────────────────
    void App::drawNewFolderDialog() {
        if (m_showNewFolderDialog) ImGui::OpenPopup("New Folder##modal");

        if (ImGui::BeginPopupModal("New Folder##modal", &m_showNewFolderDialog,
            ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::Text("Create new folder in:");
            ImGui::TextColored(g_textDim, "%s",
                m_projectRoot.empty() ? "(no project)" : m_projectRoot.c_str());
            ImGui::Spacing();
            ImGui::SetNextItemWidth(300);
            bool enter = ImGui::InputText("##newfolder_name", m_newFolderName,
                sizeof(m_newFolderName), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if ((ImGui::Button("Create", ImVec2(120, 0)) || enter)
                && m_newFolderName[0] != 0 && !m_projectRoot.empty()) {

                std::string path = FileSystem::joinPath(m_projectRoot, m_newFolderName);
                if (FileSystem::createDirectory(path)) {
                    logSuccess("Created folder: " + path);
                    m_fileTreeRoot = FileSystem::scanDirectory(m_projectRoot);
                }
                else logError("Failed to create: " + path);

                m_showNewFolderDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_showNewFolderDialog = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    // ─── Theme ──────────────────────────────────────────────
    void App::applyTheme() {
        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4* c = s.Colors;

        s.WindowRounding = 6.0f;
        s.ChildRounding = 4.0f;
        s.FrameRounding = 4.0f;
        s.PopupRounding = 4.0f;
        s.ScrollbarRounding = 8.0f;
        s.GrabRounding = 4.0f;
        s.TabRounding = 4.0f;

        s.WindowBorderSize = 0.0f;
        s.ChildBorderSize = 1.0f;
        s.FrameBorderSize = 0.0f;
        s.PopupBorderSize = 1.0f;

        s.WindowPadding = ImVec2(10, 10);
        s.FramePadding = ImVec2(8, 4);
        s.ItemSpacing = ImVec2(6, 5);
        s.ItemInnerSpacing = ImVec2(5, 5);
        s.ScrollbarSize = 11.0f;
        s.GrabMinSize = 9.0f;

        if (m_theme == Theme::Dark) {
            c[ImGuiCol_Text] = g_textMain;
            c[ImGuiCol_TextDisabled] = g_textDim;
            c[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.086f, 0.133f, 1.0f);
            c[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.10f, 1.0f);
            c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.15f, 0.98f);
            c[ImGuiCol_Border] = ImVec4(0.24f, 0.24f, 0.33f, 0.70f);
            c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.13f, 0.20f, 1.0f);
            c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.30f, 1.0f);
            c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.20f, 0.45f, 1.0f);
            c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.13f, 1.0f);
            c[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.10f, 0.24f, 1.0f);
            c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.15f, 1.0f);
            c[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.06f, 0.10f, 1.0f);
            c[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.24f, 0.33f, 1.0f);
            c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.30f, 0.55f, 1.0f);
            c[ImGuiCol_ScrollbarGrabActive] = g_accent;
            c[ImGuiCol_CheckMark] = g_accent;
            c[ImGuiCol_SliderGrab] = g_accent;
            c[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.48f, 1.00f, 1.0f);
            c[ImGuiCol_Button] = ImVec4(0.20f, 0.18f, 0.32f, 1.0f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.26f, 0.55f, 1.0f);
            c[ImGuiCol_ButtonActive] = ImVec4(0.42f, 0.32f, 0.75f, 1.0f);
            c[ImGuiCol_Header] = ImVec4(0.24f, 0.20f, 0.42f, 1.0f);
            c[ImGuiCol_HeaderHovered] = ImVec4(0.32f, 0.26f, 0.55f, 1.0f);
            c[ImGuiCol_HeaderActive] = ImVec4(0.42f, 0.32f, 0.75f, 1.0f);
            c[ImGuiCol_Separator] = ImVec4(0.24f, 0.24f, 0.33f, 0.70f);
            c[ImGuiCol_Tab] = ImVec4(0.13f, 0.13f, 0.20f, 1.0f);
            c[ImGuiCol_TabHovered] = ImVec4(0.32f, 0.26f, 0.55f, 1.0f);
            c[ImGuiCol_TabActive] = ImVec4(0.24f, 0.20f, 0.42f, 1.0f);
            c[ImGuiCol_NavHighlight] = g_accent;
        }
        else {
            c[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.15f, 1.0f);
            c[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.52f, 1.0f);
            c[ImGuiCol_WindowBg] = ImVec4(0.97f, 0.97f, 0.99f, 1.0f);
            c[ImGuiCol_ChildBg] = ImVec4(0.94f, 0.94f, 0.97f, 1.0f);
            c[ImGuiCol_PopupBg] = ImVec4(0.99f, 0.99f, 1.00f, 1.0f);
            c[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.86f, 1.0f);
            c[ImGuiCol_FrameBg] = ImVec4(0.91f, 0.91f, 0.95f, 1.0f);
            c[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.85f, 0.92f, 1.0f);
            c[ImGuiCol_FrameBgActive] = ImVec4(0.78f, 0.75f, 0.92f, 1.0f);
            c[ImGuiCol_Button] = ImVec4(0.87f, 0.86f, 0.94f, 1.0f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.71f, 0.95f, 1.0f);
            c[ImGuiCol_ButtonActive] = ImVec4(0.65f, 0.58f, 0.95f, 1.0f);
            c[ImGuiCol_Header] = ImVec4(0.85f, 0.83f, 0.95f, 1.0f);
            c[ImGuiCol_HeaderHovered] = ImVec4(0.75f, 0.71f, 0.95f, 1.0f);
            c[ImGuiCol_HeaderActive] = ImVec4(0.65f, 0.58f, 0.95f, 1.0f);
            c[ImGuiCol_CheckMark] = g_accent;
            c[ImGuiCol_NavHighlight] = g_accent;
        }

        for (auto& tab : m_tabs) {
            tab.editor.SetPalette(m_theme == Theme::Dark
                ? TextEditor::GetDarkPalette()
                : TextEditor::GetLightPalette());
        }
    }

    // ─── Fonts ──────────────────────────────────────────────
    void App::applyFonts() {
        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 1;
        cfg.PixelSnapH = false;
        cfg.RasterizerMultiply = 1.2f;

        ImFontGlyphRangesBuilder b;
        b.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        b.AddRanges(io.Fonts->GetGlyphRangesDefault());
        ImVector<ImWchar> ranges;
        b.BuildRanges(&ranges);

        const char* monoPaths[] = {
            "assets/fonts/JetBrainsMono-Regular.ttf",
            "assets/fonts/JetBrainsMonoNL-Regular.ttf",
            "D:/Minecraft/mayami/assets/fonts/JetBrainsMono-Regular.ttf",
            "C:/Windows/Fonts/CascadiaCode.ttf",
            "C:/Windows/Fonts/CascadiaMono.ttf",
            "C:/Windows/Fonts/consola.ttf",
            "C:/Windows/Fonts/consolab.ttf",
        };

        const float fontSize = 17.0f;

        for (const char* p : monoPaths) {
            FILE* f = std::fopen(p, "rb");
            if (!f) continue;
            std::fclose(f);
            m_fontText = io.Fonts->AddFontFromFileTTF(p, fontSize, &cfg, ranges.Data);
            if (m_fontText) break;
        }

        if (!m_fontText) {
            ImFontConfig def;
            def.SizePixels = fontSize;
            def.OversampleH = 3;
            def.PixelSnapH = false;
            m_fontText = io.Fonts->AddFontDefault(&def);
        }

        m_fontMono = m_fontText;

        static const ImWchar iconRanges[] = {
            0x2014, 0x2014, 0x25A0, 0x25A2, 0x263C, 0x263C,
            0x263E, 0x263E, 0x2715, 0x2716, 0x2795, 0x2795,
            0x2796, 0x2796, 0,
        };
        ImFontConfig iconCfg;
        iconCfg.OversampleH = 3;
        iconCfg.OversampleV = 1;
        iconCfg.PixelSnapH = false;
        iconCfg.RasterizerMultiply = 1.2f;

        const char* iconPaths[] = {
            "C:/Windows/Fonts/seguisym.ttf",
            "C:/Windows/Fonts/seguiemj.ttf",
        };
        for (const char* p : iconPaths) {
            FILE* f = std::fopen(p, "rb");
            if (!f) continue;
            std::fclose(f);
            m_fontIcon = io.Fonts->AddFontFromFileTTF(p, 20.0f, &iconCfg, iconRanges);
            if (m_fontIcon) break;
        }

        io.FontDefault = m_fontText;
    }

    // ─── Actions ────────────────────────────────────────────
    void App::newFile() {
        EditorTab tab;
        tab.title = "untitled.mayami";
        tab.path = "";
        tab.editor.SetText("fn main() {\n    \n}\n");
        tab.savedContent = tab.editor.GetText();
        tab.editor.SetPalette(m_theme == Theme::Dark
            ? TextEditor::GetDarkPalette()
            : TextEditor::GetLightPalette());
        tab.editor.SetLanguage(TextEditor::Language::Cpp());
        m_tabs.push_back(std::move(tab));
        m_activeTab = (int)m_tabs.size() - 1;

        clearOutputLog();
        log("New file created");
    }

    void App::openFile() {
        char cmd[512];
        std::snprintf(cmd, sizeof(cmd),
            "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
            "$f = New-Object System.Windows.Forms.OpenFileDialog; "
            "$f.Filter = 'Mayami files (*.mayami)|*.mayami|All files (*.*)|*.*'; "
            "if ($f.ShowDialog() -eq 'OK') { Write-Output $f.FileName }\"");

        std::string path = trimEol(runAndCapture(cmd));
        if (!path.empty()) openFileInEditor(path);
    }

    void App::openProject() {
        char cmd[512];
        std::snprintf(cmd, sizeof(cmd),
            "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
            "$f = New-Object System.Windows.Forms.FolderBrowserDialog; "
            "if ($f.ShowDialog() -eq 'OK') { Write-Output $f.SelectedPath }\"");

        std::string path = trimEol(runAndCapture(cmd));
        if (!path.empty()) {
            m_projectRoot = path;
            m_fileTreeRoot = FileSystem::scanDirectory(m_projectRoot);
            m_selectedPath.clear();
            logSuccess("Opened project: " + path);
        }
    }

    void App::saveFile() {
        if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return;
        auto& tab = m_tabs[m_activeTab];

        std::string path = tab.path;

        if (path.empty()) {
            char cmd[512];
            std::snprintf(cmd, sizeof(cmd),
                "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
                "$f = New-Object System.Windows.Forms.SaveFileDialog; "
                "$f.Filter = 'Mayami files (*.mayami)|*.mayami|All files (*.*)|*.*'; "
                "if ($f.ShowDialog() -eq 'OK') { Write-Output $f.FileName }\"");

            path = trimEol(runAndCapture(cmd));

            if (path.empty()) {
                log("Save cancelled");
                return;
            }
        }

        std::string current = tab.editor.GetText();
        if (FileSystem::writeFile(path, current)) {
            tab.path = path;
            tab.title = FileSystem::getFileName(path);
            tab.savedContent = current;
            tab.modified = false;
            logSuccess("Saved: " + path);
        }
        else logError("Failed to save: " + path);
    }

    // ─── Quick Run (в памяти) — Ctrl+F5 ─────────────────────
    void App::runCode() {
        if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) {
            logError("No active tab");
            return;
        }

        auto& tab = m_tabs[m_activeTab];

        clearBuildLog();
        clearOutputLog();

        log("================================");
        log("Quick Run: " + tab.title);
        log("================================");

        std::string source = tab.editor.GetText();

        log("");
        log("[1/4] Lexer...");

        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        log("  -> " + std::to_string(tokens.size()) + " tokens");

        if (lexer.hasErrors()) {
            logError("  ERROR Lexer errors:");
            for (const auto& err : lexer.errors())
                logError("    " + err);
            log("");
            logError("Compilation aborted.");
            logOutput("Compilation failed (lexer errors)");
            m_showTerminalWindow = true;
            return;
        }

        log("");
        log("[2/4] Parser...");

        Parser parser(tokens);
        std::unique_ptr<Program> program = parser.parse();

        if (!program || parser.hasErrors()) {
            logError("  ERROR Parser errors:");
            for (const auto& err : parser.errors())
                logError("    " + err);
            log("");
            logError("Compilation aborted.");
            logOutput("Compilation failed (parser errors)");
            m_showTerminalWindow = true;
            return;
        }

        logSuccess("  OK Parse successful!");

        log("");
        log("[3/4] BytecodeGen...");

        BytecodeGen gen;
        std::vector<uint8_t> bytecode = gen.generate(program.get());

        if (gen.hasErrors()) {
            logError("  ERROR BytecodeGen errors:");
            for (const auto& err : gen.errors())
                logError("    " + err);
            log("");
            logError("Compilation aborted.");
            logOutput("Compilation failed (bytecode errors)");
            m_showTerminalWindow = true;
            return;
        }

        logSuccess("  OK Bytecode: " + std::to_string(bytecode.size()) + " bytes");

        log("");
        log("[4/4] VM...");

        VM vm;
        vm.setOutputCallback([this](const std::string& s) {
            logOutput(s);
            });

        // ★ Отладка
        vm.setDebug(m_debugMode);
        vm.setDebugCallback([this](const std::string& s) {
            log(s);
            });

        if (!vm.load(bytecode)) {
            logError("  ERROR VM load errors:");
            for (const auto& err : vm.errors())
                logError("    " + err);
            logOutput("=== VM failed to load ===");
            m_showTerminalWindow = true;
            m_terminalTab = 1;
            return;
        }

        logOutput("=== Program Output ===");
        logOutput("");

        bool ok = vm.run();

        logOutput("");

        if (ok) {
            logSuccess("  OK Program executed successfully!");
            logOutput("=== Program finished ===");
        }
        else {
            logError("  ERROR Runtime errors:");
            for (const auto& err : vm.errors())
                logError("    " + err);
            logOutput("=== Program failed ===");
        }

        m_showTerminalWindow = true;
        m_terminalTab = 1;
    }

    // ─── Build EXE (F5 / Ctrl+B) ───────────────────────────
    void App::buildExe() {
        if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) {
            logError("No active tab");
            m_showTerminalWindow = true;
            m_terminalTab = 0;
            return;
        }

        auto& tab = m_tabs[m_activeTab];

        if (tab.path.empty()) {
            clearBuildLog();
            logError("Сначала сохрани файл (Ctrl+S), потом F5");
            m_showTerminalWindow = true;
            m_terminalTab = 0;
            return;
        }

        if (tab.modified) {
            std::string current = tab.editor.GetText();
            if (FileSystem::writeFile(tab.path, current)) {
                tab.savedContent = current;
                tab.modified = false;
                log("Auto-saved: " + tab.path);
            }
        }

        clearBuildLog();
        clearOutputLog();

        log("================================");
        log("Building EXE: " + tab.title);
        log("================================");

        std::string binDir = getBinDir();
        std::string mayamic = binDir + "\\mayamic.exe";
        std::string buildDir = binDir + "\\build";

        if (fileSize(mayamic) < 0) {
            logError("Не найден: " + mayamic);
            m_showTerminalWindow = true;
            m_terminalTab = 0;
            return;
        }

        CreateDirectoryA(buildDir.c_str(), nullptr);

        std::string baseName = tab.title;
        size_t dot = baseName.find_last_of('.');
        if (dot != std::string::npos) baseName = baseName.substr(0, dot);

        std::string outExe = buildDir + "\\" + baseName + ".exe";

        std::remove(outExe.c_str());

        std::string cmd = "\"" + mayamic + "\" \"" + tab.path + "\" -o \"" + outExe + "\" 2>&1";

        log("Команда: " + cmd);
        log("");

        std::string output = runAndCapture(cmd);

        if (!output.empty()) {
            log("--- Вывод компилятора ---");
            std::istringstream ss(output);
            std::string line;
            while (std::getline(ss, line)) {
                line = trimEol(line);
                if (!line.empty()) log(line);
            }
            log("--- Конец вывода ---");
            log("");
        }

        long sz = fileSize(outExe);
        if (sz > 100 * 1024) {
            m_lastBuiltExe = outExe;
            logSuccess("Готово! Размер: " + std::to_string(sz) + " байт");
            logSuccess("Путь: " + outExe);
        }
        else if (sz >= 0) {
            logError("Файл слишком маленький: " + std::to_string(sz) + " байт");
        }
        else {
            logError("Файл не создан: " + outExe);
        }

        m_showTerminalWindow = true;
        m_terminalTab = 0;
    }

    // ─── Run EXE (Ctrl+R) ──────────────────────────────────
    void App::runExe() {
        if (m_lastBuiltExe.empty()) {
            logError("Сначала собери .exe (F5 или Ctrl+B)");
            m_showTerminalWindow = true;
            m_terminalTab = 0;
            return;
        }

        if (fileSize(m_lastBuiltExe) < 0) {
            logError("Файл не найден: " + m_lastBuiltExe);
            logError("Пересобери (F5 или Ctrl+B)");
            m_showTerminalWindow = true;
            m_terminalTab = 0;
            return;
        }

        clearOutputLog();

        logOutput("=== Launching: " + m_lastBuiltExe + " ===");
        logOutput("");

        std::string cmd = "start \"\" \"" + m_lastBuiltExe + "\"";
        std::system(cmd.c_str());

        logOutput("(запущено в отдельном окне)");
        logOutput("");
        logOutput("=== Done ===");

        m_showTerminalWindow = true;
        m_terminalTab = 1;
    }

    // ─── Логи ───────────────────────────────────────────────
    void App::log(const std::string& msg) { m_buildLog.push_back("[INFO] " + msg); }
    void App::logError(const std::string& msg) { m_buildLog.push_back("[ERROR] " + msg); }
    void App::logSuccess(const std::string& msg) { m_buildLog.push_back("[OK] " + msg); }
    void App::logWarn(const std::string& msg) { m_buildLog.push_back("[WARN] " + msg); }
    void App::clearBuildLog() { m_buildLog.clear(); }

    void App::logOutput(const std::string& msg) { m_outputLog.push_back(msg); }
    void App::logOutputError(const std::string& msg) { m_outputLog.push_back("[stderr] " + msg); }
    void App::clearOutputLog() { m_outputLog.clear(); }

    // ─── Shutdown ───────────────────────────────────────────
    void App::shutdown() {
        if (m_shutdownCalled) return;
        m_shutdownCalled = true;

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();

        if (g_hwnd) {
            ::DestroyWindow(g_hwnd);
            ::UnregisterClassW(L"mayami_ide", GetModuleHandle(nullptr));
            g_hwnd = nullptr;
        }
    }

} // namespace mayami