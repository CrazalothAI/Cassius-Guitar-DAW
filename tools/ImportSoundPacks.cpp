#include "../Source/SoundPack.h"
#include "../Source/LibraryStore.h"
#include <iostream>

int main(int argc, char** argv)
{
    try {
        if (argc != 2 && argc != 3) { std::cerr << "Usage: CassianSoundImport manifest.json [library-folder]\n"; return 2; }
        const auto paths = juce::JSON::parse(juce::File(argv[1]).loadFileAsString());
        if (!paths.isArray() || paths.size() > 128) throw std::runtime_error("Manifest must be an array of up to 128 ZIP paths");
        LibraryStore store(argc == 3 ? juce::File(argv[2]) : LibraryStore::defaultRoot());
        AssetLibrary library; library.merge(store.load()); const int before = library.tree.getNumChildren(); int files = 0, errors = 0;
        for (const auto& path : *paths.getArray()) {
            if (!path.isString() || !juce::File::isAbsolutePath(path.toString())) throw std::runtime_error("Archive paths must be absolute");
            const juce::File file(path.toString());
            const auto result = SoundPack::read(file, [&](auto asset) {
                store.manage(asset);
                // Random temporary extraction paths are not useful relink aliases.
                asset.setProperty("aliases", "[]", nullptr); library.upsert(asset);
            });
            store.save(library.tree); files += result.imported; errors += result.errors.size();
            std::cout << file.getFileName() << ": " << result.imported << " imported, " << result.errors.size() << " errors\n";
            for (const auto& error : result.errors) std::cerr << error << '\n';
        }
        std::cout << "Validated/imported " << files << " files; " << library.tree.getNumChildren() - before << " new library entries\nLibrary: " << store.root().getFullPathName() << '\n';
        return errors == 0 ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
