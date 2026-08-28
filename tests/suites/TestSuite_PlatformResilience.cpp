// Platform & resilience tests (MAJ-10/11, MIN-09 + guards for CRIT-04/MAJ-04).
//
// - I18n: corrupt/missing disk tables must fall back to the embedded table,
//   and the embedded table must cover every key used by the JSON files.
// - LabApi: on non-Windows builds the stubs must throw (never link-fail).
// - FFT: non-power-of-two sizes must be rejected safely (no OOB write).
// - DirWatch/HttpClient: Windows-only smoke tests (compiled out elsewhere).
#include "../framework/TestRunner.h"

#include "reals/ai/FeatureExtractor.h"
#include "reals/i18n/I18n.h"
#include "reals/lab/LabApi.h"
#include "reals/platform/Path.h"
#ifdef _WIN32
#include "reals/net/HttpClient.h"
#include "reals/platform/DirWatch.h"
#endif

#include <atomic>
#include <chrono>
#include <complex>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string writeTempFile(const std::string& dir, const std::string& name, const std::string& content) {
    const std::string path = reals::platform::joinPath(dir, name);
    std::ofstream out(reals::platform::u8path(path), std::ios::binary);
    out << content;
    return path;
}

std::string makeTempDir(const std::string& tag) {
    const std::string dir = reals::platform::joinPath(
        reals::platform::tempDir(), "reals_i18n_test_" + tag);
    reals::platform::ensureDir(dir);
    return dir;
}

} // namespace

// ---- I18n resilience (MIN-09) ------------------------------------------------

TEST(PlatformResilience, I18nCorruptJsonFallsBackToEmbedded) {
    const std::string dir = makeTempDir("corrupt");
    // Malformed JSON on disk — must not throw, must not clear the embedded table.
    writeTempFile(dir, "strings_vi.json", "{ this is not valid json !!! ");
    writeTempFile(dir, "strings_en.json", "{\"app.title\": 42}"); // wrong type

    reals::i18n::init(dir);
    reals::i18n::setLanguage("vi");
    EXPECT_EQ(reals::i18n::tr("app.title"), "REALS LAB");
    EXPECT_EQ(reals::i18n::tr("nav.browser"), "Browser");

    reals::i18n::setLanguage("en");
    EXPECT_EQ(reals::i18n::tr("nav.account"), "Account");
    // Unknown keys fall back to the key itself (documented contract).
    EXPECT_EQ(reals::i18n::tr("no.such.key"), "no.such.key");
}

TEST(PlatformResilience, I18nMissingDirFallsBackToEmbedded) {
    reals::i18n::init("/nonexistent/reals/assets");
    reals::i18n::setLanguage("vi");
    EXPECT_EQ(reals::i18n::tr("app.title"), "REALS LAB");
    EXPECT_FALSE(reals::i18n::tr("browser.noResults").empty());
    EXPECT_EQ(reals::i18n::tr("browser.noResults"), reals::i18n::tr("browser.noResults"));
}

TEST(PlatformResilience, I18nEmbeddedTableCoversNewKeys) {
    // MIN-03/MIN-07 regression: keys added to strings_*.json must also exist
    // in the embedded fallback table (the previous table had only 93 of the
    // JSON keys). Verify with a representative sample incl. the newest keys.
    reals::i18n::init(""); // empty dir -> embedded only
    reals::i18n::setLanguage("en");
    EXPECT_EQ(reals::i18n::tr("browser.noResults"), "No similar samples found");
    EXPECT_EQ(reals::i18n::tr("toast.labError"), "Lab error");
    EXPECT_EQ(reals::i18n::tr("window.dockHint"), "Dock into REAPER / Float window");
    // Existence checks: embedded value must resolve (never echo the key back).
    const auto highWarn = reals::i18n::tr("scanner.cpuMode.highWarn");
    EXPECT_NE(highWarn, "scanner.cpuMode.highWarn");
    EXPECT_FALSE(highWarn.empty());
}

