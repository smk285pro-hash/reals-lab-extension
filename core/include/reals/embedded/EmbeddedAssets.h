#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace reals::embedded {

struct EmbeddedFile {
    const char* relativePath;
    const unsigned char* data;
    size_t size;
};

size_t getFileCount();
const EmbeddedFile* getFiles();
const EmbeddedFile* findFile(std::string_view relativePath);
const char* getBundleHash();

bool ensureUiWebExtracted(const std::string& targetDir);

} // namespace reals::embedded
