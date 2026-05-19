#pragma once

#include <cstdio>
#include <cstdarg>
#include <string>

// Petit wrapper de log qui imprime sur la console serveur + fichier.
// AsaApi expose Log::GetLog()->info(...) (spdlog). Pour rester portable
// on garde un wrapper minimal.

namespace EOSLog {

    inline std::string Format(const char* fmt, va_list args) {
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        return std::string(buf);
    }

    inline void Info(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        std::string msg = Format(fmt, args);
        va_end(args);
        std::printf("[EOSBanManager][INFO ] %s\n", msg.c_str());
    }

    inline void Warn(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        std::string msg = Format(fmt, args);
        va_end(args);
        std::printf("[EOSBanManager][WARN ] %s\n", msg.c_str());
    }

    inline void Error(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        std::string msg = Format(fmt, args);
        va_end(args);
        std::fprintf(stderr, "[EOSBanManager][ERROR] %s\n", msg.c_str());
    }

} // namespace EOSLog
