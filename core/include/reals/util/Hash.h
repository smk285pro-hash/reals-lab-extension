#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace reals::util {

// Streaming xxHash64 state for fast checksumming of sample files & memory buffers.
class XxHash64 {
public:
    explicit XxHash64(uint64_t seed = 0);

    void reset(uint64_t seed = 0);
    void update(const void* input, size_t length);
    void update(std::string_view sv);

    [[nodiscard]] uint64_t digest() const;
    [[nodiscard]] std::string digestHex() const;

private:
    uint64_t m_v[4]{};
    uint64_t m_totalLen = 0;
    uint8_t m_mem[32]{};
    size_t m_memSize = 0;
    uint64_t m_seed = 0;

    static uint64_t round(uint64_t acc, uint64_t input);
    static uint64_t mergeRound(uint64_t acc, uint64_t val);
};

// Streaming SHA-256 state for cryptographic/model verification.
class Sha256 {
public:
    Sha256();

    void reset();
    void update(const void* data, size_t length);
    void update(std::string_view sv);

    [[nodiscard]] std::vector<uint8_t> digest() const;
    [[nodiscard]] std::string digestHex() const;

private:
    void transform(const uint8_t block[64]);

    uint32_t m_state[8]{};
    uint64_t m_bitCount = 0;
    uint8_t m_buffer[64]{};
};

namespace Hash {

// Compute xxHash64 over memory buffer.
[[nodiscard]] uint64_t xx64(const void* data, size_t size, uint64_t seed = 0);
[[nodiscard]] uint64_t xx64(std::string_view text, uint64_t seed = 0);
[[nodiscard]] std::string xx64Hex(const void* data, size_t size, uint64_t seed = 0);
[[nodiscard]] std::string xx64Hex(std::string_view text, uint64_t seed = 0);

// Compute xxHash64 over a file on disk (streams in 64KB chunks).
// If maxBytes > 0, hashes only up to maxBytes.
[[nodiscard]] std::string fileXx64Hex(const std::string& path, size_t maxBytes = 0, uint64_t seed = 0);

// Compute SHA-256 over memory buffer.
[[nodiscard]] std::string sha256Hex(const void* data, size_t size);
[[nodiscard]] std::string sha256Hex(std::string_view text);

// Compute SHA-256 over a file on disk.
[[nodiscard]] std::string fileSha256Hex(const std::string& path);

// Fast file change check:
// Returns true if cached size and modified time match the file on disk.
// If size and mtime match, returns true immediately without disk I/O.
// If size or mtime differs, calculates the xx64 hash and returns whether it matches cachedHash.
[[nodiscard]] bool isFileUnchanged(const std::string& path,
                                   uint64_t cachedSize,
                                   int64_t cachedModTime,
                                   const std::string& cachedHash);

} // namespace Hash

// Top-level convenience wrappers matching standard naming conventions
[[nodiscard]] inline uint64_t xxhash64(std::string_view text, uint64_t seed = 0) {
    return Hash::xx64(text, seed);
}

[[nodiscard]] inline uint64_t xxhash64(const void* data, size_t size, uint64_t seed = 0) {
    return Hash::xx64(data, size, seed);
}

[[nodiscard]] inline std::string sha256(std::string_view text) {
    return Hash::sha256Hex(text);
}

[[nodiscard]] inline std::string sha256(const void* data, size_t size) {
    return Hash::sha256Hex(data, size);
}

[[nodiscard]] inline std::string sha256File(const std::string& path) {
    return Hash::fileSha256Hex(path);
}

[[nodiscard]] inline std::string xxhash64File(const std::string& path, size_t maxBytes = 0, uint64_t seed = 0) {
    return Hash::fileXx64Hex(path, maxBytes, seed);
}

} // namespace reals::util
