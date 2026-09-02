#include "../framework/TestRunner.h"
#include "reals/embedded/EmbeddedAssets.h"
#include "reals/platform/Path.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

TEST(EmbeddedAssets, CatalogValidation) {
    size_t count = reals::embedded::getFileCount();
    EXPECT_GE(count, 20);

    const auto* html = reals::embedded::findFile("index.html");
    EXPECT_TRUE(html != nullptr);
    if (html) {
        EXPECT_GT(html->size, 500);
    }

    const auto* js = reals::embedded::findFile("app.js");
    EXPECT_TRUE(js != nullptr);
    if (js) {
        EXPECT_GT(js->size, 10000);
    }

    const auto* css = reals::embedded::findFile("app.css");
    EXPECT_TRUE(css != nullptr);
    if (css) {
        EXPECT_GT(css->size, 5000);
    }

    std::string hash = reals::embedded::getBundleHash();
    EXPECT_EQ(hash.length(), 64);
}

TEST(EmbeddedAssets, ExtractionWorkflow) {
    std::string tempDir = reals::platform::joinPath(
        reals::platform::tempDir(),
        "reals_embedded_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

    // First run extracts everything
    bool ok1 = reals::embedded::ensureUiWebExtracted(tempDir);
    EXPECT_TRUE(ok1);
    EXPECT_TRUE(fs::exists(tempDir + "/index.html"));
    EXPECT_TRUE(fs::exists(tempDir + "/app.js"));
    EXPECT_TRUE(fs::exists(tempDir + "/.bundle.hash"));

    // Second run with matching hash is a clean no-op
    bool ok2 = reals::embedded::ensureUiWebExtracted(tempDir);
    EXPECT_TRUE(ok2);

    // Clean up temp dir
    std::error_code ec;
    fs::remove_all(tempDir, ec);
}
