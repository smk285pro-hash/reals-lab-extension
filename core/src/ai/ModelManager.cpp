#include "reals/ai/ModelManager.h"
#include "reals/ai/OnnxEngine.h"
#include "reals/platform/Path.h"
#include "reals/util/Hash.h"
#include "reals/util/Log.h"

#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace reals::ai {

namespace {
constexpr std::string_view kTag = "ModelManager";
}

struct ModelManager::Impl {
    mutable std::mutex mutex;
    std::string modelsDir;
    std::unordered_map<std::string, ModelMetadata> registry;

    void populateDefaults() {
        registry["tempo_cnn"] = {
            "tempo_cnn",
            "tempo_cnn.onnx",
            "Essentia TempoCNN for BPM and Beat Onset Detection",
            "",
            12500000,
            false,
            false
        };
        registry["edma_key"] = {
            "edma_key",
            "edma_key.onnx",
            "Essentia EDMA Key and Tonic Classification",
            "",
            8400000,
            false,
            false
        };
        registry["discogs_maest"] = {
            "discogs_maest",
            "discogs_maest.onnx",
            "Discogs-MAEST 400 Musical Subgenres Classifier",
            "",
            45000000,
            false,
            false
        };
        registry["mood_jamendo"] = {
            "mood_jamendo",
            "mood_jamendo.onnx",
            "Mood-Jamendo 56 Tags Multi-Label Classifier",
            "",
            18000000,
            false,
            false
        };
        registry["clap_audio"] = {
            "clap_audio",
            "clap_audio.onnx",
            "CLAP Audio 512-dim Semantic Embedding Model",
            "",
            140000000,
            false,
            false
        };
        registry["clap_text"] = {
            "clap_text",
            "clap_text.onnx",
            "CLAP Text 512-dim Semantic Embedding Model",
            "",
            110000000,
            false,
            false
        };
    }
};

ModelManager& ModelManager::instance() {
    static ModelManager s_instance;
    return s_instance;
}

ModelManager::ModelManager() : m_impl(std::make_unique<Impl>()) {
    m_impl->populateDefaults();
    init();
}

ModelManager::~ModelManager() = default;

bool ModelManager::init(const std::string& customDir) {
    std::lock_guard lock(m_impl->mutex);
    if (customDir.empty()) {
        m_impl->modelsDir = platform::joinPath(platform::dataDir(), "models");
    } else {
        m_impl->modelsDir = customDir;
    }
    platform::ensureDir(m_impl->modelsDir);
    OnnxEngine::instance().init(m_impl->modelsDir);
    LOG_INFO(kTag, "ModelManager initialized at: " + m_impl->modelsDir);
    return true;
}

std::string ModelManager::getModelsDir() const {
    std::lock_guard lock(m_impl->mutex);
    return m_impl->modelsDir;
}

bool ModelManager::isModelAvailable(const std::string& modelId) const {
    std::lock_guard lock(m_impl->mutex);
    auto it = m_impl->registry.find(modelId);
    if (it == m_impl->registry.end()) {
        return false;
    }
    const std::string fullPath = platform::joinPath(m_impl->modelsDir, it->second.fileName);
    return std::filesystem::exists(platform::u8path(fullPath));
}

std::string ModelManager::getModelPath(const std::string& modelId) const {
    std::lock_guard lock(m_impl->mutex);
    auto it = m_impl->registry.find(modelId);
    if (it == m_impl->registry.end()) {
        return {};
    }
    return platform::joinPath(m_impl->modelsDir, it->second.fileName);
}

bool ModelManager::verifyChecksum(const std::string& modelId, const std::string& expectedSha256) const {
    const std::string path = getModelPath(modelId);
    if (path.empty() || !std::filesystem::exists(platform::u8path(path))) {
        return false;
    }
    const std::string actual = util::sha256File(path);
    if (expectedSha256.empty()) {
        return !actual.empty();
    }
    return (actual == expectedSha256);
}

bool ModelManager::ensureModelLoaded(const std::string& modelId) {
    if (OnnxEngine::instance().isModelLoaded(modelId)) {
        return true;
    }
    if (!isModelAvailable(modelId)) {
        return false;
    }
    const std::string path = getModelPath(modelId);
    return OnnxEngine::instance().loadModel(modelId, path);
}

std::vector<ModelMetadata> ModelManager::listModels() const {
    std::lock_guard lock(m_impl->mutex);
    std::vector<ModelMetadata> list;
    list.reserve(m_impl->registry.size());

    for (const auto& [id, meta] : m_impl->registry) {
        ModelMetadata item = meta;
        const std::string fullPath = platform::joinPath(m_impl->modelsDir, meta.fileName);
        auto fsPath = platform::u8path(fullPath);
        item.isDownloaded = std::filesystem::exists(fsPath);
        item.isLoaded = OnnxEngine::instance().isModelLoaded(id);
        list.push_back(item);
    }
    return list;
}

void ModelManager::registerModel(const ModelMetadata& meta) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->registry[meta.id] = meta;
}

} // namespace reals::ai
