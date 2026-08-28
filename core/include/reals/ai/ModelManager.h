#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace reals::ai {

struct ModelMetadata {
    std::string id;
    std::string fileName;
    std::string description;
    std::string expectedSha256;
    size_t expectedSize = 0;
    bool isDownloaded = false;
    bool isLoaded = false;
};

class ModelManager {
public:
    static ModelManager& instance();

    // Initialize with models directory (defaults to %APPDATA%/RealsLab/models)
    bool init(const std::string& customDir = "");

    // Returns the models directory
    [[nodiscard]] std::string getModelsDir() const;

    // Check if model file exists on disk
    [[nodiscard]] bool isModelAvailable(const std::string& modelId) const;

    // Get absolute path to model file
    [[nodiscard]] std::string getModelPath(const std::string& modelId) const;

    // Verify SHA-256 checksum of model file
    [[nodiscard]] bool verifyChecksum(const std::string& modelId, const std::string& expectedSha256 = "") const;

    // Load model into OnnxEngine
    bool ensureModelLoaded(const std::string& modelId);

    // List all registered models with status
    [[nodiscard]] std::vector<ModelMetadata> listModels() const;

    // Register a custom model entry
    void registerModel(const ModelMetadata& meta);

private:
    ModelManager();
    ~ModelManager();
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace reals::ai
