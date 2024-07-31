#pragma once

#include <filesystem>
#include <vector>

#define MOVABLE_ONLY(ClassName)            \
    ClassName(const ClassName&) = delete;  \
    ClassName& operator=(const ClassName&) = delete; \
    ClassName(ClassName&&) noexcept = default;       \
    ClassName& operator=(ClassName&&) noexcept = default;

std::vector<char> readFile(std::filesystem::path fileName, bool isBinary = false);
