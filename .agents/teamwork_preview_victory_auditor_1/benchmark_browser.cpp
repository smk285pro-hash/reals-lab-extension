#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include "reals/browser/BrowserModel.h"
#include "reals/platform/Path.h"

namespace fs = std::filesystem;

int main() {
    std::string tempRoot = reals::platform::joinPath(reals::platform::tempDir(), "reals_perf_benchmark_2500");
    std::error_code ec;
    fs::remove_all(reals::platform::u8path(tempRoot), ec);
    fs::create_directories(reals::platform::u8path(tempRoot), ec);

    std::cout << "[Benchmark] Creating 2,500 files across 25 subdirectories at: " << tempRoot << std::endl;

    // Create 25 subdirectories with 100 files each = 2,500 files
    const std::vector<std::string> exts = {".wav", ".mp3", ".flac", ".ogg", ".aiff", ".m4a", ".mid", ".midi", ".txt", ".rpp"};
    for (int d = 0; d < 25; ++d) {
        std::string subDir = reals::platform::joinPath(tempRoot, "Folder_" + std::to_string(d));
        fs::create_directories(reals::platform::u8path(subDir), ec);
        for (int f = 0; f < 100; ++f) {
            std::string ext = exts[(d * 100 + f) % exts.size()];
            std::string filePath = reals::platform::joinPath(subDir, "sample_kick_snare_" + std::to_string(f) + ext);
            std::ofstream out(reals::platform::u8path(filePath), std::ios::binary);
            out << "RIFF....WAVEfmt ....data....";
        }
    }

    std::cout << "[Benchmark] Directory created successfully. Running cold walk + sorting..." << std::endl;

    reals::browser::BrowserModel model;
    
    // Benchmark 1: Cold scan + listDir (walk + matchMediaExt + sort)
    auto t0 = std::chrono::high_resolution_clock::now();
    auto entries = model.listDir(tempRoot);
    auto t1 = std::chrono::high_resolution_clock::now();
    double coldMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "[Benchmark] Cold listDir found: " << entries.size() << " media files in " << coldMs << " ms." << std::endl;

    // Benchmark 2: Invalidate cache and run warm iterations
    double sumWarmMs = 0.0;
    int iterations = 10;
    for (int i = 0; i < iterations; ++i) {
        model.invalidateAll();
        auto start = std::chrono::high_resolution_clock::now();
        auto res = model.listDir(tempRoot);
        auto end = std::chrono::high_resolution_clock::now();
        sumWarmMs += std::chrono::duration<double, std::milli>(end - start).count();
    }
    double avgWarmMs = sumWarmMs / iterations;

    std::cout << "[Benchmark] Warm average over " << iterations << " iterations (with cache invalidation): " << avgWarmMs << " ms." << std::endl;

    // Benchmark 3: Cached listDir
    auto tCached0 = std::chrono::high_resolution_clock::now();
    auto cachedRes = model.listDir(tempRoot);
    auto tCached1 = std::chrono::high_resolution_clock::now();
    double cachedMs = std::chrono::duration<double, std::milli>(tCached1 - tCached0).count();
    std::cout << "[Benchmark] In-memory cached listDir: " << cachedMs << " ms." << std::endl;

    // Clean up
    fs::remove_all(reals::platform::u8path(tempRoot), ec);

    std::cout << "[Benchmark] Result Verification:" << std::endl;
    std::cout << "  - Target: < 30.0 ms" << std::endl;
    std::cout << "  - Cold Time: " << coldMs << " ms" << std::endl;
    std::cout << "  - Average Time (uncached): " << avgWarmMs << " ms" << std::endl;
    std::cout << "  - Cached Time: " << cachedMs << " ms" << std::endl;

    if (avgWarmMs < 30.0) {
        std::cout << "[Benchmark] SUCCESS: Sub-30ms performance verified!" << std::endl;
        return 0;
    } else {
        std::cout << "[Benchmark] FAILED: Exceeded 30ms limit!" << std::endl;
        return 1;
    }
}
