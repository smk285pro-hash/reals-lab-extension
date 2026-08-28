#include "reals/util/Hash.h"
#include "reals/platform/Path.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace reals::util {

namespace {

constexpr uint64_t kPrime64_1 = 11400714785074694791ULL;
constexpr uint64_t kPrime64_2 = 14029467366897019727ULL;
constexpr uint64_t kPrime64_3 = 1609587929392839161ULL;
constexpr uint64_t kPrime64_4 = 9650029242287828579ULL;
constexpr uint64_t kPrime64_5 = 2870177450012600261ULL;

inline uint64_t read64Le(const uint8_t* p) {
    return static_cast<uint64_t>(p[0]) |
           (static_cast<uint64_t>(p[1]) << 8) |
           (static_cast<uint64_t>(p[2]) << 16) |
           (static_cast<uint64_t>(p[3]) << 24) |
           (static_cast<uint64_t>(p[4]) << 32) |
           (static_cast<uint64_t>(p[5]) << 40) |
           (static_cast<uint64_t>(p[6]) << 48) |
           (static_cast<uint64_t>(p[7]) << 56);
}

inline uint32_t read32Le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

} // namespace

// ---- XxHash64 ---------------------------------------------------------------

uint64_t XxHash64::round(uint64_t acc, uint64_t input) {
    acc += input * kPrime64_2;
    acc = std::rotl(acc, 31);
    acc *= kPrime64_1;
    return acc;
}

uint64_t XxHash64::mergeRound(uint64_t acc, uint64_t val) {
    val = round(0, val);
    acc ^= val;
    acc = acc * kPrime64_1 + kPrime64_4;
    return acc;
}

XxHash64::XxHash64(uint64_t seed) {
    reset(seed);
}

void XxHash64::reset(uint64_t seed) {
    m_seed = seed;
    m_v[0] = seed + kPrime64_1 + kPrime64_2;
    m_v[1] = seed + kPrime64_2;
    m_v[2] = seed + 0;
    m_v[3] = seed - kPrime64_1;
    m_totalLen = 0;
    m_memSize = 0;
    std::memset(m_mem, 0, sizeof(m_mem));
}

void XxHash64::update(const void* input, size_t length) {
    if (!input || length == 0)
        return;

    const auto* p = static_cast<const uint8_t*>(input);
    const auto* const bEnd = p + length;
    m_totalLen += length;

    if (m_memSize + length < 32) {
        std::memcpy(m_mem + m_memSize, p, length);
        m_memSize += length;
        return;
    }

    if (m_memSize > 0) {
        const size_t fill = 32 - m_memSize;
        std::memcpy(m_mem + m_memSize, p, fill);
        p += fill;
        m_v[0] = round(m_v[0], read64Le(m_mem));
        m_v[1] = round(m_v[1], read64Le(m_mem + 8));
        m_v[2] = round(m_v[2], read64Le(m_mem + 16));
        m_v[3] = round(m_v[3], read64Le(m_mem + 24));
        m_memSize = 0;
    }

    const auto* const limit = bEnd - 32;
    while (p <= limit) {
        m_v[0] = round(m_v[0], read64Le(p));
        m_v[1] = round(m_v[1], read64Le(p + 8));
        m_v[2] = round(m_v[2], read64Le(p + 16));
        m_v[3] = round(m_v[3], read64Le(p + 24));
        p += 32;
    }

    if (p < bEnd) {
        m_memSize = static_cast<size_t>(bEnd - p);
        std::memcpy(m_mem, p, m_memSize);
    }
}

void XxHash64::update(std::string_view sv) {
    update(sv.data(), sv.size());
}

