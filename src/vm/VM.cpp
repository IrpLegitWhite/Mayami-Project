// src/vm/VM.cpp
#include "VM.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <chrono>
#include <random>

#include <windows.h>
#include <windowsx.h>

namespace mayami {

    // ★ Глобальные для time_ms и random
    static std::chrono::steady_clock::time_point g_startTime = std::chrono::steady_clock::now();
    static std::mt19937_64 g_rng{ std::random_device{}() };

    // ============================================================
    // UTF-8 ↔ UTF-16
    // ============================================================
    namespace {
        static std::wstring utf8_to_wstring(const std::string& s) {
            if (s.empty()) return L"";
            int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
            if (wlen <= 0) return L"";
            std::wstring out(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], wlen);
            return out;
        }
        static std::string wstring_to_utf8(const std::wstring& w) {
            if (w.empty()) return "";
            int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
            if (len <= 0) return "";
            std::string out(len, 0);
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], len, nullptr, nullptr);
            return out;
        }
    }

    // ============================================================
    // Кастомный UI
    // ============================================================
    namespace {

        enum class UiElemType { Rect, RectRound, Text, Button };

        struct UiElem {
            UiElemType type = UiElemType::Rect;
            int x = 0, y = 0, w = 0, h = 0;
            int radius = 0;
            COLORREF color = RGB(200, 200, 200);
            COLORREF textColor = RGB(0, 0, 0);
            int textSize = 16;
            std::string text;
            std::string id;
            bool hover = false;
            bool pressed = false;
        };

        struct UiState {
            HWND hwnd = nullptr;
            int w = 600;
            int h = 400;
            COLORREF bgColor = RGB(30, 30, 40);
            bool closed = false;
            std::string clickedId;
            std::vector<UiElem> elems;
            int mouseX = 0, mouseY = 0;
            std::string pressedId;
        };

        static UiState g_ui;

        static bool hitTest(const UiElem& e, int mx, int my) {
            return mx >= e.x && mx < e.x + e.w &&
                my >= e.y && my < e.y + e.h;
        }

        static HFONT makeFont(int size) {
            if (size < 8) size = 8;
            if (size > 72) size = 72;
            return CreateFontW(
                -size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        }

        static void paintAll(HWND hwnd) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            HBRUSH bgBrush = CreateSolidBrush(g_ui.bgColor);
            FillRect(memDC, &rc, bgBrush);
            DeleteObject(bgBrush);

            SetBkMode(memDC, TRANSPARENT);

            for (auto& e : g_ui.elems) {
                switch (e.type) {

                case UiElemType::Rect: {
                    HBRUSH brush = CreateSolidBrush(e.color);
                    RECT r = { e.x, e.y, e.x + e.w, e.y + e.h };
                    FillRect(memDC, &r, brush);
                    DeleteObject(brush);
                    break;
                }

                case UiElemType::RectRound: {
                    HBRUSH brush = CreateSolidBrush(e.color);
                    HPEN pen = CreatePen(PS_NULL, 0, 0);
                    HGDIOBJ oldBrush = SelectObject(memDC, brush);
                    HGDIOBJ oldPen = SelectObject(memDC, pen);

                    RoundRect(memDC, e.x, e.y, e.x + e.w, e.y + e.h,
                        e.radius * 2, e.radius * 2);

                    SelectObject(memDC, oldBrush);
                    SelectObject(memDC, oldPen);
                    DeleteObject(brush);
                    DeleteObject(pen);
                    break;
                }

                case UiElemType::Text: {
                    HFONT f = makeFont(e.textSize);
                    HFONT oldF = (HFONT)SelectObject(memDC, f);
                    SetTextColor(memDC, e.textColor);

                    std::wstring w = utf8_to_wstring(e.text);
                    TextOutW(memDC, e.x, e.y, w.c_str(), (int)w.size());

                    SelectObject(memDC, oldF);
                    DeleteObject(f);
                    break;
                }

                case UiElemType::Button: {
                    COLORREF base = e.color;
                    int r = GetRValue(base);
                    int g = GetGValue(base);
                    int b = GetBValue(base);

                    if (e.pressed) {
                        r = r * 7 / 10;
                        g = g * 7 / 10;
                        b = b * 7 / 10;
                    }
                    else if (e.hover) {
                        r = r + 40 > 255 ? 255 : r + 40;
                        g = g + 40 > 255 ? 255 : g + 40;
                        b = b + 40 > 255 ? 255 : b + 40;
                    }

                    COLORREF col = RGB(r, g, b);
                    HBRUSH brush = CreateSolidBrush(col);
                    HPEN pen = CreatePen(PS_NULL, 0, 0);
                    HGDIOBJ oldBrush = SelectObject(memDC, brush);
                    HGDIOBJ oldPen = SelectObject(memDC, pen);

                    RoundRect(memDC, e.x, e.y, e.x + e.w, e.y + e.h,
                        e.radius * 2, e.radius * 2);

                    SelectObject(memDC, oldBrush);
                    SelectObject(memDC, oldPen);
                    DeleteObject(brush);
                    DeleteObject(pen);

                    HFONT f = makeFont(e.textSize);
                    HFONT oldF = (HFONT)SelectObject(memDC, f);
                    SetTextColor(memDC, e.textColor);

                    std::wstring w = utf8_to_wstring(e.text);
                    SIZE sz;
                    GetTextExtentPoint32W(memDC, w.c_str(), (int)w.size(), &sz);

                    int tx = e.x + (e.w - sz.cx) / 2;
                    int ty = e.y + (e.h - sz.cy) / 2;

                    TextOutW(memDC, tx, ty, w.c_str(), (int)w.size());

                    SelectObject(memDC, oldF);
                    DeleteObject(f);
                    break;
                }
                }
            }

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
        }

        static LRESULT CALLBACK UiWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            switch (msg) {

            case WM_PAINT:
                paintAll(hwnd);
                return 0;

            case WM_ERASEBKGND:
                return 1;

            case WM_MOUSEMOVE: {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                g_ui.mouseX = mx;
                g_ui.mouseY = my;

                bool needRepaint = false;
                for (auto& e : g_ui.elems) {
                    if (e.type == UiElemType::Button) {
                        bool h = hitTest(e, mx, my);
                        if (h != e.hover) {
                            e.hover = h;
                            needRepaint = true;
                        }
                    }
                }
                if (needRepaint) InvalidateRect(hwnd, nullptr, FALSE);

                bool anyHover = false;
                for (auto& e : g_ui.elems)
                    if (e.type == UiElemType::Button && e.hover) anyHover = true;

                SetCursor(LoadCursor(nullptr, anyHover ? IDC_HAND : IDC_ARROW));
                return 0;
            }

            case WM_LBUTTONDOWN: {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);

                for (auto& e : g_ui.elems) {
                    if (e.type == UiElemType::Button && hitTest(e, mx, my)) {
                        e.pressed = true;
                        g_ui.pressedId = e.id;
                        InvalidateRect(hwnd, nullptr, FALSE);
                        break;
                    }
                }
                SetCapture(hwnd);
                return 0;
            }

            case WM_LBUTTONUP: {
                ReleaseCapture();
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);

                for (auto& e : g_ui.elems) {
                    if (e.type == UiElemType::Button && e.pressed) {
                        e.pressed = false;
                        if (hitTest(e, mx, my)) {
                            g_ui.clickedId = e.id;
                        }
                        InvalidateRect(hwnd, nullptr, FALSE);
                        break;
                    }
                }
                g_ui.pressedId.clear();
                return 0;
            }

            case WM_CLOSE:
                g_ui.closed = true;
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                g_ui.hwnd = nullptr;
                g_ui.closed = true;
                PostQuitMessage(0);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        static void registerUiClass() {
            static bool registered = false;
            if (registered) return;
            registered = true;

            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(wc);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = UiWndProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            wc.lpszClassName = L"MayamiCanvas";
            RegisterClassExW(&wc);
        }

        static void pumpMessages() {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        static void resetUi() {
            g_ui.elems.clear();
            g_ui.clickedId.clear();
            g_ui.pressedId.clear();
            g_ui.closed = false;
            g_ui.mouseX = 0;
            g_ui.mouseY = 0;

            if (g_ui.hwnd && IsWindow(g_ui.hwnd)) {
                DestroyWindow(g_ui.hwnd);
                g_ui.hwnd = nullptr;
            }
        }

    } // anon namespace

    // ============================================================
    // VmValue
    // ============================================================
    std::string VmValue::toString() const {
        switch (type) {
        case VmType::Null:   return "null";
        case VmType::Int:    return std::to_string(i);
        case VmType::Float: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%g", f);
            return buf;
        }
        case VmType::Bool:   return b ? "true" : "false";
        case VmType::String: return s;
        case VmType::Char:   return s;

        case VmType::Array: {
            std::string r = "[";
            if (array) {
                for (size_t k = 0; k < array->size(); ++k) {
                    r += (*array)[k].toString();
                    if (k + 1 < array->size()) r += ", ";
                }
            }
            r += "]";
            return r;
        }

        case VmType::Struct: {
            return "Struct#" + std::to_string(structId);
        }
        }
        return "?";
    }

    double VmValue::toNumber() const {
        switch (type) {
        case VmType::Int:    return (double)i;
        case VmType::Float:  return f;
        case VmType::Bool:   return b ? 1.0 : 0.0;
        default:             return 0.0;
        }
    }

    // ============================================================
    // VM
    // ============================================================
    VM::VM() {}
    VM::~VM() = default;

    void VM::error(const std::string& msg) {
        m_errors.push_back(msg);
        if (m_debug && m_debugOut) {
            m_debugOut("[VM][ERROR] " + msg);
        }
    }

    VmValue VM::pop() {
        if (m_stack.empty()) {
            if (m_debug && m_debugOut) {
                std::ostringstream ss;
                ss << "[VM] POP-EMPTY! ip=" << m_ip
                    << " vars=" << m_vars.size()
                    << " frames=" << m_frames.size();
                m_debugOut(ss.str());
            }
            error("Stack underflow (recovered)");
            return VmValue();
        }
        VmValue v = m_stack.back();
        m_stack.pop_back();
        return v;
    }

    VmValue& VM::peek(int offset) {
        static VmValue nullVal;
        if (m_stack.size() <= (size_t)offset) return nullVal;
        size_t idx = m_stack.size() - 1 - offset;
        return m_stack[idx];
    }

    uint8_t VM::readByte() {
        if (m_ip >= m_code.size()) {
            error("Code overrun");
            throw VmRuntimeError{ "Code overrun" };
        }
        return m_code[m_ip++];
    }

    uint16_t VM::readU16() {
        if (m_ip + 2 > m_code.size()) throw VmRuntimeError{ "Code overrun" };
        uint16_t v = mayami::readU16(&m_code[m_ip]);
        m_ip += 2;
        return v;
    }

    uint32_t VM::readU32() {
        if (m_ip + 4 > m_code.size()) throw VmRuntimeError{ "Code overrun" };
        uint32_t v = mayami::readU32(&m_code[m_ip]);
        m_ip += 4;
        return v;
    }

    int64_t VM::readI64() {
        if (m_ip + 8 > m_code.size()) throw VmRuntimeError{ "Code overrun" };
        int64_t v = mayami::readI64(&m_code[m_ip]);
        m_ip += 8;
        return v;
    }

    double VM::readF64() {
        if (m_ip + 8 > m_code.size()) throw VmRuntimeError{ "Code overrun" };
        double v = mayami::readF64(&m_code[m_ip]);
        m_ip += 8;
        return v;
    }

    std::string VM::stringAt(uint32_t offset) {
        if (offset + 4 > m_constants.size()) return "";
        uint32_t len = mayami::readU32(&m_constants[offset]);
        if (offset + 4 + len > m_constants.size()) return "";
        return std::string((const char*)&m_constants[offset + 4], len);
    }

    std::string VM::readString() {
        uint32_t offset = readU32();
        return stringAt(offset);
    }

    bool VM::load(const std::vector<uint8_t>& data) {
        if (data.size() < sizeof(BytecodeHeader)) {
            error("File too small");
            return false;
        }

        const BytecodeHeader* hdr = (const BytecodeHeader*)data.data();

        if (hdr->magic != BYTECODE_MAGIC) { error("Invalid magic"); return false; }
        if (hdr->version != BYTECODE_VERSION) { error("Version mismatch"); return false; }

        size_t pos = sizeof(BytecodeHeader);

        if (pos + hdr->constSize > data.size()) { error("Bad constSize"); return false; }
        m_constants.assign(data.begin() + pos, data.begin() + pos + hdr->constSize);
        pos += hdr->constSize;

        if (pos + hdr->funcCount * sizeof(BytecodeFunction) > data.size()) {
            error("Bad funcCount"); return false;
        }
        m_functions.resize(hdr->funcCount);
        for (uint32_t i = 0; i < hdr->funcCount; ++i) {
            std::memcpy(&m_functions[i], data.data() + pos, sizeof(BytecodeFunction));
            pos += sizeof(BytecodeFunction);
        }

        pos += hdr->structCount * sizeof(BytecodeStruct);

        if (pos + hdr->codeSize > data.size()) { error("Bad codeSize"); return false; }
        m_code.assign(data.begin() + pos, data.begin() + pos + hdr->codeSize);
        pos += hdr->codeSize;

        m_entryPoint = hdr->entryPoint;

        if (m_debug && m_debugOut) {
            std::ostringstream ss;
            ss << "[VM] Loaded: code=" << m_code.size()
                << " const=" << m_constants.size()
                << " funcs=" << m_functions.size()
                << " entry=" << m_entryPoint;
            m_debugOut(ss.str());
        }

        return true;
    }
    // ─── Native ─────────────────────────────────────────────
    VmValue VM::callNative(uint32_t id, const std::vector<VmValue>& args) {
        if (m_debug && m_debugOut) {
            std::ostringstream ss;
            ss << "[VM]   -> CALL_NATIVE id=" << id << " argc=" << args.size();
            m_debugOut(ss.str());
        }

        switch (id) {
            // ─── Вывод ─────────────────────────────────────────────
        case NATIVE_PRINTLN:
        case NATIVE_PRINT:
        case NATIVE_EPRINT: {
            std::string s;
            for (size_t i = 0; i < args.size(); ++i) {
                s += args[i].toString();
                if (i + 1 < args.size()) s += " ";
            }
            if (m_output) m_output(s);
            return VmValue();
        }

                          // ─── Математика (базовая) ──────────────────────────────
        case NATIVE_SQRT:
            if (args.size() == 1) return VmValue((double)std::sqrt(args[0].toNumber()));
            break;
        case NATIVE_ABS:
            if (args.size() == 1) {
                if (args[0].type == VmType::Int)
                    return VmValue((int64_t)(args[0].i < 0 ? -args[0].i : args[0].i));
                return VmValue((double)std::fabs(args[0].toNumber()));
            }
            break;
        case NATIVE_SIN:
            if (args.size() == 1) return VmValue((double)std::sin(args[0].toNumber()));
            break;
        case NATIVE_COS:
            if (args.size() == 1) return VmValue((double)std::cos(args[0].toNumber()));
            break;
        case NATIVE_TAN:
            if (args.size() == 1) return VmValue((double)std::tan(args[0].toNumber()));
            break;
        case NATIVE_POW:
            if (args.size() == 2) return VmValue((double)std::pow(args[0].toNumber(), args[1].toNumber()));
            break;
        case NATIVE_FLOOR:
            if (args.size() == 1) return VmValue((int64_t)std::floor(args[0].toNumber()));
            break;
        case NATIVE_CEIL:
            if (args.size() == 1) return VmValue((int64_t)std::ceil(args[0].toNumber()));
            break;
        case NATIVE_ROUND:
            if (args.size() == 1) return VmValue((int64_t)std::round(args[0].toNumber()));
            break;
        case NATIVE_MIN:
            if (args.size() == 2) {
                if (args[0].type == VmType::Int && args[1].type == VmType::Int)
                    return VmValue((int64_t)(args[0].i < args[1].i ? args[0].i : args[1].i));
                return VmValue((double)std::min(args[0].toNumber(), args[1].toNumber()));
            }
            break;
        case NATIVE_MAX:
            if (args.size() == 2) {
                if (args[0].type == VmType::Int && args[1].type == VmType::Int)
                    return VmValue((int64_t)(args[0].i > args[1].i ? args[0].i : args[1].i));
                return VmValue((double)std::max(args[0].toNumber(), args[1].toNumber()));
            }
            break;

            // ─── Математика (расширенная) ──────────────────────────
        case NATIVE_LOG:
            if (args.size() == 1) return VmValue((double)std::log(args[0].toNumber()));
            break;
        case NATIVE_LOG2:
            if (args.size() == 1) return VmValue((double)std::log2(args[0].toNumber()));
            break;
        case NATIVE_LOG10:
            if (args.size() == 1) return VmValue((double)std::log10(args[0].toNumber()));
            break;
        case NATIVE_EXP:
            if (args.size() == 1) return VmValue((double)std::exp(args[0].toNumber()));
            break;
        case NATIVE_ASIN:
            if (args.size() == 1) return VmValue((double)std::asin(args[0].toNumber()));
            break;
        case NATIVE_ACOS:
            if (args.size() == 1) return VmValue((double)std::acos(args[0].toNumber()));
            break;
        case NATIVE_ATAN:
            if (args.size() == 1) return VmValue((double)std::atan(args[0].toNumber()));
            break;
        case NATIVE_ATAN2:
            if (args.size() == 2) return VmValue((double)std::atan2(args[0].toNumber(), args[1].toNumber()));
            break;
        case NATIVE_SINH:
            if (args.size() == 1) return VmValue((double)std::sinh(args[0].toNumber()));
            break;
        case NATIVE_COSH:
            if (args.size() == 1) return VmValue((double)std::cosh(args[0].toNumber()));
            break;
        case NATIVE_TANH:
            if (args.size() == 1) return VmValue((double)std::tanh(args[0].toNumber()));
            break;
        case NATIVE_HYPOT:
            if (args.size() == 2) return VmValue((double)std::hypot(args[0].toNumber(), args[1].toNumber()));
            break;
        case NATIVE_CLAMP: {
            if (args.size() == 3) {
                double x = args[0].toNumber();
                double lo = args[1].toNumber();
                double hi = args[2].toNumber();
                if (lo > hi) std::swap(lo, hi);
                double result = x < lo ? lo : (x > hi ? hi : x);
                if (args[0].type == VmType::Int &&
                    args[1].type == VmType::Int &&
                    args[2].type == VmType::Int)
                    return VmValue((int64_t)result);
                return VmValue(result);
            }
            break;
        }
        case NATIVE_SIGN: {
            if (args.size() == 1) {
                double x = args[0].toNumber();
                return VmValue((int64_t)(x > 0 ? 1 : (x < 0 ? -1 : 0)));
            }
            break;
        }
        case NATIVE_PI:
            return VmValue(3.14159265358979323846);
        case NATIVE_E:
            return VmValue(2.71828182845904523536);
        case NATIVE_DEG2RAD:
            if (args.size() == 1) return VmValue(args[0].toNumber() * 3.14159265358979323846 / 180.0);
            break;
        case NATIVE_RAD2DEG:
            if (args.size() == 1) return VmValue(args[0].toNumber() * 180.0 / 3.14159265358979323846);
            break;

            // ─── RANDOM ────────────────────────────────────────────
        case NATIVE_RANDOM: {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            return VmValue(dist(g_rng));
        }
        case NATIVE_RANDOM_INT: {
            if (args.size() >= 2) {
                int64_t a = (int64_t)args[0].toNumber();
                int64_t b = (int64_t)args[1].toNumber();
                if (a > b) std::swap(a, b);
                std::uniform_int_distribution<int64_t> dist(a, b);
                return VmValue(dist(g_rng));
            }
            break;
        }

                              // ─── Строки (методы) ───────────────────────────────────
        case NATIVE_STR_LEN:
            if (args.size() == 1) return VmValue((int64_t)args[0].s.length());
            break;
        case NATIVE_STR_UPPER: {
            if (args.size() == 1) {
                std::string r = args[0].s;
                for (auto& c : r) c = (char)std::toupper((unsigned char)c);
                VmValue v; v.type = VmType::String; v.s = r; return v;
            }
            break;
        }
        case NATIVE_STR_LOWER: {
            if (args.size() == 1) {
                std::string r = args[0].s;
                for (auto& c : r) c = (char)std::tolower((unsigned char)c);
                VmValue v; v.type = VmType::String; v.s = r; return v;
            }
            break;
        }
        case NATIVE_STR_SUBSTR: {
            if (args.size() == 3) {
                long long start = args[1].i;
                long long len = args[2].i;
                const std::string& src = args[0].s;
                if (start < 0) start = 0;
                if (start >= (long long)src.length()) {
                    VmValue v; v.type = VmType::String; v.s = ""; return v;
                }
                if (start + len > (long long)src.length())
                    len = (long long)src.length() - start;
                VmValue v; v.type = VmType::String;
                v.s = src.substr((size_t)start, (size_t)len);
                return v;
            }
            break;
        }
        case NATIVE_STR_FIND: {
            if (args.size() == 2) {
                size_t pos = args[0].s.find(args[1].s);
                return VmValue((int64_t)(pos == std::string::npos ? -1 : (long long)pos));
            }
            break;
        }
        case NATIVE_STR_CONTAINS:
            if (args.size() == 2) {
                bool c = args[0].s.find(args[1].s) != std::string::npos;
                return VmValue(c);
            }
            break;
        case NATIVE_STR_REPLACE: {
            if (args.size() == 3) {
                const std::string& src = args[0].s;
                const std::string& from = args[1].s;
                const std::string& to = args[2].s;
                std::string r = src;
                if (!from.empty()) {
                    size_t pos = 0;
                    while ((pos = r.find(from, pos)) != std::string::npos) {
                        r.replace(pos, from.length(), to);
                        pos += to.length();
                    }
                }
                VmValue v; v.type = VmType::String; v.s = r; return v;
            }
            break;
        }

                               // ═══════════════════════════════════════════════════════
                               // ★★★ НОВЫЕ СТРОКОВЫЕ МЕТОДЫ (v1.1) ★★★
                               // ═══════════════════════════════════════════════════════
        case NATIVE_STR_TRIM: {
            if (args.size() >= 1) {
                std::string s = args[0].s;
                size_t start = s.find_first_not_of(" \t\n\r");
                size_t end = s.find_last_not_of(" \t\n\r");
                VmValue r; r.type = VmType::String;
                if (start == std::string::npos) r.s = "";
                else r.s = s.substr(start, end - start + 1);
                return r;
            }
            break;
        }
        case NATIVE_STR_STARTS_WITH: {
            if (args.size() >= 2) {
                std::string s = args[0].s;
                std::string prefix = args[1].s;
                bool result = s.size() >= prefix.size() &&
                    s.compare(0, prefix.size(), prefix) == 0;
                return VmValue(result);
            }
            break;
        }
        case NATIVE_STR_ENDS_WITH: {
            if (args.size() >= 2) {
                std::string s = args[0].s;
                std::string suffix = args[1].s;
                bool result = s.size() >= suffix.size() &&
                    s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
                return VmValue(result);
            }
            break;
        }
        case NATIVE_STR_REPEAT: {
            if (args.size() >= 2) {
                std::string s = args[0].s;
                int64_t n = (int64_t)args[1].toNumber();
                std::string result;
                if (n > 0) {
                    result.reserve(s.size() * n);
                    for (int64_t i = 0; i < n; ++i) result += s;
                }
                VmValue r; r.type = VmType::String; r.s = result;
                return r;
            }
            break;
        }
        case NATIVE_STR_CHAR_AT: {
            if (args.size() >= 2) {
                std::string s = args[0].s;
                int64_t i = (int64_t)args[1].toNumber();
                VmValue r; r.type = VmType::String;
                if (i < 0 || i >= (int64_t)s.size()) r.s = "";
                else r.s = std::string(1, s[(size_t)i]);
                return r;
            }
            break;
        }
        case NATIVE_STR_REVERSE: {
            if (args.size() >= 1) {
                std::string s = args[0].s;
                std::reverse(s.begin(), s.end());
                VmValue r; r.type = VmType::String; r.s = s;
                return r;
            }
            break;
        }
        case NATIVE_STR_PAD_LEFT: {
            if (args.size() >= 3) {
                std::string s = args[0].s;
                int64_t n = (int64_t)args[1].toNumber();
                std::string ch = args[2].s;
                char c = ch.empty() ? ' ' : ch[0];
                while ((int64_t)s.size() < n) s = c + s;
                VmValue r; r.type = VmType::String; r.s = s;
                return r;
            }
            break;
        }
        case NATIVE_STR_PAD_RIGHT: {
            if (args.size() >= 3) {
                std::string s = args[0].s;
                int64_t n = (int64_t)args[1].toNumber();
                std::string ch = args[2].s;
                char c = ch.empty() ? ' ' : ch[0];
                while ((int64_t)s.size() < n) s += c;
                VmValue r; r.type = VmType::String; r.s = s;
                return r;
            }
            break;
        }
        case NATIVE_STR_INDEX_OF: {
            if (args.size() >= 2) {
                std::string s = args[0].s;
                std::string needle = args[1].s;
                size_t pos = s.find(needle);
                return VmValue((int64_t)(pos == std::string::npos ? -1 : (long long)pos));
            }
            break;
        }
        case NATIVE_STR_TO_INT: {
            if (args.size() >= 1) {
                try { return VmValue((int64_t)std::stoll(args[0].s)); }
                catch (...) { return VmValue((int64_t)0); }
            }
            break;
        }
        case NATIVE_STR_TO_FLOAT: {
            if (args.size() >= 1) {
                try { return VmValue(std::stod(args[0].s)); }
                catch (...) { return VmValue(0.0); }
            }
            break;
        }

                                // ─── Строки (глобальные) ───────────────────────────────
        case NATIVE_STR_AT: {
            if (args.size() >= 2) {
                const std::string& s = args[0].s;
                int64_t idx = (int64_t)args[1].toNumber();
                if (idx < 0 || idx >= (int64_t)s.size()) {
                    VmValue r; r.type = VmType::Char; r.s = ""; return r;
                }
                VmValue r; r.type = VmType::Char;
                r.s = std::string(1, s[(size_t)idx]);
                return r;
            }
            break;
        }
        case NATIVE_TRIM: {
            if (args.size() >= 1) {
                std::string s = args[0].s;
                size_t start = s.find_first_not_of(" \t\r\n");
                size_t end = s.find_last_not_of(" \t\r\n");
                VmValue r; r.type = VmType::String;
                if (start == std::string::npos) r.s = "";
                else r.s = s.substr(start, end - start + 1);
                return r;
            }
            break;
        }
        case NATIVE_STR_SPLIT: {
            if (args.size() >= 2) {
                const std::string& s = args[0].s;
                const std::string& delim = args[1].s;
                if (delim.empty()) {
                    VmValue r; r.type = VmType::String; r.s = s;
                    return r;
                }
                std::string result;
                size_t pos = 0, found;
                bool first = true;
                while ((found = s.find(delim, pos)) != std::string::npos) {
                    if (!first) result += "|";
                    result += s.substr(pos, found - pos);
                    first = false;
                    pos = found + delim.size();
                }
                if (!first) result += "|";
                result += s.substr(pos);
                VmValue r; r.type = VmType::String; r.s = result;
                return r;
            }
            break;
        }
        case NATIVE_STR_JOIN: {
            if (args.size() >= 2) {
                VmValue r; r.type = VmType::String;
                r.s = args[0].s + "|" + args[1].s;
                return r;
            }
            break;
        }
        case NATIVE_TO_UPPER: {
            if (args.size() >= 1) {
                std::string r = args[0].s;
                for (auto& c : r) c = (char)std::toupper((unsigned char)c);
                VmValue v; v.type = VmType::String; v.s = r; return v;
            }
            break;
        }
        case NATIVE_TO_LOWER: {
            if (args.size() >= 1) {
                std::string r = args[0].s;
                for (auto& c : r) c = (char)std::tolower((unsigned char)c);
                VmValue v; v.type = VmType::String; v.s = r; return v;
            }
            break;
        }

                            // ═══════════════════════════════════════════════════════
                            // ★★★ ПРЕОБРАЗОВАНИЯ ТИПОВ ★★★
                            // ═══════════════════════════════════════════════════════
        case NATIVE_INT: {
            if (args.size() >= 1) {
                const VmValue& a = args[0];
                if (a.type == VmType::Int)   return VmValue(a.i);
                if (a.type == VmType::Float) return VmValue((int64_t)a.f);
                if (a.type == VmType::Bool)  return VmValue((int64_t)(a.b ? 1 : 0));
                if (a.type == VmType::String || a.type == VmType::Char) {
                    std::string s = a.s;
                    size_t start = s.find_first_not_of(" \t\r\n");
                    if (start == std::string::npos) return VmValue((int64_t)0);
                    size_t end = s.find_last_not_of(" \t\r\n");
                    s = s.substr(start, end - start + 1);
                    try {
                        if (s.find('.') != std::string::npos)
                            return VmValue((int64_t)std::stod(s));
                        return VmValue((int64_t)std::stoll(s));
                    }
                    catch (...) { return VmValue((int64_t)0); }
                }
                return VmValue((int64_t)0);
            }
            break;
        }
        case NATIVE_FLOAT: {
            if (args.size() >= 1) {
                const VmValue& a = args[0];
                if (a.type == VmType::Float) return VmValue(a.f);
                if (a.type == VmType::Int)   return VmValue((double)a.i);
                if (a.type == VmType::Bool)  return VmValue((double)(a.b ? 1.0 : 0.0));
                if (a.type == VmType::String || a.type == VmType::Char) {
                    try { return VmValue(std::stod(a.s)); }
                    catch (...) { return VmValue(0.0); }
                }
                return VmValue(0.0);
            }
            break;
        }
        case NATIVE_STR: {
            if (args.size() >= 1) {
                VmValue r; r.type = VmType::String;
                r.s = args[0].toString();
                return r;
            }
            break;
        }
        case NATIVE_BOOL: {
            if (args.size() >= 1) {
                const VmValue& a = args[0];
                bool result = false;
                switch (a.type) {
                case VmType::Null:   result = false; break;
                case VmType::Int:    result = (a.i != 0); break;
                case VmType::Float:  result = (a.f != 0.0); break;
                case VmType::Bool:   result = a.b; break;
                case VmType::String: result = !a.s.empty(); break;
                case VmType::Char:   result = !a.s.empty(); break;
                default:             result = true; break;
                }
                return VmValue(result);
            }
            break;
        }
        case NATIVE_CHAR: {
            if (args.size() >= 1) {
                int64_t code = (int64_t)args[0].toNumber();
                if (code < 0 || code > 0x10FFFF) code = '?';
                VmValue r; r.type = VmType::Char;
                if (code < 0x80) {
                    r.s = std::string(1, (char)code);
                }
                else if (code < 0x800) {
                    r.s += (char)(0xC0 | (code >> 6));
                    r.s += (char)(0x80 | (code & 0x3F));
                }
                else if (code < 0x10000) {
                    r.s += (char)(0xE0 | (code >> 12));
                    r.s += (char)(0x80 | ((code >> 6) & 0x3F));
                    r.s += (char)(0x80 | (code & 0x3F));
                }
                else {
                    r.s += (char)(0xF0 | (code >> 18));
                    r.s += (char)(0x80 | ((code >> 12) & 0x3F));
                    r.s += (char)(0x80 | ((code >> 6) & 0x3F));
                    r.s += (char)(0x80 | (code & 0x3F));
                }
                return r;
            }
            break;
        }
        case NATIVE_ORD: {
            if (args.size() >= 1) {
                const std::string& s = args[0].s;
                if (s.empty()) return VmValue((int64_t)0);
                unsigned char c = (unsigned char)s[0];
                int64_t code = 0;
                if (c < 0x80) code = c;
                else if ((c & 0xE0) == 0xC0 && s.size() >= 2)
                    code = ((c & 0x1F) << 6) | ((unsigned char)s[1] & 0x3F);
                else if ((c & 0xF0) == 0xE0 && s.size() >= 3)
                    code = ((c & 0x0F) << 12) | (((unsigned char)s[1] & 0x3F) << 6) | ((unsigned char)s[2] & 0x3F);
                else if ((c & 0xF8) == 0xF0 && s.size() >= 4)
                    code = ((c & 0x07) << 18) | (((unsigned char)s[1] & 0x3F) << 12) |
                    (((unsigned char)s[2] & 0x3F) << 6) | ((unsigned char)s[3] & 0x3F);
                else code = c;
                return VmValue(code);
            }
            break;
        }

                       // ═══════════════════════════════════════════════════════
                       // ★★★ ФАЙЛЫ / I/O (v1.1) ★★★
                       // ═══════════════════════════════════════════════════════
        case NATIVE_READ_FILE: {
            if (args.size() >= 1) {
                std::ifstream f(args[0].s, std::ios::binary);
                VmValue r; r.type = VmType::String;
                if (!f) { r.s = ""; return r; }
                std::stringstream ss;
                ss << f.rdbuf();
                r.s = ss.str();
                return r;
            }
            break;
        }
        case NATIVE_WRITE_FILE: {
            if (args.size() >= 2) {
                std::ofstream f(args[0].s, std::ios::binary);
                if (f) f.write(args[1].s.data(), args[1].s.size());
                return VmValue(f.good());
            }
            break;
        }
        case NATIVE_APPEND_FILE: {
            if (args.size() >= 2) {
                std::ofstream f(args[0].s, std::ios::binary | std::ios::app);
                if (f) f.write(args[1].s.data(), args[1].s.size());
                return VmValue(f.good());
            }
            break;
        }
        case NATIVE_FILE_EXISTS: {
            if (args.size() >= 1) {
                std::ifstream f(args[0].s);
                return VmValue(f.good());
            }
            break;
        }
        case NATIVE_DELETE_FILE: {
            if (args.size() >= 1) {
                return VmValue(std::remove(args[0].s.c_str()) == 0);
            }
            break;
        }
        case NATIVE_FILE_SIZE: {
            if (args.size() >= 1) {
                std::ifstream f(args[0].s, std::ios::binary | std::ios::ate);
                if (!f) return VmValue((int64_t)-1);
                return VmValue((int64_t)f.tellg());
            }
            break;
        }

                             // ═══════════════════════════════════════════════════════
                             // ★★★ ВРЕМЯ / СИСТЕМА (v1.1) ★★★
                             // ═══════════════════════════════════════════════════════
        case NATIVE_TIME: {
            auto now = std::chrono::system_clock::now();
            auto sec = std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();
            return VmValue((int64_t)sec);
        }
        case NATIVE_TIME_MS: {
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - g_startTime).count();
            return VmValue((int64_t)ms);
        }
        case NATIVE_SLEEP_MS: {
            int ms = 16;
            if (args.size() >= 1) ms = (int)args[0].toNumber();
            if (ms < 0) ms = 0;
            if (ms > 60000) ms = 60000;
            Sleep(ms);
            return VmValue();
        }
        case NATIVE_SLEEP: {
            int ms = 1000;
            if (args.size() >= 1) ms = (int)args[0].toNumber();
            if (ms < 0) ms = 0;
            if (ms > 60000) ms = 60000;
            Sleep(ms);
            return VmValue();
        }
        case NATIVE_EXIT: {
            int code = 0;
            if (args.size() >= 1) code = (int)args[0].toNumber();
            std::exit(code);
            return VmValue();
        }
        case NATIVE_ENV: {
            if (args.size() >= 1) {
                const char* v = std::getenv(args[0].s.c_str());
                VmValue r; r.type = VmType::String;
                r.s = v ? std::string(v) : std::string("");
                return r;
            }
            break;
        }
        case NATIVE_SYSTEM: {
            if (args.size() >= 1) {
                int rc = std::system(args[0].s.c_str());
                return VmValue((int64_t)rc);
            }
            break;
        }

                          // ─── UI (старые WinAPI + кастомный) ────────────────────
        case NATIVE_UI_BEGIN: {
            if (args.size() >= 3) {
                resetUi();
                registerUiClass();

                std::wstring wTitle = utf8_to_wstring(args[0].toString());
                int w = (int)args[1].i;
                int h = (int)args[2].i;
                if (w < 200) w = 400;
                if (h < 150) h = 300;

                g_ui.w = w;
                g_ui.h = h;

                int sw = GetSystemMetrics(SM_CXSCREEN);
                int sh = GetSystemMetrics(SM_CYSCREEN);
                int wx = (sw - w) / 2;
                int wy = (sh - h) / 2;

                g_ui.hwnd = CreateWindowExW(
                    0, L"MayamiCanvas", wTitle.c_str(),
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                    wx, wy, w + 16, h + 39,
                    nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

                if (!g_ui.hwnd) {
                    error("ui_begin: CreateWindow failed");
                    return VmValue(false);
                }

                ShowWindow(g_ui.hwnd, SW_SHOW);
                UpdateWindow(g_ui.hwnd);
                pumpMessages();

                return VmValue(true);
            }
            break;
        }
        case NATIVE_UI_THEME: {
            if (args.size() >= 3) {
                int r = (int)args[0].i;
                int g = (int)args[1].i;
                int b = (int)args[2].i;
                g_ui.bgColor = RGB(r, g, b);
                if (g_ui.hwnd) InvalidateRect(g_ui.hwnd, nullptr, TRUE);
                return VmValue(true);
            }
            break;
        }
        case NATIVE_UI_RECT: {
            if (args.size() >= 7) {
                UiElem e;
                e.type = UiElemType::Rect;
                e.x = (int)args[0].i;
                e.y = (int)args[1].i;
                e.w = (int)args[2].i;
                e.h = (int)args[3].i;
                e.color = RGB((int)args[4].i, (int)args[5].i, (int)args[6].i);
                g_ui.elems.push_back(e);
                if (g_ui.hwnd) InvalidateRect(g_ui.hwnd, nullptr, FALSE);
                return VmValue(true);
            }
            break;
        }
        case NATIVE_UI_RECT_ROUND: {
            if (args.size() >= 8) {
                UiElem e;
                e.type = UiElemType::RectRound;
                e.x = (int)args[0].i;
                e.y = (int)args[1].i;
                e.w = (int)args[2].i;
                e.h = (int)args[3].i;
                e.radius = (int)args[4].i;
                e.color = RGB((int)args[5].i, (int)args[6].i, (int)args[7].i);
                g_ui.elems.push_back(e);
                if (g_ui.hwnd) InvalidateRect(g_ui.hwnd, nullptr, FALSE);
                return VmValue(true);
            }
            break;
        }
        case NATIVE_UI_TEXT: {
            if (args.size() >= 6) {
                UiElem e;
                e.type = UiElemType::Text;
                e.x = (int)args[0].i;
                e.y = (int)args[1].i;
                e.text = args[2].toString();
                e.textColor = RGB((int)args[3].i, (int)args[4].i, (int)args[5].i);
                e.textSize = 16;
                g_ui.elems.push_back(e);
                if (g_ui.hwnd) InvalidateRect(g_ui.hwnd, nullptr, FALSE);
                return VmValue(true);
            }
            break;
        }
        case NATIVE_UI_TEXT_SIZE: {
            if (args.size() >= 7) {
                UiElem e;
                e.type = UiElemType::Text;
                e.x = (int)args[0].i;
                e.y = (int)args[1].i;
                e.text = args[2].toString();
                e.textSize = (int)args[3].i;
                e.textColor = RGB((int)args[4].i, (int)args[5].i, (int)args[6].i);
                g_ui.elems.push_back(e);
                if (g_ui.hwnd) InvalidateRect(g_ui.hwnd, nullptr, FALSE);
                return VmValue(true);
            }
            break;
        }
        case NATIVE_UI_BUTTON_C: {
            if (args.size() >= 9) {
                UiElem e;
                e.type = UiElemType::Button;
                e.x = (int)args[0].i;
                e.y = (int)args[1].i;
                e.w = (int)args[2].i;
                e.h = (int)args[3].i;
                e.text = args[4].toString();
                e.id = args[5].toString();
                e.color = RGB((int)args[6].i, (int)args[7].i, (int)args[8].i);
                e.textColor = RGB(255, 255, 255);
                e.textSize = 16;
                e.radius = 6;
                g_ui.elems.push_back(e);
                if (g_ui.hwnd) InvalidateRect(g_ui.hwnd, nullptr, FALSE);
                return VmValue(true);
            }
            break;
        }
        case NATIVE_UI_CLEAR: {
            g_ui.elems.clear();
            if (g_ui.hwnd) InvalidateRect(g_ui.hwnd, nullptr, TRUE);
            return VmValue(true);
        }
        case NATIVE_UI_WIDTH:  return VmValue((int64_t)g_ui.w);
        case NATIVE_UI_HEIGHT: return VmValue((int64_t)g_ui.h);
        case NATIVE_UI_MOUSE_X: return VmValue((int64_t)g_ui.mouseX);
        case NATIVE_UI_MOUSE_Y: return VmValue((int64_t)g_ui.mouseY);
        case NATIVE_UI_RUNNING: {
            if (!g_ui.hwnd) return VmValue(false);
            if (!IsWindow(g_ui.hwnd)) {
                g_ui.closed = true;
                return VmValue(false);
            }
            pumpMessages();
            return VmValue(!g_ui.closed);
        }
        case NATIVE_UI_POLL: {
            pumpMessages();
            std::string cid = g_ui.clickedId;
            g_ui.clickedId.clear();
            VmValue r; r.type = VmType::String; r.s = cid;
            return r;
        }
        case NATIVE_UI_SLEEP: {
            int ms = 16;
            if (args.size() >= 1) ms = (int)args[0].i;
            if (ms < 1) ms = 1;
            if (ms > 1000) ms = 1000;
            Sleep(ms);
            return VmValue();
        }
        case NATIVE_UI_REFRESH: {
            if (g_ui.hwnd && IsWindow(g_ui.hwnd)) {
                InvalidateRect(g_ui.hwnd, nullptr, TRUE);
                UpdateWindow(g_ui.hwnd);
            }
            pumpMessages();
            return VmValue(true);
        }
        case NATIVE_UI_SHOW: {
            if (g_ui.hwnd && IsWindow(g_ui.hwnd)) {
                ShowWindow(g_ui.hwnd, SW_SHOW);
                UpdateWindow(g_ui.hwnd);
                pumpMessages();
            }
            return VmValue(true);
        }
        case NATIVE_UI_LABEL:
        case NATIVE_UI_BUTTON:
        case NATIVE_UI_INPUT:
        case NATIVE_UI_CHECKBOX:
        case NATIVE_UI_CLICKED:
        case NATIVE_UI_GETVAL:
        case NATIVE_UI_TEXTBOX:
        case NATIVE_UI_SET_LABEL:
        case NATIVE_UI_SET_TEXT:
        case NATIVE_UI_REMOVE:
            return VmValue();

        default:
            break;
        }

        error("Unknown native function: " + std::to_string(id));
        return VmValue();
    }

    std::string VM::formatValue(const VmValue& v) {
        return v.toString();
    }
    // ─── Один шаг ───────────────────────────────────────────
    bool VM::executeOne() {
        if (m_ip >= m_code.size()) return false;

        if (m_stack.size() > 100000) { error("Stack overflow");  return false; }
        if (m_vars.size() > 100000) { error("Vars overflow");   return false; }
        if (m_frames.size() > 10000) { error("Frames overflow"); return false; }

        size_t curIP = m_ip;
        uint8_t opByte = readByte();
        OpCode op = (OpCode)opByte;

        if (m_debug && m_debugOut) {
            std::ostringstream ss;
            ss << "[VM] #" << (m_stepCount++)
                << " ip=" << curIP
                << " op=0x";
            char hexbuf[8];
            std::snprintf(hexbuf, sizeof(hexbuf), "%02X", opByte);
            ss << hexbuf
                << " (" << OpCodeName(op) << ")"
                << " stack=" << m_stack.size()
                << " vars=" << m_vars.size()
                << " frames=" << m_frames.size();
            m_debugOut(ss.str());
        }

        switch (op) {
        case OpCode::NOP: break;

        case OpCode::PUSH_NULL:  push(VmValue()); break;
        case OpCode::PUSH_TRUE:  push(VmValue(true)); break;
        case OpCode::PUSH_FALSE: push(VmValue(false)); break;

        case OpCode::PUSH_INT: { int64_t v = readI64(); push(VmValue(v)); break; }
        case OpCode::PUSH_FLOAT: { double v = readF64(); push(VmValue(v)); break; }
        case OpCode::PUSH_STR: {
            std::string s = readString();
            VmValue v; v.type = VmType::String; v.s = s; push(v); break;
        }
        case OpCode::PUSH_CHAR: {
            char c = (char)readByte();
            VmValue v; v.type = VmType::Char; v.s = std::string(1, c); push(v); break;
        }

        case OpCode::POP:
            if (!m_stack.empty()) m_stack.pop_back();
            break;

        case OpCode::DUP:
            if (!m_stack.empty()) push(m_stack.back());
            break;

        case OpCode::SWAP:
            if (m_stack.size() >= 2)
                std::swap(m_stack[m_stack.size() - 1], m_stack[m_stack.size() - 2]);
            break;

        case OpCode::LOAD_VAR: {
            uint32_t slot = readU32();
            uint32_t base = m_frames.empty() ? 0 : m_frames.back().varBase;
            if (slot > 100000) { push(VmValue()); break; }
            uint32_t real = base + slot;
            if (real >= m_vars.size()) push(VmValue());
            else push(m_vars[real]);
            break;
        }

        case OpCode::STORE_VAR: {
            uint32_t slot = readU32();
            uint32_t base = m_frames.empty() ? 0 : m_frames.back().varBase;
            if (slot > 100000) { pop(); break; }
            VmValue v = pop();
            uint32_t real = base + slot;
            if (real >= m_vars.size()) {
                if (real + 1 > 100000) break;
                m_vars.resize(real + 1);
            }
            m_vars[real] = v;
            break;
        }

        case OpCode::LOAD_LOCAL:
        case OpCode::STORE_LOCAL:
        case OpCode::LOAD_GLOBAL:
        case OpCode::STORE_GLOBAL: {
            uint32_t idx = readU32();
            (void)idx;
            error("LOAD_LOCAL/STORE_LOCAL not implemented");
            return false;
        }

        case OpCode::ADD: {
            VmValue b = pop(); VmValue a = pop();
            if (a.type == VmType::String || b.type == VmType::String) {
                VmValue r; r.type = VmType::String;
                r.s = a.toString() + b.toString();
                push(r); break;
            }
            if (a.type == VmType::Int && b.type == VmType::Int) {
                push(VmValue((int64_t)(a.i + b.i))); break;
            }
            push(VmValue((double)(a.toNumber() + b.toNumber())));
            break;
        }
        case OpCode::SUB: {
            VmValue b = pop(); VmValue a = pop();
            if (a.type == VmType::Int && b.type == VmType::Int)
                push(VmValue((int64_t)(a.i - b.i)));
            else push(VmValue((double)(a.toNumber() - b.toNumber())));
            break;
        }
        case OpCode::MUL: {
            VmValue b = pop(); VmValue a = pop();
            if (a.type == VmType::Int && b.type == VmType::Int)
                push(VmValue((int64_t)(a.i * b.i)));
            else push(VmValue((double)(a.toNumber() * b.toNumber())));
            break;
        }
        case OpCode::DIV: {
            VmValue b = pop(); VmValue a = pop();
            if (b.toNumber() == 0.0) { error("Division by zero"); push(VmValue()); break; }
            if (a.type == VmType::Int && b.type == VmType::Int)
                push(VmValue((int64_t)(a.i / b.i)));
            else push(VmValue((double)(a.toNumber() / b.toNumber())));
            break;
        }
        case OpCode::MOD: {
            VmValue b = pop(); VmValue a = pop();
            if (b.i == 0) { error("Modulo by zero"); push(VmValue()); break; }
            push(VmValue((int64_t)(a.i % b.i)));
            break;
        }
        case OpCode::NEG: {
            VmValue a = pop();
            if (a.type == VmType::Int) push(VmValue((int64_t)(-a.i)));
            else push(VmValue((double)(-a.toNumber())));
            break;
        }

        case OpCode::EQ: { VmValue b = pop(); VmValue a = pop(); push(VmValue(a.toString() == b.toString())); break; }
        case OpCode::NEQ: { VmValue b = pop(); VmValue a = pop(); push(VmValue(a.toString() != b.toString())); break; }
        case OpCode::LT: { VmValue b = pop(); VmValue a = pop(); push(VmValue(a.toNumber() < b.toNumber()));  break; }
        case OpCode::GT: { VmValue b = pop(); VmValue a = pop(); push(VmValue(a.toNumber() > b.toNumber()));  break; }
        case OpCode::LE: { VmValue b = pop(); VmValue a = pop(); push(VmValue(a.toNumber() <= b.toNumber())); break; }
        case OpCode::GE: { VmValue b = pop(); VmValue a = pop(); push(VmValue(a.toNumber() >= b.toNumber())); break; }

        case OpCode::AND: { VmValue b = pop(); VmValue a = pop(); push(VmValue(a.b && b.b)); break; }
        case OpCode::OR: { VmValue b = pop(); VmValue a = pop(); push(VmValue(a.b || b.b)); break; }
        case OpCode::NOT: { VmValue a = pop(); push(VmValue(!a.b)); break; }
        case OpCode::BIT_AND: { VmValue b = pop(); VmValue a = pop(); push(VmValue((int64_t)(a.i & b.i))); break; }
        case OpCode::BIT_OR: { VmValue b = pop(); VmValue a = pop(); push(VmValue((int64_t)(a.i | b.i))); break; }
        case OpCode::BIT_XOR: { VmValue b = pop(); VmValue a = pop(); push(VmValue((int64_t)(a.i ^ b.i))); break; }
        case OpCode::BIT_NOT: { VmValue a = pop(); push(VmValue((int64_t)(~a.i))); break; }
        case OpCode::SHL: { VmValue b = pop(); VmValue a = pop(); push(VmValue((int64_t)(a.i << b.i))); break; }
        case OpCode::SHR: { VmValue b = pop(); VmValue a = pop(); push(VmValue((int64_t)(a.i >> b.i))); break; }

        case OpCode::JUMP: {
            uint32_t addr = readU32();
            if (addr > m_code.size()) { error("JUMP out of bounds"); return false; }
            m_ip = addr;
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            uint32_t addr = readU32();
            if (addr > m_code.size()) { error("JUMP out of bounds"); return false; }
            VmValue cond = pop();
            if (!cond.b) m_ip = addr;
            break;
        }
        case OpCode::JUMP_IF_TRUE: {
            uint32_t addr = readU32();
            if (addr > m_code.size()) { error("JUMP out of bounds"); return false; }
            VmValue cond = pop();
            if (cond.b) m_ip = addr;
            break;
        }

        case OpCode::CALL: {
            uint32_t funcIdx = readU32();
            uint8_t argc = readByte();
            if (funcIdx >= m_functions.size()) {
                error("CALL: invalid funcIdx " + std::to_string(funcIdx));
                return false;
            }
            const BytecodeFunction& fn = m_functions[funcIdx];
            std::vector<VmValue> args(argc);
            for (int k = argc - 1; k >= 0; --k) args[k] = pop();

            Frame frame;
            frame.returnIP = (uint32_t)m_ip;
            frame.varBase = (uint32_t)m_vars.size();
            frame.funcIdx = funcIdx;
            m_frames.push_back(frame);

            m_vars.resize(frame.varBase + fn.localCount);
            for (uint32_t k = 0; k < argc && k < fn.localCount; ++k) {
                m_vars[frame.varBase + k] = args[k];
            }

            if (fn.codeOffset > m_code.size()) {
                error("CALL: invalid codeOffset");
                return false;
            }
            m_ip = fn.codeOffset;
            break;
        }

        case OpCode::RET: {
            VmValue ret;
            if (!m_stack.empty()) ret = pop();
            if (m_frames.empty()) { push(ret); return false; }
            Frame frame = m_frames.back();
            m_frames.pop_back();
            if (m_vars.size() > frame.varBase) m_vars.resize(frame.varBase);
            if (frame.returnIP > m_code.size()) {
                error("RET: invalid returnIP");
                return false;
            }
            m_ip = frame.returnIP;
            push(ret);
            break;
        }

        case OpCode::CALL_NATIVE: {
            uint32_t id = readU32();
            uint8_t argc = readByte();
            std::vector<VmValue> args;
            args.resize(argc);
            for (int k = argc - 1; k >= 0; --k) args[k] = pop();
            VmValue result = callNative(id, args);
            push(result);
            break;
        }

        case OpCode::NEW_ARRAY: {
            uint32_t sz = readU32();
            if (sz > 10000000) { error("Array too big"); return false; }
            VmValue v;
            v.type = VmType::Array;
            v.array = std::make_shared<std::vector<VmValue>>(sz);
            push(v);
            break;
        }
        case OpCode::ARRAY_GET: {
            VmValue idx = pop(); VmValue arr = pop();
            if (arr.type != VmType::Array || !arr.array) { push(VmValue()); break; }
            long long i = idx.i;
            if (i < 0 || i >= (long long)arr.array->size()) {
                error("Array index OOB"); push(VmValue()); break;
            }
            push((*arr.array)[(size_t)i]);
            break;
        }
        case OpCode::ARRAY_SET: {
            VmValue val = pop(); VmValue idx = pop(); VmValue arr = pop();
            if (arr.type != VmType::Array || !arr.array) break;
            long long i = idx.i;
            if (i < 0 || i >= (long long)arr.array->size()) break;
            (*arr.array)[(size_t)i] = val;
            push(arr);
            break;
        }
        case OpCode::ARRAY_LEN: {
            VmValue arr = pop();
            if (arr.type != VmType::Array || !arr.array) { push(VmValue((int64_t)0)); break; }
            push(VmValue((int64_t)arr.array->size()));
            break;
        }

        case OpCode::NEW_STRUCT: {
            uint32_t sid = readU32();
            VmValue v;
            v.type = VmType::Struct;
            v.structId = sid;
            v.fields = std::make_shared<std::unordered_map<uint32_t, VmValue>>();
            push(v);
            break;
        }
        case OpCode::FIELD_GET: {
            uint32_t fid = readU32();
            VmValue obj = pop();
            if (obj.type != VmType::Struct || !obj.fields) { push(VmValue()); break; }
            auto it = obj.fields->find(fid);
            if (it == obj.fields->end()) { push(VmValue()); break; }
            push(it->second);
            break;
        }
        case OpCode::FIELD_SET: {
            uint32_t fid = readU32();
            VmValue val = pop(); VmValue obj = pop();
            if (obj.type == VmType::Struct && obj.fields)
                (*obj.fields)[fid] = val;
            push(obj);
            break;
        }

        case OpCode::ITER_NEW:
        case OpCode::ITER_NEXT:
        case OpCode::ITER_GET:
            error("ITER_* not yet implemented");
            return false;

        case OpCode::HALT:
            return false;

        default:
            error("Unknown opcode: 0x" + std::to_string(opByte));
            return false;
        }

        return true;
    }

    // ─── Main loop ──────────────────────────────────────────
    bool VM::run() {
        m_ip = m_entryPoint;
        m_stepCount = 0;

        Frame root;
        root.returnIP = 0;
        root.varBase = 0;
        root.funcIdx = 0;
        m_frames.push_back(root);

        while (true) {
            try {
                if (!executeOne()) break;
            }
            catch (VmReturnSignal&) { break; }
            catch (VmRuntimeError& e) { error(e.message); break; }
            catch (const std::out_of_range& e) {
                std::ostringstream ss;
                ss << "OUT_OF_RANGE at ip=" << m_ip
                    << " stack=" << m_stack.size()
                    << " vars=" << m_vars.size()
                    << " frames=" << m_frames.size()
                    << " (" << e.what() << ")";
                error(ss.str());
                break;
            }
            catch (const std::exception& e) {
                std::ostringstream ss;
                ss << "STD_EXCEPTION at ip=" << m_ip << " (" << e.what() << ")";
                error(ss.str());
                break;
            }
            catch (...) {
                error("UNKNOWN exception");
                break;
            }
        }

        return !hasErrors();
    }

} // namespace mayami