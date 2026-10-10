// src/core/FileSystem.cpp
#include "FileSystem.h"

#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace mayami {

    bool FileSystem::exists(const std::string& path) {
        std::error_code ec;
        return fs::exists(path, ec);
    }

    bool FileSystem::isDirectory(const std::string& path) {
        std::error_code ec;
        return fs::is_directory(path, ec);
    }

    std::string FileSystem::readFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return "";
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    bool FileSystem::writeFile(const std::string& path, const std::string& content) {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f << content;
        return true;
    }

    bool FileSystem::createDirectory(const std::string& path) {
        std::error_code ec;
        return fs::create_directories(path, ec);
    }

    bool FileSystem::createFile(const std::string& path, const std::string& content) {
        if (exists(path)) return false;
        return writeFile(path, content);
    }

    bool FileSystem::remove(const std::string& path) {
        std::error_code ec;
        return fs::remove_all(path, ec) > 0;
    }

    bool FileSystem::rename(const std::string& oldPath, const std::string& newPath) {
        std::error_code ec;
        fs::rename(oldPath, newPath, ec);
        return !ec;
    }

    FileEntry FileSystem::scanDirectory(const std::string& path, int depth) {
        FileEntry root;
        root.name = getFileName(path);
        root.path = path;
        root.isDirectory = true;

        if (depth > 4) return root;   // ограничение глубины

        std::error_code ec;
        if (!fs::exists(path, ec) || !fs::is_directory(path, ec))
            return root;

        std::vector<fs::directory_entry> entries;
        for (auto& entry : fs::directory_iterator(path, ec)) {
            entries.push_back(entry);
        }

        // Сортировка: папки → файлы, потом по имени
        std::sort(entries.begin(), entries.end(),
            [](const fs::directory_entry& a, const fs::directory_entry& b) {
                bool aDir = a.is_directory();
                bool bDir = b.is_directory();
                if (aDir != bDir) return aDir > bDir;
                return a.path().filename().string() < b.path().filename().string();
            });

        for (auto& e : entries) {
            FileEntry child;
            child.name = e.path().filename().string();
            child.path = e.path().string();
            child.isDirectory = e.is_directory();

            // Пропускаем системные
            if (child.name == ".git" || child.name == "out" ||
                child.name == "build" || child.name == ".vs" ||
                child.name == ".vscode" || child.name == "bin") {
                continue;
            }

            if (child.isDirectory && depth < 3) {
                child = scanDirectory(child.path, depth + 1);
            }

            root.children.push_back(std::move(child));
        }

        return root;
    }

    std::string FileSystem::getFileName(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        return (pos == std::string::npos) ? path : path.substr(pos + 1);
    }

    std::string FileSystem::getParentPath(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        return (pos == std::string::npos) ? "" : path.substr(0, pos);
    }

    std::string FileSystem::getExtension(const std::string& path) {
        size_t pos = path.find_last_of('.');
        return (pos == std::string::npos) ? "" : path.substr(pos);
    }

    std::string FileSystem::joinPath(const std::string& a, const std::string& b) {
        if (a.empty()) return b;
        char last = a.back();
        if (last == '/' || last == '\\') return a + b;
        return a + "/" + b;
    }

} // namespace mayami