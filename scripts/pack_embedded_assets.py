import os
import hashlib
from pathlib import Path

def main():
    root = Path(__file__).resolve().parent.parent
    ui_web = root / "ui-web"
    output_cpp = root / "core" / "src" / "embedded" / "EmbeddedAssets.cpp"
    
    files = []
    for p in sorted(ui_web.rglob("*")):
        if p.is_file():
            rel = p.relative_to(ui_web).as_posix()
            files.append((rel, p))
            
    hasher = hashlib.sha256()
    for rel, p in files:
        hasher.update(rel.encode("utf-8"))
        hasher.update(p.read_bytes())
    bundle_hash = hasher.hexdigest()
    
    with open(output_cpp, "w", encoding="utf-8") as f:
        f.write('#include "reals/embedded/EmbeddedAssets.h"\n')
        f.write('#include "reals/platform/Path.h"\n')
        f.write('#include "reals/util/Log.h"\n\n')
        f.write('#include <filesystem>\n')
        f.write('#include <fstream>\n')
        f.write('#include <vector>\n')
        f.write('#include <algorithm>\n\n')
        f.write('namespace fs = std::filesystem;\n\n')
        f.write('namespace reals::embedded {\n\n')
        f.write('namespace {\n')
        f.write('constexpr auto kTag = "embedded";\n')
        f.write(f'constexpr const char* kBundleHash = "{bundle_hash}";\n\n')
        
        for idx, (rel, p) in enumerate(files):
            data = p.read_bytes()
            f.write(f'// {rel} ({len(data)} bytes)\n')
            f.write(f'static const unsigned char kFile_{idx}[] = {{\n')
            for i in range(0, len(data), 16):
                chunk = data[i:i+16]
                hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
                f.write(f'    {hex_str},\n')
            f.write('};\n\n')
            
        f.write('static const EmbeddedFile kEmbeddedFiles[] = {\n')
        for idx, (rel, p) in enumerate(files):
            size = p.stat().st_size
            f.write(f'    {{ "{rel}", kFile_{idx}, {size} }},\n')
        f.write('};\n\n')
        
        f.write('} // namespace\n\n')
        f.write('size_t getFileCount() {\n')
        f.write('    return sizeof(kEmbeddedFiles) / sizeof(kEmbeddedFiles[0]);\n')
        f.write('}\n\n')
        f.write('const EmbeddedFile* getFiles() {\n')
        f.write('    return kEmbeddedFiles;\n')
        f.write('}\n\n')
        f.write('const EmbeddedFile* findFile(std::string_view relativePath) {\n')
        f.write('    for (size_t i = 0; i < getFileCount(); ++i) {\n')
        f.write('        if (relativePath == kEmbeddedFiles[i].relativePath) {\n')
        f.write('            return &kEmbeddedFiles[i];\n')
        f.write('        }\n')
        f.write('    }\n')
        f.write('    return nullptr;\n')
        f.write('}\n\n')
        f.write('const char* getBundleHash() {\n')
        f.write('    return kBundleHash;\n')
        f.write('}\n\n')
        f.write('''bool ensureUiWebExtracted(const std::string& targetDir) {
    if (targetDir.empty()) return false;

    try {
        platform::ensureDir(targetDir);
        const std::string hashFile = platform::joinPath(targetDir, ".bundle.hash");
        
        bool needExtract = false;
        if (!fs::exists(targetDir + "/index.html")) {
            needExtract = true;
        } else if (fs::exists(hashFile)) {
            std::ifstream hIn(platform::u8path(hashFile));
            std::string existingHash;
            if (hIn >> existingHash) {
                if (existingHash != kBundleHash) {
                    needExtract = true;
                }
            } else {
                needExtract = true;
            }
        } else {
            needExtract = true;
        }

        if (!needExtract) {
            return true;
        }

        LOG_INFO(kTag, "Extracting embedded ui-web assets to target directory...");
        for (size_t i = 0; i < getFileCount(); ++i) {
            const auto& item = kEmbeddedFiles[i];
            const std::string outPath = platform::joinPath(targetDir, item.relativePath);
            
            const size_t slashPos = outPath.find_last_of("/\\\\");
            if (slashPos != std::string::npos) {
                platform::ensureDir(outPath.substr(0, slashPos));
            }

            std::ofstream out(platform::u8path(outPath), std::ios::binary);
            if (out.is_open()) {
                out.write(reinterpret_cast<const char*>(item.data), item.size);
                out.close();
            } else {
                LOG_ERROR(kTag, ("Failed to write embedded asset: " + outPath).c_str());
            }
        }

        std::ofstream hOut(platform::u8path(hashFile));
        if (hOut.is_open()) {
            hOut << kBundleHash;
            hOut.close();
        }

        LOG_INFO(kTag, "Successfully extracted all embedded assets.");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(kTag, (std::string("Exception extracting embedded assets: ") + e.what()).c_str());
        return false;
    }
}

} // namespace reals::embedded
''')
    print("EmbeddedAssets.cpp generated successfully with bundle hash:", bundle_hash)

if __name__ == '__main__':
    main()
