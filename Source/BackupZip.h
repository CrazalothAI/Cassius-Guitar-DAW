#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <vector>

// Bounded streaming personal archives. ZIP64 reading intentionally supports
// only the stored, single-disk profile emitted here; legacy ZIP uses JUCE.
namespace BackupZip {
constexpr juce::int64 classicLimit = 2LL * 1024 * 1024 * 1024 - 16 * 1024 * 1024;
constexpr juce::int64 archiveLimit = 32LL * 1024 * 1024 * 1024;
constexpr int entryLimit = 8193;
struct Source { juce::File file; juce::String name; juce::int64 bytes; };
struct Entry { juce::String name; juce::int64 bytes; juce::uint32 crc; bool symlink; };
bool needsZip64(const std::vector<Source>&);
juce::uint32 crcUpdate(juce::uint32, const char*, int);
void write(const juce::File&, const std::vector<Source>&, const std::atomic<bool>&,
           std::function<void(double)> = {}, bool forceZip64 = false);
class Reader {
public:
    explicit Reader(const juce::File&);
    bool isZip64() const { return zip64; }
    const std::vector<Entry>& entries() const { return rows; }
    std::unique_ptr<juce::InputStream> open(int index) const;
private:
    juce::File file;
    bool zip64 = false;
    std::unique_ptr<juce::ZipFile> legacy;
    std::vector<Entry> rows;
    std::vector<juce::int64> offsets;
};
}
