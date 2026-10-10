// src/main_vm.cpp — оконная версия для .exe
#include "vm/VM.h"
#include "vm/SelfLoader.h"

#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace mayami;

// ============================================================
// Чтение файла
// ============================================================
static std::vector<uint8_t> readFile(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { ok = false; return {}; }
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf((size_t)size);
    if (!f.read((char*)buf.data(), size)) { ok = false; return {}; }
    ok = true;
    return buf;
}

static std::string getSelfPath() {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (n > 0) return std::string(buf, n);
    return "";
}

// ============================================================
// Оконная процедура
// ============================================================
struct WindowData {
    std::string text;
    HWND hEdit = nullptr;
    HFONT hFont = nullptr;
};

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowData* wd = (WindowData*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        wd = (WindowData*)cs->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)wd);

        // Создаём edit control (многострочный, readonly)
        wd->hEdit = CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT", wd->text.c_str(),
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
            0, 0, 100, 100,
            hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

        // Моноширинный шрифт
        wd->hFont = CreateFontA(
            16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

        SendMessageA(wd->hEdit, WM_SETFONT, (WPARAM)wd->hFont, TRUE);
        return 0;
    }

    case WM_SIZE: {
        if (wd && wd->hEdit) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            SetWindowPos(wd->hEdit, nullptr, 0, 0, rc.right, rc.bottom,
                SWP_NOZORDER);
        }
        return 0;
    }

    case WM_SETFOCUS:
        if (wd && wd->hEdit) SetFocus(wd->hEdit);
        return 0;

    case WM_DESTROY:
        if (wd && wd->hFont) DeleteObject(wd->hFont);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ============================================================
// Показать окно с текстом
// ============================================================
static int showOutputWindow(const std::string& title, const std::string& text) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Регистрируем класс
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "MayamiOutputWindow";

    RegisterClassExA(&wc);

    // Данные окна
    WindowData wd;
    wd.text = text;

    // Размеры: 800x600, по центру
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 800, wh = 600;
    int wx = (sw - ww) / 2;
    int wy = (sh - wh) / 2;

    HWND hwnd = CreateWindowExA(
        0,
        "MayamiOutputWindow",
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        wx, wy, ww, wh,
        nullptr, nullptr, hInst, &wd);

    if (!hwnd) return 1;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Цикл сообщений
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// ============================================================
// main
// ============================================================
int main(int argc, char** argv) {
    std::vector<uint8_t> data;
    bool ok = false;
    std::string title = "Mayami Program";

    if (argc >= 2) {
        // Запуск .myc из командной строки
        data = readFile(argv[1], ok);
        if (!ok) {
            std::string err = "Не могу открыть файл:\n" + std::string(argv[1]);
            return showOutputWindow("Mayami — Ошибка", err);
        }
        title = "Mayami — " + std::string(argv[1]);
    }
    else {
        // Читаем байткод из себя (собранный .exe)
        std::string self = getSelfPath();
        if (self.empty() || !loadBytecodeFromSelf(self, data)) {
            // Не нашли байткод — показываем справку
            std::string help =
                "mayamivm — виртуальная машина Mayami\n"
                "\n"
                "Использование:\n"
                "  mayamivm <file.myc>\n"
                "\n"
                "Или запустите .exe, собранный через mayamic.exe\n"
                "(в нём уже встроен байткод)\n";
            return showOutputWindow("Mayami VM", help);
        }

        // Заголовок = имя exe
        size_t pos = self.find_last_of("\\/");
        std::string name = (pos == std::string::npos) ? self : self.substr(pos + 1);
        title = "Mayami — " + name;
    }

    // Запускаем VM
    VM vm;

    // ★ Собираем ВЕСЬ вывод программы в буфер
    std::ostringstream outBuf;
    vm.setOutputCallback([&outBuf](const std::string& s) {
        outBuf << s << "\r\n";   // \r\n — для корректного отображения в EDIT
        });

    // Загружаем
    if (!vm.load(data)) {
        std::string err = "Ошибка загрузки байткода:\n\n";
        for (auto& e : vm.errors()) err += "  " + e + "\n";
        return showOutputWindow(title + " — Ошибка", err);
    }

    // Запускаем
    vm.run();

    // Формируем финальный текст
    std::string text = outBuf.str();

    if (vm.hasErrors()) {
        text += "\r\n--- Ошибки ---\r\n";
        for (auto& e : vm.errors()) text += "  " + e + "\r\n";
    }

    if (text.empty()) {
        text = "(программа не вывела ничего)\r\n";
    }

    // Показываем окно
    return showOutputWindow(title, text);
}