uint64_t XxHash64::digest() const {
    uint64_t h64 = 0;

    if (m_totalLen >= 32) {
        const uint64_t v1 = m_v[0];
        const uint64_t v2 = m_v[1];
        const uint64_t v3 = m_v[2];
        const uint64_t v4 = m_v[3];

        h64 = std::rotl(v1, 1) + std::rotl(v2, 7) + std::rotl(v3, 12) + std::rotl(v4, 18);
        h64 = mergeRound(h64, v1);
        h64 = mergeRound(h64, v2);
        h64 = mergeRound(h64, v3);
        h64 = mergeRound(h64, v4);
    } else {
        h64 = m_seed + kPrime64_5;
    }

    h64 += m_totalLen;

    const uint8_t* p = m_mem;
    const uint8_t* const bEnd = m_mem + m_memSize;

    while (p + 8 <= bEnd) {
        const uint64_t k1 = round(0, read64Le(p));
        h64 ^= k1;
        h64 = std::rotl(h64, 27) * kPrime64_1 + kPrime64_4;
        p += 8;
    }

    if (p + 4 <= bEnd) {
        h64 ^= static_cast<uint64_t>(read32Le(p)) * kPrime64_1;
        h64 = std::rotl(h64, 23) * kPrime64_2 + kPrime64_3;
        p += 4;
    }

    while (p < bEnd) {
        h64 ^= static_cast<uint64_t>(*p) * kPrime64_5;
        h64 = std::rotl(h64, 11) * kPrime64_1;
        ++p;
    }

    h64 ^= h64 >> 33;
    h64 *= kPrime64_2;
    h64 ^= h64 >> 29;
    h64 *= kPrime64_3;
    h64 ^= h64 >> 32;

    return h64;
}

std::string XxHash64::digestHex() const {
    const uint64_t val = digest();
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << val;
    return oss.str();
}

// ---- Sha256 -----------------------------------------------------------------

namespace {
constexpr std::array<uint32_t, 64> kSha256K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};
} // namespace

Sha256::Sha256() {
    reset();
}

void Sha256::reset() {
    m_state[0] = 0x6a09e667;
    m_state[1] = 0xbb67ae85;
    m_state[2] = 0x3c6ef372;
    m_state[3] = 0xa54ff53a;
    m_state[4] = 0x510e527f;
    m_state[5] = 0x9b05688c;
    m_state[6] = 0x1f83d9ab;
    m_state[7] = 0x5be0cd19;
    m_bitCount = 0;
    std::memset(m_buffer, 0, sizeof(m_buffer));
}

