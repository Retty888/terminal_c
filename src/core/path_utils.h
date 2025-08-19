#pragma once

#include <filesystem>

namespace Core {

// Returns the directory containing the current executable.
std::filesystem::path executable_dir();

// Resolves a path relative to the executable directory.
std::filesystem::path path_from_executable(const std::filesystem::path &relative);

} // namespace Core

