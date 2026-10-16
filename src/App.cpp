// src/App.cpp
#include "App.h"

#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "parser/AST.h"
#include "codegen/BytecodeGen.h"
#include "vm/VM.h"

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <memory>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

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

    // ★ Дизайн-токены
    static ImVec4 g_accent = ImVec4(0.42f, 0.55f, 1.00f, 1.00f);
    static ImVec4 g_accent2 = ImVec4(0.75f, 0.42f, 1.00f, 1.00f);
    static ImVec4 g_accentPink = ImVec4(0.90f, 0.35f, 0.75f, 1.00f);
    static ImVec4 g_accentCyan = ImVec4(0.35f, 0.85f, 1.00f, 1.00f);
    static ImVec4 g_textMain = ImVec4(0.93f, 0.93f, 0.97f, 1.00f);
    static ImVec4 g_textDim = ImVec4(0.55f, 0.55f, 0.65f, 1.00f);
    static ImVec4 g_danger = ImVec4(0.90f, 0.35f, 0.40f, 1.00f);
    static ImVec4 g_warning = ImVec4(1.00f, 0.78f, 0.40f, 1.00f);
    static ImVec4 g_success = ImVec4(0.40f, 0.90f, 0.55f, 1.00f);

    static ImVec4 g_bgTop = ImVec4(0.055f, 0.055f, 0.085f, 1.0f);
    static ImVec4 g_bgBottom = ImVec4(0.025f, 0.025f, 0.045f, 1.0f);
    static ImVec4 g_panelBg = ImVec4(0.075f, 0.075f, 0.115f, 1.0f);
    static ImVec4 g_titleBg = ImVec4(0.065f, 0.065f, 0.105f, 1.0f);
    static ImVec4 g_menuBg = ImVec4(0.095f, 0.095f, 0.145f, 1.0f);
    static ImVec4 g_editorBg = ImVec4(0.05f, 0.05f, 0.075f, 1.0f);
    static ImVec4 g_termBg = ImVec4(0.045f, 0.045f, 0.075f, 0.985f);
    static ImVec4 g_statusBg = ImVec4(0.075f, 0.075f, 0.115f, 1.0f);
    static ImVec4 g_borderCol = ImVec4(0.22f, 0.22f, 0.32f, 0.60f);

    // ============================================================
    // ★ Палитра TextEditor (Goossens API)
    // ============================================================
    static TextEditor::Palette myMayamiPalette(Theme dark) {
        TextEditor::Palette p = dark == Theme::Dark
            ? TextEditor::GetDarkPalette()
            : TextEditor::GetLightPalette();

        auto set = [&p](TextEditor::Color c, ImU32 v) {
            p[static_cast<size_t>(c)] = v;
            };

        if (dark == Theme::Dark) {
            set(TextEditor::Color::text, IM_COL32(0xE8, 0xEC, 0xF4, 0xFF));
            set(TextEditor::Color::keyword, IM_COL32(0x7C, 0xA8, 0xFF, 0xFF));
            set(TextEditor::Color::declaration, IM_COL32(0xF0, 0xC0, 0x80, 0xFF));
            set(TextEditor::Color::number, IM_COL32(0xC8, 0xE0, 0xB0, 0xFF));
            set(TextEditor::Color::string, IM_COL32(0xE0, 0xA0, 0x80, 0xFF));
            set(TextEditor::Color::comment, IM_COL32(0x70, 0x90, 0x60, 0xFF));
            set(TextEditor::Color::identifier, IM_COL32(0xA8, 0xE0, 0xFF, 0xFF));
            set(TextEditor::Color::knownIdentifier, IM_COL32(0xA8, 0xE0, 0xFF, 0xFF));
            set(TextEditor::Color::punctuation, IM_COL32(0xB0, 0xB8, 0xC8, 0xFF));
            set(TextEditor::Color::preprocessor, IM_COL32(0xD8, 0x90, 0xD0, 0xFF));

            set(TextEditor::Color::currentLineHighlight, IM_COL32(70, 90, 150, 45));
            set(TextEditor::Color::currentLineHighlightBorder, IM_COL32(0, 0, 0, 0)); 

            set(TextEditor::Color::lineNumber, IM_COL32(85, 95, 115, 255));
            set(TextEditor::Color::currentLineNumber, IM_COL32(160, 180, 230, 255));
            set(TextEditor::Color::background, IM_COL32(0x0C, 0x0C, 0x14, 0xFF));
            set(TextEditor::Color::whitespace, IM_COL32(60, 65, 80, 255));

            set(TextEditor::Color::selection, IM_COL32(70, 100, 180, 110));
            set(TextEditor::Color::cursor, IM_COL32(0xE8, 0xEC, 0xF4, 0xFF));

            set(TextEditor::Color::matchingBracketBackground, IM_COL32(80, 110, 180, 80));
            set(TextEditor::Color::matchingBracketActive, IM_COL32(120, 170, 255, 200));
            set(TextEditor::Color::matchingBracketLevel1, IM_COL32(0xF0, 0xD0, 0x80, 0xFF));
            set(TextEditor::Color::matchingBracketLevel2, IM_COL32(0xF0, 0xA0, 0xD0, 0xFF));
            set(TextEditor::Color::matchingBracketLevel3, IM_COL32(0x90, 0xD0, 0xF0, 0xFF));
            set(TextEditor::Color::matchingBracketError, IM_COL32(0xF0, 0x60, 0x60, 0xFF));
        }
        else {
            set(TextEditor::Color::currentLineHighlight, IM_COL32(210, 218, 240, 130));
            set(TextEditor::Color::currentLineHighlightBorder, IM_COL32(70, 100, 180, 200));
        }

        return p;
    }

    // ============================================================
    // Хелперы
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
            nullptr, buf.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

        if (!ok) {
            DWORD err = GetLastError();
            CloseHandle(hRead);
            CloseHandle(hWrite);
            return "[ERROR] CreateProcess failed (code " + std::to_string(err) + ")\n";
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

    static std::string getModuleDir() {
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

    static long fileSize(const std::string& path) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return -1;
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fclose(f);
        return sz;
    }

    static std::string getBinDir() {
        std::string base = getModuleDir();
        std::string candidates[] = {
            base + "\\mayamic.exe",
            base + "\\bin\\mayamic.exe",
            base + "\\..\\bin\\mayamic.exe",
        };
        for (const auto& c : candidates) {
            if (fileSize(c) >= 0) {
                size_t pos = c.find_last_of("\\/");
                return (pos == std::string::npos) ? base : c.substr(0, pos);
            }
        }
        return base;
    }

    static std::string normalizeNewlines(const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (char c : s) if (c != '\r') r.push_back(c);
        return r;
    }

    // ★ Карточка с многослойной тенью
    static void drawCard(ImVec2 p, ImVec2 s, float radius,
        ImVec4 bgCol, ImVec4 borderCol, float borderThick = 1.0f)
    {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // Многослойная тень
        for (int i = 10; i > 0; --i) {
            float a = 0.020f * (float)i / 10.0f;
            dl->AddRectFilled(
                ImVec2(p.x - i, p.y - i + 4),
                ImVec2(p.x + s.x + i, p.y + s.y + i + 4),
                IM_COL32(0, 0, 0, (int)(a * 255)),
                radius + i);
        }

        dl->AddRectFilled(
            p, ImVec2(p.x + s.x, p.y + s.y),
            ImGui::ColorConvertFloat4ToU32(bgCol), radius);

        dl->AddRect(
            p, ImVec2(p.x + s.x, p.y + s.y),
            ImGui::ColorConvertFloat4ToU32(borderCol),
            radius, 0, borderThick);
    }

    // ★ Универсальная кастомная кнопка (не ImGui::Button)
    static bool RoundedIconButton(
        const char* id,
        const char* label,
        ImVec2 size,
        ImVec4 bgCol,
        ImVec4 bgHover,
        ImVec4 bgActive,
        float radius,
        ImFont* iconFont = nullptr,
        bool glow = false)
    {
        ImGui::PushID(id);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##ib", size);

        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        ImVec4 col = bgCol;
        if (active) col = bgActive;
        else if (hovered) col = bgHover;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Мягкое свечение при hover
        if (hovered && glow) {
            for (int i = 4; i > 0; --i) {
                dl->AddRectFilled(
                    ImVec2(pos.x - i, pos.y - i),
                    ImVec2(pos.x + size.x + i, pos.y + size.y + i),
                    IM_COL32((int)(col.x * 255), (int)(col.y * 255),
                        (int)(col.z * 255), (int)(18 / i)),
                    radius + i);
            }
        }

        dl->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(col), radius);

        // Тонкая обводка
        dl->AddRect(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            IM_COL32(255, 255, 255, hovered ? 30 : 15),
            radius, 0, 1.0f);

        // Центрированный текст
        if (iconFont) ImGui::PushFont(iconFont);
        ImVec2 ts = ImGui::CalcTextSize(label);
        ImVec2 textPos(
            pos.x + (size.x - ts.x) * 0.5f,
            pos.y + (size.y - ts.y) * 0.5f);
        dl->AddText(textPos, ImGui::ColorConvertFloat4ToU32(g_textMain), label);
        if (iconFont) ImGui::PopFont();

        ImGui::PopID();
        return clicked;
    }

    // ★ Pill-индикатор (для status bar)
    static void drawPill(ImVec2 pos, const char* text, ImVec4 color) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 ts = ImGui::CalcTextSize(text);
        float padX = 8.0f, padY = 2.0f;
        ImVec2 size(ts.x + padX * 2, ts.y + padY * 2);

        ImVec4 bg = ImVec4(color.x, color.y, color.z, 0.15f);
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(bg), 10.0f);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.35f)),
            10.0f, 0, 1.0f);

        dl->AddText(
            ImVec2(pos.x + padX, pos.y + padY),
            ImGui::ColorConvertFloat4ToU32(color), text);
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
        const int ww = 1360, wh = 800;
        int wx = (sw - ww) / 2, wy = (sh - wh) / 2;

        HWND hwnd = ::CreateWindowExW(
            0, wc.lpszClassName, L"Mayami IDE",
            WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
            wx, wy, ww, wh, nullptr, nullptr, wc.hInstance, nullptr);

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

        applyFonts();
        applyThemeById(ThemeId::Dark);

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
        tab.editor.SetPalette(myMayamiPalette(Theme::Dark));
        tab.editor.SetLanguage(TextEditor::Language::Cpp());
        m_tabs.push_back(std::move(tab));
        m_activeTab = 0;

        m_projectRoot.clear();
        {
            std::string base = getModuleDir();
            std::string guesses[] = {
                base + "\\..", base, base + "\\..\\..", "D:/Minecraft/mayami",
            };
            for (const auto& g : guesses) {
                if (!FileSystem::exists(g) || !FileSystem::isDirectory(g)) continue;
                if (FileSystem::exists(g + "/src/App.cpp") || FileSystem::exists(g + "/src")) {
                    m_projectRoot = g;
                    break;
                }
            }
        }

        if (!m_projectRoot.empty() && FileSystem::exists(m_projectRoot)) {
            m_fileTreeRoot = FileSystem::scanDirectory(m_projectRoot);
            log("Project loaded: " + m_projectRoot);
        }
        else logWarn("Project not auto-detected.");

        log("Mayami IDE v0.9.5 initialized");
        log("F5 = Build+Run | Ctrl+B = Build | Ctrl+R = Run | Ctrl+F5 = Quick Run");
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

            // Фон окна
            {
                ImDrawList* dl = ImGui::GetBackgroundDrawList();
                ImVec2 sz = ImGui::GetIO().DisplaySize;
                dl->AddRectFilledMultiColor(
                    ImVec2(0, 0), ImVec2(sz.x, sz.y),
                    ImGui::ColorConvertFloat4ToU32(g_bgTop),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(g_bgTop.x + 0.02f, g_bgTop.y, g_bgTop.z + 0.03f, 1.0f)),
                    ImGui::ColorConvertFloat4ToU32(g_bgBottom),
                    ImGui::ColorConvertFloat4ToU32(g_bgBottom));
            }

            drawUI();
            ImGui::Render();

            ImVec4 bg = g_bgBottom;
            const float clear[4] = { bg.x, bg.y, bg.z, 1.0f };

            if (g_mainRenderTargetView && g_pd3dDeviceContext && g_pSwapChain) {
                g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
                g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                g_pSwapChain->Present(1, 0);
            }
        }
    }

    // ─── drawUI ─────────────────────────────────────────────
    void App::drawUI() {
        ImGuiIO& io = ImGui::GetIO();

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
            m_showTerminalWindow = !m_showTerminalWindow;

        if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
            if (io.KeyCtrl && io.KeyShift) {
                m_debugMode = !m_debugMode;
                log(m_debugMode ? "=== DEBUG MODE ON ===" : "=== DEBUG MODE OFF ===");
            }
            else if (io.KeyCtrl) runCode();
            else {
                buildExe();
                if (!m_lastBuiltExe.empty() && fileSize(m_lastBuiltExe) > 0) runExe();
            }
        }

        if (io.KeyCtrl && !io.KeyShift) {
            if (ImGui::IsKeyPressed(ImGuiKey_S, false)) saveFile();
            else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) openFile();
            else if (ImGui::IsKeyPressed(ImGuiKey_N, false)) newFile();
            else if (ImGui::IsKeyPressed(ImGuiKey_B, false)) buildExe();
            else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) runExe();
            else if (ImGui::IsKeyPressed(ImGuiKey_W, false)) closeTab(m_activeTab);
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

        ImGui::Begin("##TitleBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();

        // Градиентный фон
        dl->AddRectFilledMultiColor(
            ImVec2(wp.x, wp.y), ImVec2(wp.x + ws.x, wp.y + ws.y),
            ImGui::ColorConvertFloat4ToU32(ImVec4(g_titleBg.x + 0.02f, g_titleBg.y + 0.02f, g_titleBg.z + 0.04f, 1.0f)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(g_titleBg.x + 0.05f, g_titleBg.y + 0.02f, g_titleBg.z + 0.06f, 1.0f)),
            ImGui::ColorConvertFloat4ToU32(g_titleBg),
            ImGui::ColorConvertFloat4ToU32(g_titleBg));

        // Цветная акцентная линия снизу
        dl->AddRectFilledMultiColor(
            ImVec2(wp.x, wp.y + ws.y - 2), ImVec2(wp.x + ws.x, wp.y + ws.y),
            ImGui::ColorConvertFloat4ToU32(g_accent),
            ImGui::ColorConvertFloat4ToU32(g_accent2),
            ImGui::ColorConvertFloat4ToU32(g_accentPink),
            ImGui::ColorConvertFloat4ToU32(g_accentPink));

        // Логотип с двойным кругом
        {
            const float logoSize = 24.0f;
            float logoX = wp.x + 14;
            float logoY = wp.y + (ws.y - logoSize) * 0.5f;
            ImVec2 center(logoX + logoSize * 0.5f, logoY + logoSize * 0.5f);

            // Внешнее свечение
            for (int i = 3; i > 0; --i) {
                dl->AddCircleFilled(
                    center, logoSize * 0.5f + i,
                    IM_COL32((int)(g_accent.x * 255), (int)(g_accent.y * 255),
                        (int)(g_accent.z * 255), 20 / i), 32);
            }

            // Градиентный круг
            dl->AddCircleFilled(center, logoSize * 0.5f,
                ImGui::ColorConvertFloat4ToU32(g_accent), 32);
            dl->AddCircleFilled(
                ImVec2(center.x - 2, center.y - 2), logoSize * 0.35f,
                ImGui::ColorConvertFloat4ToU32(ImVec4(
                    g_accent.x * 0.7f + 0.3f, g_accent.y * 0.7f + 0.3f, 1.0f, 1.0f)), 32);

            // Буква M
            const char* letter = "M";
            ImVec2 ts = ImGui::CalcTextSize(letter);
            dl->AddText(
                ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f),
                IM_COL32(255, 255, 255, 255), letter);
        }

        // Название
        {
            const char* title1 = "Mayami";
            const char* title2 = " IDE";
            ImVec2 ts1 = ImGui::CalcTextSize(title1);
            ImVec2 ts2 = ImGui::CalcTextSize(title2);
            ImVec2 ts(ts1.x + ts2.x, ts1.y);
            float startX = wp.x + (ws.x - ts.x) * 0.5f;
            float startY = wp.y + (ws.y - ts.y) * 0.5f;

            dl->AddText(ImVec2(startX, startY), ImGui::ColorConvertFloat4ToU32(g_textMain), title1);
            dl->AddText(ImVec2(startX + ts1.x, startY), ImGui::ColorConvertFloat4ToU32(g_accent), title2);
        }

        // Кнопки управления
        const float btnSize = 34.0f;
        const float spacing = 6.0f;
        float totalW = btnSize * 3 + spacing * 2;
        float startX = io.DisplaySize.x - totalW - 14.0f;

        ImGui::SetCursorPosX(startX);
        ImGui::SetCursorPosY((barHeight - btnSize) * 0.5f);

        // ★ Кнопка Свернуть
        if (RoundedIconButton("min", "\xE2\x80\x94",
            ImVec2(btnSize, btnSize),
            ImVec4(1, 1, 1, 0.05f),
            ImVec4(0.85f, 0.75f, 0.30f, 0.30f),
            ImVec4(0.85f, 0.75f, 0.30f, 0.55f),
            10.0f, m_fontIcon))
        {
            ::ShowWindow(g_hwnd, SW_MINIMIZE);
        }

        ImGui::SameLine(0, spacing);

        // ★ Кнопка Развернуть
        if (RoundedIconButton("max", "\xE2\x96\xA2",
            ImVec2(btnSize, btnSize),
            ImVec4(1, 1, 1, 0.05f),
            ImVec4(0.30f, 0.60f, 0.85f, 0.30f),
            ImVec4(0.30f, 0.60f, 0.85f, 0.55f),
            10.0f, m_fontIcon))
        {
            if (::IsZoomed(g_hwnd)) ::ShowWindow(g_hwnd, SW_RESTORE);
            else ::ShowWindow(g_hwnd, SW_MAXIMIZE);
        }

        ImGui::SameLine(0, spacing);

        // ★ Кнопка Закрыть
        if (RoundedIconButton("close", "\xE2\x9C\x95",
            ImVec2(btnSize, btnSize),
            ImVec4(1, 1, 1, 0.05f),
            ImVec4(0.90f, 0.25f, 0.30f, 0.45f),
            ImVec4(0.90f, 0.25f, 0.30f, 0.75f),
            10.0f, m_fontIcon, true))
        {
            ::PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
        }

        // Drag-зона
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, g_menuBg);

        ImGui::Begin("##MenuBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();

        dl->AddRectFilled(
            ImVec2(wp.x, wp.y + ws.y - 1), ImVec2(wp.x + ws.x, wp.y + ws.y),
            IM_COL32(70, 70, 110, 200));

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
                if (ImGui::BeginMenu("Theme")) {
                    if (ImGui::MenuItem("Dark", nullptr, m_themeId == ThemeId::Dark))
                        applyThemeById(ThemeId::Dark);
                    if (ImGui::MenuItem("Midnight", nullptr, m_themeId == ThemeId::Midnight))
                        applyThemeById(ThemeId::Midnight);
                    if (ImGui::MenuItem("Nord", nullptr, m_themeId == ThemeId::Nord))
                        applyThemeById(ThemeId::Nord);
                    if (ImGui::MenuItem("Dracula", nullptr, m_themeId == ThemeId::Dracula))
                        applyThemeById(ThemeId::Dracula);
                    if (ImGui::MenuItem("Light", nullptr, m_themeId == ThemeId::Light))
                        applyThemeById(ThemeId::Light);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Run")) {
                if (ImGui::MenuItem("Build + Run EXE", "F5")) {
                    buildExe();
                    if (!m_lastBuiltExe.empty() && fileSize(m_lastBuiltExe) > 0)
                        runExe();
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
                if (ImGui::MenuItem("About")) m_showAbout = true;
                ImGui::EndMenu();
            }

            // ★ Кнопки справа — градиентные
            {
                const float btnW = 78.0f;
                const float btnH = 22.0f;
                const float spacing = 8.0f;
                float totalW = btnW * 3 + spacing * 2;
                float startX = io.DisplaySize.x - totalW - 20.0f;

                ImGui::SameLine(startX);
                ImGui::SetCursorPosY(4.0f);

                if (RoundedIconButton("m_build", "Build",
                    ImVec2(btnW, btnH),
                    ImVec4(0.30f, 0.38f, 0.60f, 1.0f),
                    ImVec4(0.42f, 0.50f, 0.72f, 1.0f),
                    ImVec4(0.25f, 0.33f, 0.55f, 1.0f),
                    11.0f, nullptr, true))
                    buildExe();

                ImGui::SameLine(0, spacing);
                if (RoundedIconButton("m_run", "Run",
                    ImVec2(btnW, btnH),
                    ImVec4(0.22f, 0.50f, 0.32f, 1.0f),
                    ImVec4(0.32f, 0.62f, 0.42f, 1.0f),
                    ImVec4(0.18f, 0.45f, 0.28f, 1.0f),
                    11.0f, nullptr, true))
                    runExe();

                ImGui::SameLine(0, spacing);
                if (RoundedIconButton("m_debug",
                    m_debugMode ? "DBG ON" : "DBG",
                    ImVec2(btnW, btnH),
                    m_debugMode ? ImVec4(0.65f, 0.48f, 0.15f, 1.0f)
                    : ImVec4(0.30f, 0.30f, 0.38f, 1.0f),
                    m_debugMode ? ImVec4(0.78f, 0.60f, 0.25f, 1.0f)
                    : ImVec4(0.42f, 0.42f, 0.50f, 1.0f),
                    m_debugMode ? ImVec4(0.58f, 0.42f, 0.10f, 1.0f)
                    : ImVec4(0.25f, 0.25f, 0.32f, 1.0f),
                    11.0f, nullptr, m_debugMode))
                {
                    m_debugMode = !m_debugMode;
                    log(m_debugMode ? "=== DEBUG MODE ON ===" : "=== DEBUG MODE OFF ===");
                }
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

        bool isSelected = (entry.path == selectedPath);

        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

        ImVec2 pos = ImGui::GetCursorScreenPos();
        float rowHeight = ImGui::GetTextLineHeight() + 6.0f;
        float width = ImGui::GetContentRegionAvail().x - 4.0f;

        if (isSelected) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(
                ImVec2(pos.x - 4, pos.y - 2),
                ImVec2(pos.x + width, pos.y + rowHeight - 4),
                IM_COL32(90, 110, 220, 130),
                8.0f);  // ← скругление
        }

        if (entry.isDirectory) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                ImGuiTreeNodeFlags_SpanAvailWidth;
            std::string label = std::string("[D] ") + entry.name;
            bool open = ImGui::TreeNodeEx(label.c_str(), flags);
            if (ImGui::IsItemClicked()) outSelected = entry.path;
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

            bool isMayami = entry.name.size() > 7 &&
                entry.name.substr(entry.name.size() - 7) == ".mayami";
            std::string icon = isMayami ? "[M] " : "[F] ";
            std::string label = icon + entry.name;

            ImGui::TreeNodeEx(label.c_str(), flags);
            if (ImGui::IsItemClicked()) {
                outSelected = entry.path;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    outOpen = true;
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    void App::drawFileTree() {
        ImGuiIO& io = ImGui::GetIO();

        const float cardRadius = 14.0f;
        float x = 6.0f;
        float y = m_titleBarHeight + m_menuBarHeight + 6.0f;
        float w = m_sidePanelWidth - 12.0f;
        float h = io.DisplaySize.y - y - m_statusBarHeight - 6.0f;

        drawCard(ImVec2(x, y), ImVec2(w, h), cardRadius,
            g_panelBg, ImVec4(g_accent.x, g_accent.y, g_accent.z, 0.30f), 1.0f);

        ImGui::PushClipRect(ImVec2(x, y), ImVec2(x + w, y + h), true);
        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(w, h));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, cardRadius);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

        ImGui::Begin("##FileTree", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextColored(g_accent, "EXPLORER");

        {
            float btnW = 30.0f, btnH = 24.0f, spacing = 4.0f;
            float avail = ImGui::GetContentRegionAvail().x;
            float totalW = btnW * 3 + spacing * 2;
            ImGui::SameLine(ImGui::GetCursorPosX() + avail - totalW);

            if (RoundedIconButton("bt_newfile", "+F", ImVec2(btnW, btnH),
                ImVec4(0.16f, 0.16f, 0.24f, 1.0f),
                ImVec4(g_accent.x * 0.7f, g_accent.y * 0.7f, g_accent.z * 0.9f, 1.0f),
                ImVec4(g_accent.x, g_accent.y, g_accent.z, 1.0f),
                8.0f))
            {
                m_showNewFileDialog = true;
                m_newFileName[0] = 0;
            }

            ImGui::SameLine(0, spacing);
            if (RoundedIconButton("bt_newdir", "+D", ImVec2(btnW, btnH),
                ImVec4(0.16f, 0.16f, 0.24f, 1.0f),
                ImVec4(g_accent.x * 0.7f, g_accent.y * 0.7f, g_accent.z * 0.9f, 1.0f),
                ImVec4(g_accent.x, g_accent.y, g_accent.z, 1.0f),
                8.0f))
            {
                m_showNewFolderDialog = true;
                m_newFolderName[0] = 0;
            }

            ImGui::SameLine(0, spacing);
            if (RoundedIconButton("bt_refresh", "R", ImVec2(btnW, btnH),
                ImVec4(0.16f, 0.16f, 0.24f, 1.0f),
                ImVec4(g_accent.x * 0.7f, g_accent.y * 0.7f, g_accent.z * 0.9f, 1.0f),
                ImVec4(g_accent.x, g_accent.y, g_accent.z, 1.0f),
                8.0f))
            {
                if (!m_projectRoot.empty())
                    m_fileTreeRoot = FileSystem::scanDirectory(m_projectRoot);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (m_projectRoot.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(g_textDim, "No project opened");
            ImGui::Spacing();
            ImGui::Spacing();

            if (RoundedIconButton("bt_openproj", "Open Project...",
                ImVec2(ImGui::GetContentRegionAvail().x, 34),
                ImVec4(g_accent.x * 0.7f, g_accent.y * 0.7f, g_accent.z * 0.9f, 1.0f),
                ImVec4(g_accent.x, g_accent.y, g_accent.z, 1.0f),
                ImVec4(g_accent.x * 0.9f, g_accent.y * 0.9f, g_accent.z * 0.9f, 1.0f),
                10.0f))
                openProject();
        }
        else {
            ImGui::TextColored(g_accent, "> %s",
                FileSystem::getFileName(m_projectRoot).c_str());
            ImGui::Spacing();

            std::string newSelected = m_selectedPath;
            bool openRequested = false;
            DrawFileNode(m_fileTreeRoot, m_selectedPath, newSelected, openRequested);

            if (newSelected != m_selectedPath) m_selectedPath = newSelected;
            if (openRequested && !m_selectedPath.empty() &&
                !FileSystem::isDirectory(m_selectedPath))
                openFileInEditor(m_selectedPath);
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
        ImGui::PopClipRect();
    }

    // ─── Editor ─────────────────────────────────────────────
    void App::drawEditor() {
        ImGuiIO& io = ImGui::GetIO();

        const float cardRadius = 16.0f;
        const float padTop = m_titleBarHeight + m_menuBarHeight + 6.0f;
        const float padLeft = m_sidePanelWidth + 6.0f;
        const float padRight = 6.0f;
        const float padBottom = m_statusBarHeight + 6.0f;

        float x = padLeft, y = padTop;
        float w = io.DisplaySize.x - padLeft - padRight;
        float h = io.DisplaySize.y - padTop - padBottom;

        drawCard(ImVec2(x, y), ImVec2(w, h), cardRadius,
            g_editorBg, ImVec4(g_accent.x, g_accent.y, g_accent.z, 0.40f), 1.0f);

        ImGui::PushClipRect(ImVec2(x, y), ImVec2(x + w, y + h), true);
        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(w, h));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, cardRadius);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

        ImGui::Begin("##EditorArea", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16, 8));

        if (ImGui::BeginTabBar("##EditorTabs",
            ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs |
            ImGuiTabBarFlags_FittingPolicyScroll)) {

            for (size_t i = 0; i < m_tabs.size(); ++i) {
                auto& tab = m_tabs[i];
                bool open = true;

                ImGuiTabItemFlags flags = 0;
                if (tab.modified) flags |= ImGuiTabItemFlags_UnsavedDocument;

                std::string tabLabel = "   " + tab.title + "   ";

                if (ImGui::BeginTabItem(tabLabel.c_str(), &open, flags)) {
                    m_activeTab = (int)i;
                    std::string current = normalizeNewlines(tab.editor.GetText());
                    std::string saved = normalizeNewlines(tab.savedContent);
                    tab.modified = (current != saved);

                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    std::string editorId = "##editor_" + std::to_string(i);
                    tab.editor.Render(editorId.c_str(), avail);
                    ImGui::EndTabItem();
                }

                if (!open) {
                    if (tab.modified) {
                        std::string msg = "Файл \"" + tab.title +
                            "\" изменён. Сохранить перед закрытием?";
                        int res = MessageBoxA(g_hwnd, msg.c_str(), "Mayami IDE",
                            MB_YESNOCANCEL | MB_ICONQUESTION);
                        if (res == IDCANCEL) continue;
                        if (res == IDYES) { m_activeTab = (int)i; saveFile(); }
                    }

                    m_tabs.erase(m_tabs.begin() + i);
                    --i;
                    if (m_activeTab >= (int)m_tabs.size())
                        m_activeTab = (int)m_tabs.size() - 1;
                    if (m_activeTab < 0 && !m_tabs.empty()) m_activeTab = 0;
                }
            }
            ImGui::EndTabBar();
        }

        ImGui::PopStyleVar(2);
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
        ImGui::PopClipRect();
    }

    // ─── Terminal Window ────────────────────────────────────
    void App::drawTerminalWindow() {
        if (!m_showTerminalWindow) return;

        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(
            ImVec2(io.DisplaySize.x - 120, io.DisplaySize.y - 120),
            ImGuiCond_Appearing);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, g_termBg);

        bool open = true;
        if (ImGui::Begin("Terminal##window", &open,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoTitleBar)) {

            // ★ Кастомный title-bar терминала
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImVec2 ws = ImGui::GetWindowSize();
            const float titleH = 38.0f;

            dl->AddRectFilled(
                ImVec2(wp.x, wp.y),
                ImVec2(wp.x + ws.x, wp.y + titleH),
                IM_COL32(30, 30, 50, 255),
                16.0f, ImDrawFlags_RoundCornersTop);

            // Три цветные точки (как в macOS)
            float dotR = 6.0f;
            float dotY = wp.y + titleH * 0.5f;
            dl->AddCircleFilled(ImVec2(wp.x + 18, dotY), dotR, IM_COL32(255, 95, 86, 255));
            dl->AddCircleFilled(ImVec2(wp.x + 38, dotY), dotR, IM_COL32(255, 189, 46, 255));
            dl->AddCircleFilled(ImVec2(wp.x + 58, dotY), dotR, IM_COL32(39, 201, 63, 255));

            // Заголовок
            const char* title = "MAYAMI TERMINAL";
            ImVec2 ts = ImGui::CalcTextSize(title);
            dl->AddText(
                ImVec2(wp.x + (ws.x - ts.x) * 0.5f, wp.y + (titleH - ts.y) * 0.5f),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.7f, 0.85f, 1.0f)), title);

            // Кнопка Close справа
            ImGui::SetCursorPos(ImVec2(ws.x - 84, 8));
            if (RoundedIconButton("term_close", "Close", ImVec2(70, 22),
                ImVec4(0.55f, 0.25f, 0.30f, 1.0f),
                ImVec4(0.70f, 0.32f, 0.38f, 1.0f),
                ImVec4(0.50f, 0.20f, 0.25f, 1.0f),
                8.0f))
            {
                m_showTerminalWindow = false;
                open = false;
            }

            // Отступ после title-bar
            ImGui::SetCursorPos(ImVec2(16, titleH + 12));

            ImGui::BeginGroup();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

            ImGui::TextColored(g_textDim, "Build: %d  |  Output: %d  |  Debug: %s",
                (int)m_buildLog.size(), (int)m_outputLog.size(),
                m_debugMode ? "ON" : "OFF");

            ImGui::Spacing();

            if (ImGui::BeginTabBar("##TermTabs", ImGuiTabBarFlags_None)) {
                std::string buildTitle = "BUILD (" + std::to_string(m_buildLog.size()) + ")";
                if (ImGui::BeginTabItem(buildTitle.c_str())) {
                    m_terminalTab = 0;

                    if (RoundedIconButton("term_clear_build", "Clear", ImVec2(64, 22),
                        ImVec4(0.20f, 0.20f, 0.28f, 1.0f),
                        ImVec4(0.32f, 0.32f, 0.42f, 1.0f),
                        ImVec4(0.15f, 0.15f, 0.22f, 1.0f),
                        6.0f))
                        clearBuildLog();

                    ImGui::SameLine();
                    ImGui::Checkbox("Auto-scroll##build", &m_autoScrollBuild);
                    ImGui::SameLine();
                    ImGui::TextColored(g_textDim, "Лог компилятора");

                    ImGui::Separator();

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, g_bgBottom);
                    ImGui::BeginChild("##build_scroll", ImVec2(0, 0), false,
                        ImGuiWindowFlags_HorizontalScrollbar);
                    if (m_buildLog.empty()) {
                        ImGui::TextColored(g_textDim,
                            "F5 = Build+Run | Ctrl+B = Build | Ctrl+R = Run");
                    }
                    else {
                        for (const auto& line : m_buildLog) {
                            ImVec4 col = g_textMain;
                            if (line.find("[ERROR]") != std::string::npos) col = g_danger;
                            else if (line.find("[WARN]") != std::string::npos) col = g_warning;
                            else if (line.find("[OK]") != std::string::npos) col = g_success;
                            else if (line.find("[VM]") != std::string::npos) col = g_accentCyan;
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

                    if (RoundedIconButton("term_clear_out", "Clear", ImVec2(64, 22),
                        ImVec4(0.20f, 0.20f, 0.28f, 1.0f),
                        ImVec4(0.32f, 0.32f, 0.42f, 1.0f),
                        ImVec4(0.15f, 0.15f, 0.22f, 1.0f),
                        6.0f))
                        clearOutputLog();

                    ImGui::SameLine();
                    ImGui::Checkbox("Auto-scroll##out", &m_autoScrollOutput);
                    ImGui::SameLine();
                    ImGui::TextColored(g_textDim, "Вывод программы");

                    ImGui::Separator();

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, g_bgBottom);
                    ImGui::BeginChild("##output_scroll", ImVec2(0, 0), false,
                        ImGuiWindowFlags_HorizontalScrollbar);
                    if (m_outputLog.empty()) {
                        ImGui::TextColored(g_textDim, "Здесь появится вывод программы");
                    }
                    else {
                        for (const auto& line : m_outputLog) {
                            ImVec4 col = g_textMain;
                            if (line.find("[stderr]") != std::string::npos) col = g_danger;
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

            ImGui::EndGroup();
            ImGui::PopStyleVar();

            // Padding вокруг содержимого
            ImGui::Dummy(ImVec2(0, 8));
        }

        ImGui::End();
        ImGui::PopStyleColor();
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 4));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, g_statusBg);

        ImGui::Begin("##StatusBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();

        // Верхняя акцентная линия
        dl->AddRectFilledMultiColor(
            ImVec2(wp.x, wp.y), ImVec2(wp.x + ws.x, wp.y + 2),
            ImGui::ColorConvertFloat4ToU32(g_accent),
            ImGui::ColorConvertFloat4ToU32(g_accent2),
            ImGui::ColorConvertFloat4ToU32(g_accentPink),
            ImGui::ColorConvertFloat4ToU32(g_accentCyan));

        // Левая группа
        ImGui::TextColored(g_accent, "Mayami");
        ImGui::SameLine(0, 12);
        ImGui::TextColored(g_textDim, "v0.9.5");
        ImGui::SameLine(0, 16);

        ImGui::PushStyleColor(ImGuiCol_Text, g_accentCyan);
        ImGui::Text("Theme: %s", themeName(m_themeId));
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 16);
        if (m_debugMode) {
            ImGui::PushStyleColor(ImGuiCol_Text, g_warning);
            ImGui::Text("DBG ON");
            ImGui::PopStyleColor();
        }
        else {
            ImGui::TextColored(g_textDim, "DBG OFF");
        }

        ImGui::SameLine(0, 16);
        if (!m_lastBuiltExe.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, g_success);
            ImGui::Text("EXE ready");
            ImGui::PopStyleColor();
        }
        else {
            ImGui::TextColored(g_textDim, "EXE not built");
        }

        // Правая группа
        const char* right = "Mayami | UTF-8 | LF";
        float rw = ImGui::CalcTextSize(right).x;
        ImGui::SetCursorPosX(io.DisplaySize.x - rw - 16);
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
            ImGui::TextColored(g_textDim, "v0.9.5");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextWrapped("A modern IDE for the Mayami programming language.");
            ImGui::TextWrapped("Fast as C++, safe as Rust, simple as Python.");
            ImGui::Spacing();
            ImGui::TextColored(g_textDim, "Built with ImGui + DirectX 11");
            ImGui::Spacing();

            if (RoundedIconButton("about_ok", "OK", ImVec2(120, 30),
                ImVec4(g_accent.x * 0.75f, g_accent.y * 0.75f, g_accent.z * 0.9f, 1.0f),
                ImVec4(g_accent.x, g_accent.y, g_accent.z, 1.0f),
                ImVec4(g_accent.x * 0.85f, g_accent.y * 0.85f, g_accent.z, 1.0f),
                10.0f))
            {
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
            ImGui::SetNextItemWidth(320);
            bool enter = ImGui::InputText("##newfile_name", m_newFileName,
                sizeof(m_newFileName), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if (RoundedIconButton("newfile_create", "Create", ImVec2(130, 30),
                ImVec4(0.25f, 0.50f, 0.35f, 1.0f),
                ImVec4(0.35f, 0.60f, 0.45f, 1.0f),
                ImVec4(0.20f, 0.45f, 0.30f, 1.0f),
                10.0f) || (enter && m_newFileName[0] != 0))
            {
                if (m_newFileName[0] != 0 && !m_projectRoot.empty()) {
                    std::string path = FileSystem::joinPath(m_projectRoot, m_newFileName);
                    std::string content = "// " + std::string(m_newFileName) +
                        "\n\nfn main() {\n    \n}\n";
                    if (FileSystem::createFile(path, content)) {
                        logSuccess("Created: " + path);
                        m_fileTreeRoot = FileSystem::scanDirectory(m_projectRoot);
                        openFileInEditor(path);
                    }
                    else logError("Failed to create: " + path);
                    m_showNewFileDialog = false;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (RoundedIconButton("newfile_cancel", "Cancel", ImVec2(130, 30),
                ImVec4(0.30f, 0.30f, 0.38f, 1.0f),
                ImVec4(0.42f, 0.42f, 0.50f, 1.0f),
                ImVec4(0.25f, 0.25f, 0.32f, 1.0f),
                10.0f))
            {
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
            ImGui::SetNextItemWidth(320);
            bool enter = ImGui::InputText("##newfolder_name", m_newFolderName,
                sizeof(m_newFolderName), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if (RoundedIconButton("newfolder_create", "Create", ImVec2(130, 30),
                ImVec4(0.25f, 0.50f, 0.35f, 1.0f),
                ImVec4(0.35f, 0.60f, 0.45f, 1.0f),
                ImVec4(0.20f, 0.45f, 0.30f, 1.0f),
                10.0f) || (enter && m_newFolderName[0] != 0))
            {
                if (m_newFolderName[0] != 0 && !m_projectRoot.empty()) {
                    std::string path = FileSystem::joinPath(m_projectRoot, m_newFolderName);
                    if (FileSystem::createDirectory(path)) {
                        logSuccess("Created folder: " + path);
                        m_fileTreeRoot = FileSystem::scanDirectory(m_projectRoot);
                    }
                    else logError("Failed to create: " + path);
                    m_showNewFolderDialog = false;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (RoundedIconButton("newfolder_cancel", "Cancel", ImVec2(130, 30),
                ImVec4(0.30f, 0.30f, 0.38f, 1.0f),
                ImVec4(0.42f, 0.42f, 0.50f, 1.0f),
                ImVec4(0.25f, 0.25f, 0.32f, 1.0f),
                10.0f))
            {
                m_showNewFolderDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // ─── Theme name ─────────────────────────────────────────
    const char* App::themeName(ThemeId id) const {
        switch (id) {
        case ThemeId::Dark:     return "Dark";
        case ThemeId::Midnight: return "Midnight";
        case ThemeId::Nord:     return "Nord";
        case ThemeId::Dracula:  return "Dracula";
        case ThemeId::Light:    return "Light";
        }
        return "Unknown";
    }

    // ─── Apply theme by id ──────────────────────────────────
    void App::applyThemeById(ThemeId id) {
        m_themeId = id;
        m_theme = (id == ThemeId::Light) ? Theme::Light : Theme::Dark;

        switch (id) {
        case ThemeId::Dark:
            g_accent = ImVec4(0.42f, 0.55f, 1.00f, 1.00f);
            g_accent2 = ImVec4(0.75f, 0.42f, 1.00f, 1.00f);
            g_accentPink = ImVec4(0.90f, 0.35f, 0.75f, 1.00f);
            g_accentCyan = ImVec4(0.35f, 0.85f, 1.00f, 1.00f);
            g_textMain = ImVec4(0.93f, 0.93f, 0.97f, 1.00f);
            g_textDim = ImVec4(0.55f, 0.55f, 0.65f, 1.00f);
            g_bgTop = ImVec4(0.055f, 0.055f, 0.085f, 1.0f);
            g_bgBottom = ImVec4(0.025f, 0.025f, 0.045f, 1.0f);
            g_panelBg = ImVec4(0.075f, 0.075f, 0.115f, 1.0f);
            g_titleBg = ImVec4(0.065f, 0.065f, 0.105f, 1.0f);
            g_menuBg = ImVec4(0.095f, 0.095f, 0.145f, 1.0f);
            g_editorBg = ImVec4(0.05f, 0.05f, 0.075f, 1.0f);
            g_termBg = ImVec4(0.045f, 0.045f, 0.075f, 0.985f);
            g_statusBg = ImVec4(0.075f, 0.075f, 0.115f, 1.0f);
            g_borderCol = ImVec4(0.22f, 0.22f, 0.32f, 0.60f);
            break;

        case ThemeId::Midnight:
            g_accent = ImVec4(0.55f, 0.35f, 1.00f, 1.00f);
            g_accent2 = ImVec4(0.85f, 0.30f, 1.00f, 1.00f);
            g_accentPink = ImVec4(0.75f, 0.30f, 0.95f, 1.00f);
            g_accentCyan = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
            g_textMain = ImVec4(0.90f, 0.88f, 1.00f, 1.00f);
            g_textDim = ImVec4(0.55f, 0.50f, 0.70f, 1.00f);
            g_bgTop = ImVec4(0.045f, 0.030f, 0.095f, 1.0f);
            g_bgBottom = ImVec4(0.020f, 0.015f, 0.060f, 1.0f);
            g_panelBg = ImVec4(0.065f, 0.050f, 0.130f, 1.0f);
            g_titleBg = ImVec4(0.055f, 0.040f, 0.115f, 1.0f);
            g_menuBg = ImVec4(0.075f, 0.055f, 0.145f, 1.0f);
            g_editorBg = ImVec4(0.040f, 0.030f, 0.085f, 1.0f);
            g_termBg = ImVec4(0.035f, 0.025f, 0.080f, 0.985f);
            g_statusBg = ImVec4(0.065f, 0.050f, 0.130f, 1.0f);
            g_borderCol = ImVec4(0.30f, 0.20f, 0.50f, 0.60f);
            break;

        case ThemeId::Nord:
            g_accent = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);
            g_accent2 = ImVec4(0.65f, 0.85f, 0.90f, 1.00f);
            g_accentPink = ImVec4(0.75f, 0.55f, 0.65f, 1.00f);
            g_accentCyan = ImVec4(0.55f, 0.80f, 0.90f, 1.00f);
            g_textMain = ImVec4(0.90f, 0.93f, 0.95f, 1.00f);
            g_textDim = ImVec4(0.55f, 0.60f, 0.68f, 1.00f);
            g_bgTop = ImVec4(0.160f, 0.180f, 0.220f, 1.0f);
            g_bgBottom = ImVec4(0.115f, 0.135f, 0.170f, 1.0f);
            g_panelBg = ImVec4(0.180f, 0.200f, 0.240f, 1.0f);
            g_titleBg = ImVec4(0.150f, 0.170f, 0.210f, 1.0f);
            g_menuBg = ImVec4(0.190f, 0.210f, 0.250f, 1.0f);
            g_editorBg = ImVec4(0.140f, 0.160f, 0.195f, 1.0f);
            g_termBg = ImVec4(0.120f, 0.140f, 0.175f, 0.985f);
            g_statusBg = ImVec4(0.180f, 0.200f, 0.240f, 1.0f);
            g_borderCol = ImVec4(0.30f, 0.35f, 0.42f, 0.60f);
            break;

        case ThemeId::Dracula:
            g_accent = ImVec4(0.75f, 0.55f, 1.00f, 1.00f);
            g_accent2 = ImVec4(1.00f, 0.47f, 0.78f, 1.00f);
            g_accentPink = ImVec4(1.00f, 0.47f, 0.78f, 1.00f);
            g_accentCyan = ImVec4(0.55f, 0.95f, 0.95f, 1.00f);
            g_textMain = ImVec4(0.97f, 0.97f, 0.95f, 1.00f);
            g_textDim = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
            g_bgTop = ImVec4(0.130f, 0.130f, 0.190f, 1.0f);
            g_bgBottom = ImVec4(0.090f, 0.090f, 0.140f, 1.0f);
            g_panelBg = ImVec4(0.155f, 0.155f, 0.215f, 1.0f);
            g_titleBg = ImVec4(0.130f, 0.130f, 0.190f, 1.0f);
            g_menuBg = ImVec4(0.165f, 0.165f, 0.225f, 1.0f);
            g_editorBg = ImVec4(0.115f, 0.115f, 0.170f, 1.0f);
            g_termBg = ImVec4(0.095f, 0.095f, 0.145f, 0.985f);
            g_statusBg = ImVec4(0.155f, 0.155f, 0.215f, 1.0f);
            g_borderCol = ImVec4(0.35f, 0.30f, 0.45f, 0.60f);
            break;

        case ThemeId::Light:
            g_accent = ImVec4(0.35f, 0.45f, 0.90f, 1.00f);
            g_accent2 = ImVec4(0.65f, 0.35f, 0.90f, 1.00f);
            g_accentPink = ImVec4(0.85f, 0.35f, 0.70f, 1.00f);
            g_accentCyan = ImVec4(0.30f, 0.65f, 0.90f, 1.00f);
            g_textMain = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
            g_textDim = ImVec4(0.45f, 0.45f, 0.52f, 1.00f);
            g_bgTop = ImVec4(0.97f, 0.97f, 0.99f, 1.0f);
            g_bgBottom = ImVec4(0.92f, 0.92f, 0.96f, 1.0f);
            g_panelBg = ImVec4(0.95f, 0.95f, 0.98f, 1.0f);
            g_titleBg = ImVec4(0.93f, 0.93f, 0.96f, 1.0f);
            g_menuBg = ImVec4(0.94f, 0.94f, 0.97f, 1.0f);
            g_editorBg = ImVec4(0.98f, 0.98f, 1.00f, 1.0f);
            g_termBg = ImVec4(0.96f, 0.96f, 0.99f, 0.985f);
            g_statusBg = ImVec4(0.95f, 0.95f, 0.98f, 1.0f);
            g_borderCol = ImVec4(0.75f, 0.75f, 0.80f, 0.60f);
            break;
        }

        applyTheme();
    }

    // ─── Theme ──────────────────────────────────────────────
    void App::applyTheme() {
        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4* c = s.Colors;

        s.WindowRounding = 14.0f;
        s.ChildRounding = 12.0f;
        s.FrameRounding = 10.0f;
        s.PopupRounding = 12.0f;
        s.ScrollbarRounding = 12.0f;
        s.GrabRounding = 10.0f;
        s.TabRounding = 12.0f;

        s.WindowBorderSize = 0.0f;
        s.ChildBorderSize = 1.0f;
        s.FrameBorderSize = 0.0f;
        s.PopupBorderSize = 1.0f;

        s.WindowPadding = ImVec2(14, 14);
        s.FramePadding = ImVec2(10, 6);
        s.ItemSpacing = ImVec2(8, 7);
        s.ItemInnerSpacing = ImVec2(6, 6);
        s.ScrollbarSize = 12.0f;
        s.GrabMinSize = 10.0f;

        bool isLight = (m_themeId == ThemeId::Light);

        c[ImGuiCol_Text] = g_textMain;
        c[ImGuiCol_TextDisabled] = g_textDim;
        c[ImGuiCol_WindowBg] = g_panelBg;
        c[ImGuiCol_ChildBg] = g_bgBottom;
        c[ImGuiCol_PopupBg] = isLight ? ImVec4(0.99f, 0.99f, 1.0f, 0.98f)
            : ImVec4(g_panelBg.x + 0.03f, g_panelBg.y + 0.03f, g_panelBg.z + 0.04f, 0.98f);
        c[ImGuiCol_Border] = g_borderCol;

        c[ImGuiCol_FrameBg] = ImVec4(g_bgTop.x + 0.02f, g_bgTop.y + 0.02f, g_bgTop.z + 0.03f, 1.0f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(g_accent.x * 0.3f, g_accent.y * 0.3f, g_accent.z * 0.3f, 1.0f);
        c[ImGuiCol_FrameBgActive] = ImVec4(g_accent.x * 0.5f, g_accent.y * 0.5f, g_accent.z * 0.5f, 1.0f);

        c[ImGuiCol_TitleBg] = g_titleBg;
        c[ImGuiCol_TitleBgActive] = ImVec4(g_titleBg.x + 0.05f, g_titleBg.y + 0.05f, g_titleBg.z + 0.05f, 1.0f);
        c[ImGuiCol_MenuBarBg] = g_menuBg;

        c[ImGuiCol_ScrollbarBg] = g_bgBottom;
        c[ImGuiCol_ScrollbarGrab] = ImVec4(g_accent.x * 0.55f, g_accent.y * 0.55f, g_accent.z * 0.55f, 0.85f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(g_accent.x * 0.75f, g_accent.y * 0.75f, g_accent.z * 0.75f, 1.0f);
        c[ImGuiCol_ScrollbarGrabActive] = g_accent;

        c[ImGuiCol_CheckMark] = g_accent;
        c[ImGuiCol_SliderGrab] = g_accent;
        c[ImGuiCol_SliderGrabActive] = g_accentPink;

        // Прозрачные ImGui-кнопки — все кастомные
        c[ImGuiCol_Button] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ButtonHovered] = ImVec4(g_accent.x, g_accent.y, g_accent.z, 0.25f);
        c[ImGuiCol_ButtonActive] = ImVec4(g_accent.x, g_accent.y, g_accent.z, 0.45f);

        c[ImGuiCol_Header] = ImVec4(g_accent.x * 0.35f, g_accent.y * 0.35f, g_accent.z * 0.55f, 1.0f);
        c[ImGuiCol_HeaderHovered] = ImVec4(g_accent.x * 0.55f, g_accent.y * 0.55f, g_accent.z * 0.75f, 1.0f);
        c[ImGuiCol_HeaderActive] = ImVec4(g_accent.x * 0.85f, g_accent.y * 0.85f, g_accent.z, 1.0f);

        c[ImGuiCol_Separator] = g_borderCol;

        c[ImGuiCol_Tab] = ImVec4(g_bgTop.x + 0.03f, g_bgTop.y + 0.03f, g_bgTop.z + 0.05f, 1.0f);
        c[ImGuiCol_TabHovered] = ImVec4(g_accent.x * 0.55f, g_accent.y * 0.55f, g_accent.z * 0.75f, 1.0f);
        c[ImGuiCol_TabActive] = ImVec4(g_accent.x * 0.45f, g_accent.y * 0.45f, g_accent.z * 0.65f, 1.0f);

        c[ImGuiCol_NavHighlight] = g_accent;
        c[ImGuiCol_TextSelectedBg] = ImVec4(g_accent.x, g_accent.y, g_accent.z, 0.35f);

        for (auto& tab : m_tabs)
            tab.editor.SetPalette(myMayamiPalette(isLight ? Theme::Light : Theme::Dark));
    }

    // ─── Fonts ──────────────────────────────────────────────
    void App::applyFonts() {
        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 2;
        cfg.PixelSnapH = false;
        cfg.RasterizerMultiply = 1.35f;

        ImFontGlyphRangesBuilder b;
        b.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        b.AddRanges(io.Fonts->GetGlyphRangesDefault());

        ImVector<ImWchar> ranges;
        b.BuildRanges(&ranges);

        const char* boldPaths[] = {
            "assets/fonts/JetBrainsMono-Bold.ttf",
            "assets/fonts/JetBrainsMonoNL-Bold.ttf",
            "D:/Minecraft/mayami/assets/fonts/JetBrainsMono-Bold.ttf",
            "C:/Windows/Fonts/consolab.ttf",
            "C:/Windows/Fonts/CascadiaCode-Bold.ttf",
            "C:/Windows/Fonts/CascadiaMono-Bold.ttf",
        };
        const char* regularPaths[] = {
            "assets/fonts/JetBrainsMono-Regular.ttf",
            "assets/fonts/JetBrainsMonoNL-Regular.ttf",
            "D:/Minecraft/mayami/assets/fonts/JetBrainsMono-Regular.ttf",
            "C:/Windows/Fonts/CascadiaCode.ttf",
            "C:/Windows/Fonts/CascadiaMono.ttf",
            "C:/Windows/Fonts/consola.ttf",
        };

        const float fontSize = 18.0f;

        for (const char* p : boldPaths) {
            FILE* f = std::fopen(p, "rb");
            if (!f) continue;
            std::fclose(f);
            m_fontText = io.Fonts->AddFontFromFileTTF(p, fontSize, &cfg, ranges.Data);
            if (m_fontText) break;
        }
        if (!m_fontText) {
            for (const char* p : regularPaths) {
                FILE* f = std::fopen(p, "rb");
                if (!f) continue;
                std::fclose(f);
                m_fontText = io.Fonts->AddFontFromFileTTF(p, fontSize, &cfg, ranges.Data);
                if (m_fontText) break;
            }
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
        tab.editor.SetPalette(myMayamiPalette(m_theme));
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
            if (path.empty()) { log("Save cancelled"); return; }
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

    void App::openFileInEditor(const std::string& path) {
        for (auto& tab : m_tabs) if (tab.path == path) return;

        std::string content = FileSystem::readFile(path);
        EditorTab tab;
        tab.title = FileSystem::getFileName(path);
        tab.path = path;
        tab.editor.SetText(content);
        tab.savedContent = content;
        tab.editor.SetPalette(myMayamiPalette(m_theme));
        tab.editor.SetLanguage(TextEditor::Language::Cpp());
        m_tabs.push_back(std::move(tab));
        m_activeTab = (int)m_tabs.size() - 1;
        clearOutputLog();
        log("Opened: " + path);
    }

    void App::closeTab(int index) {
        if (index < 0 || index >= (int)m_tabs.size()) return;
        auto& tab = m_tabs[index];

        if (tab.modified) {
            std::string msg = "Файл \"" + tab.title + "\" изменён. Сохранить?";
            int res = MessageBoxA(g_hwnd, msg.c_str(), "Mayami IDE",
                MB_YESNOCANCEL | MB_ICONQUESTION);
            if (res == IDCANCEL) return;
            if (res == IDYES) { m_activeTab = index; saveFile(); }
        }

        m_tabs.erase(m_tabs.begin() + index);
        if (m_activeTab >= (int)m_tabs.size())
            m_activeTab = (int)m_tabs.size() - 1;
        if (m_activeTab < 0 && !m_tabs.empty()) m_activeTab = 0;
    }

    // ─── runCode / buildExe / runExe (без изменений) ────────
    void App::runCode() {
        if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) {
            logError("No active tab"); return;
        }
        auto& tab = m_tabs[m_activeTab];
        clearBuildLog();
        clearOutputLog();
        log("================================");
        log("Quick Run: " + tab.title);
        log("================================");

        std::string source = tab.editor.GetText();

        log(""); log("[1/4] Lexer...");
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        log("  -> " + std::to_string(tokens.size()) + " tokens");
        if (lexer.hasErrors()) {
            logError("  ERROR Lexer errors:");
            for (const auto& err : lexer.errors()) logError("    " + err);
            log(""); logError("Compilation aborted.");
            logOutput("Compilation failed (lexer errors)");
            m_showTerminalWindow = true; return;
        }

        log(""); log("[2/4] Parser...");
        Parser parser(tokens);
        std::unique_ptr<Program> program = parser.parse();
        if (!program || parser.hasErrors()) {
            logError("  ERROR Parser errors:");
            for (const auto& err : parser.errors()) logError("    " + err);
            log(""); logError("Compilation aborted.");
            logOutput("Compilation failed (parser errors)");
            m_showTerminalWindow = true; return;
        }
        logSuccess("  OK Parse successful!");

        log(""); log("[3/4] BytecodeGen...");
        BytecodeGen gen;
        std::vector<uint8_t> bytecode = gen.generate(program.get());
        if (gen.hasErrors()) {
            logError("  ERROR BytecodeGen errors:");
            for (const auto& err : gen.errors()) logError("    " + err);
            log(""); logError("Compilation aborted.");
            logOutput("Compilation failed (bytecode errors)");
            m_showTerminalWindow = true; return;
        }
        logSuccess("  OK Bytecode: " + std::to_string(bytecode.size()) + " bytes");

        log(""); log("[4/4] VM...");
        VM vm;
        vm.setOutputCallback([this](const std::string& s) { logOutput(s); });
        vm.setDebug(m_debugMode);
        vm.setDebugCallback([this](const std::string& s) { log(s); });

        if (!vm.load(bytecode)) {
            logError("  ERROR VM load errors:");
            for (const auto& err : vm.errors()) logError("    " + err);
            logOutput("=== VM failed to load ===");
            m_showTerminalWindow = true; m_terminalTab = 1; return;
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
            for (const auto& err : vm.errors()) logError("    " + err);
            logOutput("=== Program failed ===");
        }
        m_showTerminalWindow = true;
        m_terminalTab = 1;
    }

    void App::buildExe() {
        if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) {
            logError("No active tab");
            m_showTerminalWindow = true; m_terminalTab = 0; return;
        }
        auto& tab = m_tabs[m_activeTab];
        if (tab.path.empty()) {
            clearBuildLog();
            logError("Сначала сохрани файл (Ctrl+S), потом F5");
            m_showTerminalWindow = true; m_terminalTab = 0; return;
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
            m_showTerminalWindow = true; m_terminalTab = 0; return;
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

    void App::runExe() {
        if (m_lastBuiltExe.empty()) {
            logError("Сначала собери .exe (F5 или Ctrl+B)");
            m_showTerminalWindow = true; m_terminalTab = 0; return;
        }
        if (fileSize(m_lastBuiltExe) < 0) {
            logError("Файл не найден: " + m_lastBuiltExe);
            logError("Пересобери (F5 или Ctrl+B)");
            m_showTerminalWindow = true; m_terminalTab = 0; return;
        }
        clearOutputLog();
        logOutput("=== Launching: " + m_lastBuiltExe + " ===");
        logOutput("");

        HINSTANCE h = ShellExecuteA(nullptr, "open", m_lastBuiltExe.c_str(),
            nullptr, nullptr, SW_SHOWNORMAL);

        if ((INT_PTR)h <= 32) {
            logOutput("[stderr] Не удалось запустить (код " + std::to_string((INT_PTR)h) + ")");
            logOutput("=== Launch failed ===");
        }
        else {
            logOutput("(запущено в отдельном окне)");
            logOutput("");
            logOutput("=== Done ===");
        }
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