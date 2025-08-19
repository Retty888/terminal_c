#include "config_path.h"

#include <array>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace {
std::filesystem::path executable_dir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer, buffer + len).parent_path();
#elif __APPLE__
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0)
        return std::filesystem::path(path).parent_path();
    return std::filesystem::current_path();
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1)
        return std::filesystem::path(std::string(result, count)).parent_path();
    return std::filesystem::current_path();
#endif
}
} // namespace

std::filesystem::path resolve_config_path(const std::filesystem::path &filename) {
    if (filename.is_absolute()) {
        return filename;
    }
    if (filename.filename() != "config.json") {
        return std::filesystem::absolute(filename);
    }
    if (const char *env_cfg = std::getenv("CANDLE_CONFIG_PATH")) {
        return std::filesystem::path(env_cfg);
    }
    auto exe_dir = executable_dir();
    std::array<std::filesystem::path, 3> candidates = {
        exe_dir / filename,
        exe_dir.parent_path() / filename,
        std::filesystem::current_path() / filename,
    };
    for (const auto &c : candidates) {
        if (std::filesystem::exists(c))
            return c;
    }
    return candidates[0];
}

