// src/core/FileSystem.h
#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace mayami {

    struct FileEntry {
        std::string name;
        std::string path;
        bool isDirectory = false;
        std::vector<FileEntry> children;
    };

    class FileSystem {
    public:
        // ─── Чтение ─────────────────────────────────────────
        static bool exists(const std::string& path);
        static bool isDirectory(const std::string& path);
        static std::string readFile(const std::string& path);
        static bool writeFile(const std::string& path, const std::string& content);

        // ─── Изменение ──────────────────────────────────────
        static bool createDirectory(const std::string& path);
        static bool createFile(const std::string& path, const std::string& content = "");
        static bool remove(const std::string& path);
        static bool rename(const std::string& oldPath, const std::string& newPath);

        // ─── Скан ───────────────────────────────────────────
        static FileEntry scanDirectory(const std::string& path, int depth = 0);

        // ─── Утилиты ────────────────────────────────────────
        static std::string getFileName(const std::string& path);
        static std::string getParentPath(const std::string& path);
        static std::string getExtension(const std::string& path);
        static std::string joinPath(const std::string& a, const std::string& b);
    };

} // namespace mayami