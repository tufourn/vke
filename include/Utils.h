#pragma once

#include <filesystem>
#include <vector>

std::vector<char> readFile(std::filesystem::path fileName, bool isBinary = false);
