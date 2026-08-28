# Milestone 2 Scope Breakdown: Local Offline AI Inference Engine & Models

## Target Deliverables
1. **Directory Structure**:
   - `core/include/reals/ai/`
     - `OnnxEngine.h`
     - `ModelManager.h`
     - `FeatureExtractor.h`
     - `TempoDetector.h`
     - `KeyDetector.h`
     - `GenreClassifier.h`
     - `MoodClassifier.h`
     - `ClapEmbedder.h`
   - `core/src/ai/`
     - `OnnxEngine.cpp`
     - `ModelManager.cpp`
     - `FeatureExtractor.cpp`
     - `TempoDetector.cpp`
     - `KeyDetector.cpp`
     - `GenreClassifier.cpp`
     - `MoodClassifier.cpp`
     - `ClapEmbedder.cpp`
   - `tests/`
     - `test_ai.cpp`

2. **Feature Contracts**:
   - `OnnxEngine`:
     - `bool init(const std::string& modelsDir)`
     - `bool loadModel(const std::string& modelName, const std::string& modelPath)`
     - `bool isModelLoaded(const std::string& modelName) const`
     - `bool runInference(...)`
   - `ModelManager`:
     - Path resolution (`%APPDATA%/RealsLab/models/`)
     - Model registration, status querying (`isReady`, `getModelPath`)
     - SHA256 checksum verification
   - `TempoDetector`:
     - `struct TempoResult { float bpm; float confidence; std::vector<float> beatOnsets; std::string method; }`
     - `TempoResult detect(const float* pcm, size_t frames, int sampleRate)`
     - Model: Essentia TempoCNN + fallback: `RhythmExtractor2013` (envelope autocorrelation + peak picking)
   - `KeyDetector`:
     - `struct KeyResult { std::string key; std::string mode; std::string camelot; std::string openKey; float confidence; }`
     - `KeyResult detect(const float* pcm, size_t frames, int sampleRate)`
     - Ensemble voting: EDMA + Temperley + Krumhansl-Schmuckler profiles
     - Camelot notation mapping (`1A` - `12B`) and OpenKey (`1d` - `12m`)
   - `GenreClassifier`:
     - `struct GenreResult { std::string tag; float score; }`
     - `std::vector<GenreResult> classify(const float* pcm, size_t frames, int sampleRate, int topK = 5)`
     - 400 Discogs subgenres
   - `MoodClassifier`:
     - `struct MoodResult { std::string tag; float score; }`
     - `std::vector<MoodResult> classify(const float* pcm, size_t frames, int sampleRate, float threshold = 0.2f)`
     - 56 Jamendo mood & theme tags
   - `ClapEmbedder`:
     - `std::vector<float> embedAudio(const float* pcm, size_t frames, int sampleRate)` (512-dim normalized)
     - `std::vector<float> embedText(const std::string& text)` (512-dim normalized)
     - Cosine similarity helper