TEST(PlatformResilience, I18nDiskTableOverridesEmbedded) {
    const std::string dir = makeTempDir("override");
    writeTempFile(dir, "strings_vi.json", "{\"app.title\": \"DISK VI\"}");
    writeTempFile(dir, "strings_en.json", "{\"app.title\": \"DISK EN\"}");

    reals::i18n::init(dir);
    reals::i18n::setLanguage("en");
    EXPECT_EQ(reals::i18n::tr("app.title"), "DISK EN");
    // Keys absent from the disk table still resolve from the embedded table.
    EXPECT_EQ(reals::i18n::tr("nav.browser"), "Browser");
}

// ---- FFT safety guard (MAJ-04) -----------------------------------------------

TEST(PlatformResilience, FftRejectsNonPowerOfTwoSafely) {
    // n = 6 would previously index x[7] — out of bounds heap write.
    std::vector<std::complex<float>> x6(6, {1.0f, 0.0f});
    reals::ai::FeatureExtractor::fft(x6);
    EXPECT_EQ(x6.size(), static_cast<size_t>(6));
    // Guarded: buffer left untouched (all ones).
    for (const auto& v : x6)
        EXPECT_NEAR(v.real(), 1.0f, 1e-6f);
}

TEST(PlatformResilience, FftPowerOfTwoStillTransforms) {
    // Constant signal of length 8: DC bin must equal 8, everything else 0.
    std::vector<std::complex<float>> x(8, {1.0f, 0.0f});
    reals::ai::FeatureExtractor::fft(x);
    EXPECT_NEAR(x[0].real(), 8.0f, 1e-4f);
    EXPECT_NEAR(std::abs(x[1]), 0.0f, 1e-4f);
    EXPECT_NEAR(std::abs(x[7]), 0.0f, 1e-4f);
}

// ---- LabApi non-Windows behavior (MAJ-10) ------------------------------------

#ifndef _WIN32
TEST(PlatformResilience, LabApiStubsThrowOnNonWindows) {
    // The stubs must throw (caught by Bridge lab workers -> lab.error event),
    // never crash the process or fail to link.
    EXPECT_THROW(reals::lab::LabApi::analyze("/tmp/x.wav"), std::runtime_error);
    EXPECT_THROW(reals::lab::LabApi::pollJob("task-1"), std::runtime_error);
    EXPECT_THROW(reals::lab::LabApi::downloadToFile("/a", "/b"), std::runtime_error);
}
#else
TEST(PlatformResilience, LabApiWindowsSymbolsLinked) {
    // Windows: methods exist and fail gracefully on a nonexistent file
    // instead of crashing.
    EXPECT_THROW(reals::lab::LabApi::analyze("Z:/definitely/not/a/file.wav"),
                 std::runtime_error);
}
#endif

// ---- DirWatch & HttpClient smoke coverage (MAJ-10/11, Windows-only) ----------

#ifdef _WIN32

TEST(PlatformResilience, DirWatchDeliversCallback) {
    reals::platform::DirWatch watch;
    std::atomic<bool> fired{false};
    const std::string dir = makeTempDir("dirwatch");
    watch.start(dir, [&](const std::string&) { fired = true; });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    writeTempFile(dir, "touch.txt", "x");
    // IOCP delivery is asynchronous — allow a generous window.
    for (int i = 0; i < 50 && !fired; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    watch.stop();
    EXPECT_TRUE(fired.load());
}

TEST(PlatformResilience, HttpClientTransportErrorIsReported) {
    auto& http = reals::net::HttpClient::instance();
    reals::net::Request req;
    req.method = "GET";
    req.url = "http://invalid.invalid.invalid.nonexistent/reals-test";
    const auto res = http.send(req);
    // Transport-level failure: no 2xx, error string populated, no crash.
    EXPECT_TRUE(res.statusCode < 200 || res.statusCode >= 300);
    EXPECT_FALSE(res.error.empty());
}

#endif // _WIN32
