#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace reals::ai {

struct TensorData {
    std::string name;
    std::vector<int64_t> shape;
    std::vector<float> data;
};

class OnnxEngine {
public:
    static OnnxEngine& instance();

    // Initialize the engine with models directory
    bool init(const std::string& modelsDir = "");
    [[nodiscard]] bool isInitialized() const;

    // Load an ONNX model by logical name (e.g. "tempo_cnn", "edma_key")
    bool loadModel(const std::string& modelName, const std::string& modelPath);
    [[nodiscard]] bool isModelLoaded(const std::string& modelName) const;
    void unloadModel(const std::string& modelName);
    void unloadAll();

    // Run multi-input multi-output inference
    bool runInference(
        const std::string& modelName,
        const std::vector<TensorData>& inputs,
        std::vector<TensorData>& outputs);

    // Convenience single input / single output inference
    bool runSingle(
        const std::string& modelName,
        const std::vector<float>& inputData,
        const std::vector<int64_t>& shape,
        std::vector<float>& outputData);

    [[nodiscard]] std::string getModelsDir() const;

private:
    OnnxEngine();
    ~OnnxEngine();
    OnnxEngine(const OnnxEngine&) = delete;
    OnnxEngine& operator=(const OnnxEngine&) = delete;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace reals::ai
