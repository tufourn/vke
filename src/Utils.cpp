#include "Utils.h"

#include <fstream>

std::vector<char> readFile(std::filesystem::path fileName, bool isBinary) {
    std::filesystem::path shaderPath = std::filesystem::current_path();
    shaderPath /= fileName;

    std::ios_base::openmode mode = std::ios::ate;
    if (isBinary) {
        mode |= std::ios::binary;
    }

    std::ifstream file(shaderPath, mode);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file");
    }

    size_t fileSize = (size_t) file.tellg();

    if (!isBinary) {
        fileSize += 1; // add extra for null char at end
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()),
              static_cast<std::streamsize>(fileSize));
    file.close();

    if (!isBinary) {
        buffer[buffer.size() - 1] = '\0';
    }

    return buffer;
}
