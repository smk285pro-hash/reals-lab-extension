#include "reals/lab/StoredZip.h"

#include "reals/platform/Path.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace reals::lab {
namespace {

uint16_t readU16(std::istream& in) {
    std::array<unsigned char, 2> b{};
    if (!in.read(reinterpret_cast<char*>(b.data()), b.size()))
        throw std::runtime_error("truncated ZIP header");
    return static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
}

uint32_t readU32(std::istream& in) {
    std::array<unsigned char, 4> b{};
    if (!in.read(reinterpret_cast<char*>(b.data()), b.size()))
        throw std::runtime_error("truncated ZIP header");
    return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
}

void copyBytes(std::istream& in, std::ostream& out, uint32_t byteCount) {
    std::array<char, 64 * 1024> buffer{};
    uint32_t remaining = byteCount;
    while (remaining > 0) {
        const auto chunk = static_cast<std::streamsize>(
            std::min<uint32_t>(remaining, static_cast<uint32_t>(buffer.size())));
        if (!in.read(buffer.data(), chunk))
            throw std::runtime_error("truncated ZIP entry");
        out.write(buffer.data(), chunk);
        if (!out)
            throw std::runtime_error("failed to write extracted file");
        remaining -= static_cast<uint32_t>(chunk);
    }
}

void validateWav(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::array<char, 12> header{};
    if (!in.read(header.data(), header.size()) ||
        std::string(header.data(), 4) != "RIFF" ||
        std::string(header.data() + 8, 4) != "WAVE")
        throw std::runtime_error("invalid WAV in ZIP");
}

} // namespace

std::vector<ExtractedZipFile> extractStoredZip(const std::string& zipPath,
                                               const std::string& destinationDir) {
    std::ifstream in(platform::u8path(zipPath), std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot open ZIP file");
    const fs::path destination = platform::u8path(destinationDir);
    std::error_code ec;
    fs::create_directories(destination, ec);
    if (ec)
        throw std::runtime_error("cannot create ZIP destination");

    std::vector<ExtractedZipFile> extracted;
    for (;;) {
        const uint32_t signature = readU32(in);
        if (signature == 0x02014b50 || signature == 0x06054b50)
            break;
        if (signature != 0x04034b50)
            throw std::runtime_error("invalid ZIP local header");
        (void)readU16(in);
        const uint16_t flags = readU16(in);
        const uint16_t compression = readU16(in);
        (void)readU16(in);
        (void)readU16(in);
        (void)readU32(in);
        const uint32_t compressedSize = readU32(in);
        const uint32_t uncompressedSize = readU32(in);
        const uint16_t nameLength = readU16(in);
        const uint16_t extraLength = readU16(in);
        if ((flags & 0x0009u) != 0 || compression != 0 || compressedSize != uncompressedSize)
            throw std::runtime_error("unsupported ZIP entry encoding");
        std::string name(nameLength, '\0');
        if (!in.read(name.data(), name.size()))
            throw std::runtime_error("truncated ZIP filename");
        in.seekg(extraLength, std::ios::cur);
        if (!in || name.empty() || name.find('/') != std::string::npos ||
            name.find('\\') != std::string::npos || name == "." || name == "..")
            throw std::runtime_error("unsafe ZIP filename");
        const fs::path output = destination / platform::u8path(name);
        const fs::path part = fs::path(output.native() + L".part");
        {
            std::ofstream out(part, std::ios::binary | std::ios::trunc);
            if (!out)
                throw std::runtime_error("cannot create extracted file");
            copyBytes(in, out, compressedSize);
        }
        validateWav(part);
        fs::remove(output, ec);
        ec.clear();
        fs::rename(part, output, ec);
        if (ec)
            throw std::runtime_error("cannot finalize extracted file");
        extracted.push_back({name, platform::pathToUtf8(output)});
    }
    if (extracted.empty())
        throw std::runtime_error("ZIP contains no stems");
    return extracted;
}

} // namespace reals::lab
