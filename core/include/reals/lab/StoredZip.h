#pragma once

#include <string>
#include <vector>

namespace reals::lab {

struct ExtractedZipFile {
    std::string name;
    std::string path;
};

std::vector<ExtractedZipFile> extractStoredZip(const std::string& zipPath,
                                               const std::string& destinationDir);

} // namespace reals::lab