void Sha256::transform(const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = m_state[0];
    uint32_t b = m_state[1];
    uint32_t c = m_state[2];
    uint32_t d = m_state[3];
    uint32_t e = m_state[4];
    uint32_t f = m_state[5];
    uint32_t g = m_state[6];
    uint32_t h = m_state[7];

    for (int i = 0; i < 64; ++i) {
        const uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + s1 + ch + kSha256K[i] + w[i];
        const uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
    m_state[4] += e;
    m_state[5] += f;
    m_state[6] += g;
    m_state[7] += h;
}

void Sha256::update(const void* data, size_t length) {
    if (!data || length == 0)
        return;

    const auto* p = static_cast<const uint8_t*>(data);
    size_t bufferOffset = static_cast<size_t>((m_bitCount / 8) % 64);
    m_bitCount += length * 8;

    while (length > 0) {
        const size_t copyLen = std::min(length, 64 - bufferOffset);
        std::memcpy(m_buffer + bufferOffset, p, copyLen);
        p += copyLen;
        length -= copyLen;
        bufferOffset += copyLen;

        if (bufferOffset == 64) {
            transform(m_buffer);
            bufferOffset = 0;
        }
    }
}

void Sha256::update(std::string_view sv) {
    update(sv.data(), sv.size());
}

std::vector<uint8_t> Sha256::digest() const {
    Sha256 copy = *this;
    const uint64_t totalBits = copy.m_bitCount;
    size_t bufferOffset = static_cast<size_t>((totalBits / 8) % 64);

    copy.m_buffer[bufferOffset++] = 0x80;
    if (bufferOffset > 56) {
        std::memset(copy.m_buffer + bufferOffset, 0, 64 - bufferOffset);
        copy.transform(copy.m_buffer);
        bufferOffset = 0;
    }
    std::memset(copy.m_buffer + bufferOffset, 0, 56 - bufferOffset);

    for (int i = 0; i < 8; ++i) {
        copy.m_buffer[56 + i] = static_cast<uint8_t>((totalBits >> (56 - i * 8)) & 0xFF);
    }
    copy.transform(copy.m_buffer);

    std::vector<uint8_t> out(32);
    for (int i = 0; i < 8; ++i) {
        out[i * 4 + 0] = static_cast<uint8_t>((copy.m_state[i] >> 24) & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((copy.m_state[i] >> 16) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((copy.m_state[i] >> 8) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>(copy.m_state[i] & 0xFF);
    }
    return out;
}

std::string Sha256::digestHex() const {
    const auto bytes = digest();
    std::ostringstream oss;
    for (const uint8_t b : bytes) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

// ---- Hash helper namespace --------------------------------------------------

namespace Hash {

uint64_t xx64(const void* data, size_t size, uint64_t seed) {
    XxHash64 h(seed);
    h.update(data, size);
    return h.digest();
}

uint64_t xx64(std::string_view text, uint64_t seed) {
    return xx64(text.data(), text.size(), seed);
}

std::string xx64Hex(const void* data, size_t size, uint64_t seed) {
    XxHash64 h(seed);
    h.update(data, size);
    return h.digestHex();
}

std::string xx64Hex(std::string_view text, uint64_t seed) {
    return xx64Hex(text.data(), text.size(), seed);
}

std::string fileXx64Hex(const std::string& path, size_t maxBytes, uint64_t seed) {
    std::ifstream file(platform::u8path(path), std::ios::binary);
    if (!file.is_open())
        return {};

    XxHash64 hasher(seed);
    constexpr size_t kChunkSize = 65536;
    std::vector<char> buffer(kChunkSize);
    size_t totalRead = 0;

    while (file) {
        size_t toRead = kChunkSize;
        if (maxBytes > 0 && totalRead + toRead > maxBytes) {
            toRead = maxBytes - totalRead;
        }
        if (toRead == 0)
            break;

        file.read(buffer.data(), static_cast<std::streamsize>(toRead));
        const std::streamsize count = file.gcount();
        if (count <= 0)
            break;

        hasher.update(buffer.data(), static_cast<size_t>(count));
        totalRead += static_cast<size_t>(count);
        if (maxBytes > 0 && totalRead >= maxBytes)
            break;
    }

    return hasher.digestHex();
}

std::string sha256Hex(const void* data, size_t size) {
    Sha256 h;
    h.update(data, size);
    return h.digestHex();
}

std::string sha256Hex(std::string_view text) {
    return sha256Hex(text.data(), text.size());
}

std::string fileSha256Hex(const std::string& path) {
    std::ifstream file(platform::u8path(path), std::ios::binary);
    if (!file.is_open())
        return {};

    Sha256 hasher;
    constexpr size_t kChunkSize = 65536;
    std::vector<char> buffer(kChunkSize);

    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(kChunkSize));
        const std::streamsize count = file.gcount();
        if (count <= 0)
            break;
        hasher.update(buffer.data(), static_cast<size_t>(count));
    }

    return hasher.digestHex();
}

bool isFileUnchanged(const std::string& path,
                    uint64_t cachedSize,
                    int64_t cachedModTime,
                    const std::string& cachedHash) {
    std::error_code ec;
    const auto u8p = platform::u8path(path);
    if (!std::filesystem::exists(u8p, ec))
        return false;

    const uint64_t currentSize = std::filesystem::file_size(u8p, ec);
    if (ec || currentSize != cachedSize)
        return false;

    const auto ftime = std::filesystem::last_write_time(u8p, ec);
    if (ec)
        return false;

    const auto sysTime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
    const int64_t currentModTime =
        std::chrono::duration_cast<std::chrono::seconds>(sysTime.time_since_epoch()).count();

    if (currentModTime == cachedModTime)
        return true; // Size and mtime match: file is unchanged!

    // If mtime differed, verify the hash to be certain
    const std::string currentHash = fileXx64Hex(path);
    return !currentHash.empty() && currentHash == cachedHash;
}

} // namespace Hash

} // namespace reals::util
