// src/vm/SelfLoader.h
#pragma once
#include "Bytecode.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>

namespace mayami {

    // "MYMI" в little-endian
    constexpr uint32_t TRAILER_MAGIC = 0x4D594D49;
    constexpr uint32_t TRAILER_SIZE = 12;   // magic(4) + size(4) + reserved(4)

    // ============================================================
    // Дописать байткод в конец exe + трейлер
    // ============================================================
    inline bool appendBytecodeToExe(const std::string& exePath,
        const std::vector<uint8_t>& bytecode) {
        FILE* f = std::fopen(exePath.c_str(), "ab");
        if (!f) return false;

        // Байткод
        if (!bytecode.empty()) {
            std::fwrite(bytecode.data(), 1, bytecode.size(), f);
        }

        // Трейлер
        uint32_t magic = TRAILER_MAGIC;
        uint32_t size = (uint32_t)bytecode.size();
        uint32_t rsvd = 0;

        std::fwrite(&magic, 4, 1, f);
        std::fwrite(&size, 4, 1, f);
        std::fwrite(&rsvd, 4, 1, f);

        std::fclose(f);
        return true;
    }

    // ============================================================
    // Прочитать байткод из хвоста exe
    // ============================================================
    inline bool loadBytecodeFromSelf(const std::string& exePath,
        std::vector<uint8_t>& out) {
        FILE* f = std::fopen(exePath.c_str(), "rb");
        if (!f) return false;

        std::fseek(f, 0, SEEK_END);
        long long fileSize = std::ftell(f);

        if (fileSize < (long long)TRAILER_SIZE) {
            std::fclose(f);
            return false;
        }

        // Читаем трейлер из конца
        std::fseek(f, (long)(fileSize - TRAILER_SIZE), SEEK_SET);

        uint32_t magic = 0, bcSize = 0, rsvd = 0;
        std::fread(&magic, 4, 1, f);
        std::fread(&bcSize, 4, 1, f);
        std::fread(&rsvd, 4, 1, f);

        if (magic != TRAILER_MAGIC) {
            std::fclose(f);
            return false;
        }

        long long bcStart = fileSize - TRAILER_SIZE - bcSize;
        if (bcStart < 0) {
            std::fclose(f);
            return false;
        }

        out.resize(bcSize);
        if (bcSize > 0) {
            std::fseek(f, (long)bcStart, SEEK_SET);
            std::fread(out.data(), 1, bcSize, f);
        }

        std::fclose(f);
        return true;
    }

} // namespace mayami