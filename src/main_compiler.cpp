// src/main_compiler.cpp
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/BytecodeGen.h"
#include "vm/SelfLoader.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif

using namespace mayami;

// ============================================================
// Хелперы
// ============================================================
static std::string readTextFile(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { ok = false; return {}; }
    std::string s((std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    ok = true;
    return s;
}

static bool writeBinary(const std::string& path,
    const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    if (!data.empty())
        f.write((const char*)data.data(), (std::streamsize)data.size());
    return (bool)f;
}

static std::string trimEol(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' ||
        s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    return s;
}

static std::string getSelfDir(const char* argv0) {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (n > 0) {
        std::string path(buf, n);
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos)
            return path.substr(0, pos);
    }
#endif
    std::string a = argv0;
    size_t pos = a.find_last_of("\\/");
    if (pos != std::string::npos) return a.substr(0, pos);
    return ".";
}

static bool fileExists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES &&
        !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

static bool makeDirRecursive(const std::string& dir) {
    if (dir.empty()) return true;
    DWORD attrs = GetFileAttributesA(dir.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES &&
        (attrs & FILE_ATTRIBUTE_DIRECTORY))
        return true;

    std::string cmd = "cmd /c md \"" + dir + "\" >nul 2>nul";
    int rc = std::system(cmd.c_str());
    if (rc != 0) return false;

    attrs = GetFileAttributesA(dir.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES &&
        (attrs & FILE_ATTRIBUTE_DIRECTORY));
}

static std::string parentDir(const std::string& filePath) {
    size_t pos = filePath.find_last_of("\\/");
    if (pos == std::string::npos) return "";
    return filePath.substr(0, pos);
}

static std::string findVMExe(const char* argv0) {
    std::string dir = getSelfDir(argv0);

    std::vector<std::string> candidates = {
        dir + "\\mayamivm.exe",
        dir + "\\..\\bin\\mayamivm.exe",
        dir + "\\mayamivm",
        "mayamivm.exe",
    };

    for (auto& p : candidates) {
        if (fileExists(p)) {
            char full[MAX_PATH];
            DWORD n = GetFullPathNameA(p.c_str(), MAX_PATH, full, nullptr);
            if (n > 0 && n < MAX_PATH) return std::string(full, n);
            return p;
        }
    }
    return "";
}

// ============================================================
// main
// ============================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "mayamic — компилятор Mayami\n";
        std::cerr << "Использование:\n";
        std::cerr << "  mayamic <file.mayami> [-o out.exe] [--myc-only]\n";
        return 1;
    }

    std::string inPath = argv[1];
    std::string outPath;
    bool mycOnly = false;

    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            outPath = argv[++i];
        }
        else if (a == "--myc-only") {
            mycOnly = true;
        }
    }

    auto unquote = [](std::string s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            return s.substr(1, s.size() - 2);
        return s;
        };
    inPath = unquote(trimEol(inPath));
    outPath = unquote(trimEol(outPath));

    // ─── Имя по умолчанию ──────────────────────────────
    if (outPath.empty()) {
        std::string base = inPath;
        size_t slash = base.find_last_of("\\/");
        if (slash != std::string::npos) base = base.substr(slash + 1);
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos) base = base.substr(0, dot);

        outPath = "build/" + base + (mycOnly ? ".myc" : ".exe");
    }

    for (auto& c : outPath) if (c == '/') c = '\\';
    for (auto& c : inPath)  if (c == '/') c = '\\';

    // ─── Создаём папку вывода ──────────────────────────
    std::string outDir = parentDir(outPath);
    if (!outDir.empty()) {
        if (!makeDirRecursive(outDir)) {
            std::cerr << "Ошибка: не могу создать папку '" << outDir << "'\n";
            return 1;
        }
    }

    // ─── Читаем исходник ────────────────────────────────
    bool ok = false;
    std::string source = readTextFile(inPath, ok);
    if (!ok) {
        std::cerr << "Ошибка: не могу открыть '" << inPath << "'\n";
        std::cerr << "(проверь что файл существует)\n";
        return 1;
    }

    // ─── Lexer ──────────────────────────────────────────
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (lexer.hasErrors()) {
        std::cerr << "Ошибки лексинга:\n";
        for (auto& e : lexer.errors()) std::cerr << "  " << e << "\n";
        return 1;
    }

    // ─── Parser ─────────────────────────────────────────
    Parser parser(tokens);
    std::unique_ptr<Program> program = parser.parse();
    if (!program || parser.hasErrors()) {
        std::cerr << "Ошибки парсинга:\n";
        for (auto& e : parser.errors()) std::cerr << "  " << e << "\n";
        return 1;
    }

    // ─── BytecodeGen ────────────────────────────────────
    BytecodeGen gen;

    // ★ Папка исходного файла — для поиска import
    {
        size_t pos = inPath.find_last_of("\\/");
        std::string dir = (pos != std::string::npos)
            ? inPath.substr(0, pos)
            : ".";
        gen.setSourceDir(dir);
        std::cout << "[INFO] Source dir: " << dir << "\n";
    }

    std::vector<uint8_t> bytecode = gen.generate(program.get());
    if (gen.hasErrors()) {
        std::cerr << "Ошибки генерации байткода:\n";
        for (auto& e : gen.errors()) std::cerr << "  " << e << "\n";
        return 1;
    }

    std::cout << "[OK] Байткод: " << bytecode.size() << " байт\n";

    // ─── Только .myc ────────────────────────────────────
    if (mycOnly) {
        if (!writeBinary(outPath, bytecode)) {
            std::cerr << "Ошибка: не могу записать '" << outPath << "'\n";
            return 1;
        }
        std::cout << "[OK] Записан: " << outPath << "\n";
        return 0;
    }

    // ─── .exe: копируем mayamivm + дописываем байткод ──
    std::string vmExe = findVMExe(argv[0]);
    if (vmExe.empty()) {
        std::cerr << "[ERROR] Не могу найти mayamivm.exe\n";
        std::cerr << "  Искал в: " << getSelfDir(argv[0]) << "\n";
        return 1;
    }

    std::cout << "[INFO] VM: " << vmExe << "\n";

    {
        std::string cmd = "cmd /c copy /Y \"" + vmExe + "\" \"" + outPath + "\" >nul 2>nul";
        int rc = std::system(cmd.c_str());
        if (rc != 0 || !fileExists(outPath)) {
            std::cerr << "Ошибка: не могу скопировать '" << vmExe
                << "' → '" << outPath << "'\n";
            return 1;
        }
    }

    if (!appendBytecodeToExe(outPath, bytecode)) {
        std::cerr << "Ошибка: не могу дописать байткод в '" << outPath << "'\n";
        return 1;
    }

    long sz = 0;
    {
        std::ifstream f(outPath, std::ios::binary | std::ios::ate);
        if (f) sz = (long)f.tellg();
    }

    std::cout << "[OK] Собран: " << outPath
        << " (" << sz << " байт)\n";
    std::cout << "[OK] Запусти: " << outPath << "\n";

    return 0;
}