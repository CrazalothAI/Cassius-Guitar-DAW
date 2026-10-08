#include "BackupZip.h"
#include <array>
#include <stdexcept>

namespace BackupZip {
namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void checkCancel(const std::atomic<bool>& cancelled) { require(!cancelled.load(), "Backup/recovery cancelled. Existing work was preserved."); }
int nameLength(const juce::String& name) {
    const auto length = name.getNumBytesAsUTF8();
    require(length > 0 && length <= 960, "Unsupported archive filename length."); return static_cast<int>(length);
}
void seek(juce::InputStream& input, juce::int64 position, juce::int64 bytes) {
    require(position >= 0 && bytes >= 0 && position <= input.getTotalLength() && bytes <= input.getTotalLength() - position
        && input.setPosition(position), "Truncated or invalid backup archive offsets.");
}
juce::int64 read64(juce::InputStream& input) {
    const auto value = input.readInt64(); require(value >= 0 && value <= archiveLimit, "Backup archive size/offset exceeds 32 GiB."); return value;
}
juce::String readName(juce::InputStream& input, int length) {
    require(length > 0 && length <= 960, "Invalid backup filename length.");
    std::vector<char> buffer(static_cast<size_t>(length));
    require(input.read(buffer.data(), length) == length, "Truncated backup filename.");
    const auto name = juce::String::fromUTF8(buffer.data(), length);
    require(name.getNumBytesAsUTF8() == length && std::memcmp(name.toRawUTF8(), buffer.data(), static_cast<size_t>(length)) == 0, "Invalid UTF-8 backup filename."); return name;
}
}
juce::uint32 crcUpdate(juce::uint32 crc, const char* data, int size) {
    static const auto table = [] { std::array<juce::uint32,256> values{}; for (unsigned i=0;i<256;++i) {auto v=i; for(int b=0;b<8;++b) v=(v>>1)^((v&1)?0xedb88320u:0u); values[i]=v;} return values; }();
    for(int i=0;i<size;++i) crc=table[(crc^static_cast<unsigned char>(data[i]))&255]^(crc>>8); return crc;
}
bool needsZip64(const std::vector<Source>& files) {
    juce::int64 size=22;
    for(const auto& item:files) {
        require(item.bytes>=0 && item.bytes<=archiveLimit && size<=archiveLimit-item.bytes, "Backup exceeds 32 GiB.");
        size+=item.bytes+92+2*nameLength(item.name);
    }
    return size>=classicLimit;
}
void write(const juce::File& destination, const std::vector<Source>& files, const std::atomic<bool>& cancelled, std::function<void(double)> progress, bool forceZip64) {
    require(!files.empty() && files.size()<=entryLimit, "Invalid backup file count.");
    const bool extended=needsZip64(files) || forceZip64;
    juce::int64 archiveBytes=extended?98:22, total=0, done=0;
    for(const auto& item:files) {
        archiveBytes+=item.bytes+(extended?148:92)+2*nameLength(item.name); total+=item.bytes;
        require(archiveBytes<=archiveLimit, "Backup exceeds the 32 GiB archive limit.");
    }
    require(!destination.existsAsFile() || destination.getSize()==0, "Backup temporary output must be empty.");
    auto output=destination.createOutputStream(); require(output!=nullptr, "Could not open backup output.");
    struct Central { const Source* item; juce::uint32 crc; juce::int64 offset; }; std::vector<Central> entries;
    std::vector<char> buffer(65536);
    for(const auto& item:files) {
        checkCancel(cancelled); const auto offset=output->getPosition(); const auto length=static_cast<short>(nameLength(item.name));
        output->writeInt(0x04034b50); output->writeShort(extended?45:20); output->writeShort(0x0808); output->writeShort(0);
        output->writeShort(0); output->writeShort(33); output->writeInt(0); output->writeInt(extended?-1:0); output->writeInt(extended?-1:0); output->writeShort(length); output->writeShort(extended?20:0);
        output->write(item.name.toRawUTF8(),static_cast<size_t>(length));
        if(extended) {output->writeShort(1); output->writeShort(16); output->writeInt64(0); output->writeInt64(0);}
        auto input=item.file.createInputStream(); require(input!=nullptr, "Could not read backup source.");
        juce::uint32 crc=0xffffffffu; juce::int64 copied=0;
        while(copied<item.bytes) {
            checkCancel(cancelled); const int wanted=static_cast<int>(juce::jmin<juce::int64>(buffer.size(),item.bytes-copied));
            const int count=input->read(buffer.data(),wanted); require(count==wanted && output->write(buffer.data(),static_cast<size_t>(count)), "Backup source changed or disk write failed.");
            crc=crcUpdate(crc,buffer.data(),count); copied+=count; done+=count;
            if(progress) progress(.15+.6*static_cast<double>(done)/static_cast<double>(juce::jmax<juce::int64>(1,total)));
        }
        require(input->isExhausted(), "Backup source grew while copying. Finish recording or editing and try again.");
        crc^=0xffffffffu; output->writeInt(0x08074b50); output->writeInt(static_cast<int>(crc));
        if(extended) {output->writeInt64(item.bytes); output->writeInt64(item.bytes);} else {output->writeInt(static_cast<int>(item.bytes)); output->writeInt(static_cast<int>(item.bytes));}
        entries.push_back({&item,crc,offset});
    }
    const auto start=output->getPosition();
    for(const auto& entry:entries) {
        const auto& item=*entry.item; output->writeInt(0x02014b50); output->writeShort(extended?45:20); output->writeShort(extended?45:20); output->writeShort(0x0808); output->writeShort(0);
        output->writeShort(0); output->writeShort(33); output->writeInt(static_cast<int>(entry.crc)); output->writeInt(extended?-1:static_cast<int>(item.bytes)); output->writeInt(extended?-1:static_cast<int>(item.bytes));
        output->writeShort(static_cast<short>(nameLength(item.name))); output->writeShort(extended?28:0); output->writeShort(0); output->writeShort(0); output->writeShort(0); output->writeInt(0); output->writeInt(extended?-1:static_cast<int>(entry.offset));
        output->write(item.name.toRawUTF8(),static_cast<size_t>(nameLength(item.name)));
        if(extended) {output->writeShort(1); output->writeShort(24); output->writeInt64(item.bytes); output->writeInt64(item.bytes); output->writeInt64(entry.offset);}
    }
    const auto end=output->getPosition();
    if(extended) {
        output->writeInt(0x06064b50); output->writeInt64(44); output->writeShort(45); output->writeShort(45); output->writeInt(0); output->writeInt(0);
        output->writeInt64(static_cast<juce::int64>(entries.size())); output->writeInt64(static_cast<juce::int64>(entries.size())); output->writeInt64(end-start); output->writeInt64(start);
        output->writeInt(0x07064b50); output->writeInt(0); output->writeInt64(end); output->writeInt(1);
    }
    output->writeInt(0x06054b50); output->writeShort(0); output->writeShort(0);
    output->writeShort(extended?-1:static_cast<short>(entries.size())); output->writeShort(extended?-1:static_cast<short>(entries.size()));
    output->writeInt(extended?-1:static_cast<int>(end-start)); output->writeInt(extended?-1:static_cast<int>(start)); output->writeShort(0);
    output->flush(); require(output->getStatus().wasOk() && output->getPosition()==archiveBytes, "Backup write failed or archive size was inconsistent.");
}
Reader::Reader(const juce::File& archive):file(archive) {
    require(file.existsAsFile() && file.getSize()>=22 && file.getSize()<=archiveLimit, "Choose a Cassian backup no larger than 32 GiB.");
    auto input=file.createInputStream(); require(input!=nullptr, "Could not read backup archive.");
    const auto tail=file.getSize()-22; seek(*input,tail,22);
    if(input->readInt()==0x06054b50) {
        input->skipNextBytes(4); const auto count=static_cast<juce::uint16>(input->readShort()); const auto countTotal=static_cast<juce::uint16>(input->readShort());
        const auto size=static_cast<juce::uint32>(input->readInt()), offset=static_cast<juce::uint32>(input->readInt());
        zip64=count==0xffff || countTotal==0xffff || size==0xffffffffu || offset==0xffffffffu;
    }
    if(!zip64) {
        require(file.getSize()<classicLimit, "Large backups require Cassian ZIP64 format."); legacy=std::make_unique<juce::ZipFile>(file);
        require(legacy->getNumEntries()>0 && legacy->getNumEntries()<=entryLimit, "Invalid backup file count.");
        for(int i=0;i<legacy->getNumEntries();++i) {const auto* entry=legacy->getEntry(i); rows.push_back({entry->filename,entry->uncompressedSize,0,entry->isSymbolicLink});}
        return;
    }
    // Strict profile rather than a general-purpose ZIP64 importer. Bounds and
    // consistency are checked before opening any payload stream or output file.
    seek(*input,tail,22); require(input->readInt()==0x06054b50 && input->readShort()==0 && input->readShort()==0
        && static_cast<juce::uint16>(input->readShort())==0xffff && static_cast<juce::uint16>(input->readShort())==0xffff
        && input->readInt()==-1 && input->readInt()==-1 && input->readShort()==0, "Unsupported ZIP64 end record.");
    seek(*input,tail-20,20); require(input->readInt()==0x07064b50 && input->readInt()==0, "Missing or split ZIP64 locator.");
    const auto end64=read64(*input); require(input->readInt()==1 && end64==tail-76, "Invalid ZIP64 locator offset.");
    seek(*input,end64,56); require(input->readInt()==0x06064b50 && input->readInt64()==44 && input->readShort()==45 && input->readShort()==45 && input->readInt()==0 && input->readInt()==0, "Unsupported ZIP64 directory record.");
    const auto count=read64(*input), totalCount=read64(*input), directoryBytes=read64(*input), directoryStart=read64(*input);
    require(count>0 && count<=entryLimit && count==totalCount && directoryStart<=end64 && directoryBytes==end64-directoryStart, "Invalid ZIP64 directory bounds/count.");
    seek(*input,directoryStart,directoryBytes);
    struct Header {juce::int64 local; int nameBytes;}; std::vector<Header> headers;
    for(juce::int64 i=0;i<count;++i) {
        seek(*input,input->getPosition(),46);
        require(input->getPosition()<=end64-46 && input->readInt()==0x02014b50 && input->readShort()==45 && input->readShort()==45 && input->readShort()==0x0808 && input->readShort()==0, "Unsupported ZIP64 entry (stored, unencrypted files only).");
        input->skipNextBytes(4); const auto crc=static_cast<juce::uint32>(input->readInt());
        require(input->readInt()==-1 && input->readInt()==-1, "ZIP64 entry has inconsistent size markers.");
        const auto length=static_cast<juce::uint16>(input->readShort());
        require(input->readShort()==28 && input->readShort()==0 && input->readShort()==0 && input->readShort()==0 && input->readInt()==0 && input->readInt()==-1, "Unsupported ZIP64 entry attributes/extra fields.");
        require(input->getPosition()<=end64-28-length, "Truncated ZIP64 directory entry.");
        const auto name=readName(*input,length); require(input->readShort()==1 && input->readShort()==24, "Missing ZIP64 size/offset extension.");
        const auto bytes=read64(*input), packed=read64(*input), local=read64(*input); require(bytes==packed, "Compressed ZIP64 backups are unsupported.");
        rows.push_back({name,bytes,crc,false}); headers.push_back({local,length});
    }
    require(input->getPosition()==end64, "ZIP64 directory length/count mismatch.");
    juce::int64 nextLocal=0;
    for(size_t i=0;i<rows.size();++i) {
        const auto& entry=rows[i]; const auto header=headers[i]; require(header.local==nextLocal, "Overlapping or displaced ZIP64 entries.");
        seek(*input,header.local,30); require(input->readInt()==0x04034b50 && input->readShort()==45 && input->readShort()==0x0808 && input->readShort()==0, "ZIP64 local header mismatch.");
        input->skipNextBytes(4); require(input->readInt()==0 && input->readInt()==-1 && input->readInt()==-1 && input->readShort()==header.nameBytes && input->readShort()==20, "ZIP64 local sizes/filename mismatch.");
        require(readName(*input,header.nameBytes)==entry.name && input->readShort()==1 && input->readShort()==16 && input->readInt64()==0 && input->readInt64()==0, "ZIP64 local extension mismatch.");
        const auto data=input->getPosition(); require(data<=directoryStart-24 && entry.bytes<=directoryStart-24-data, "ZIP64 payload exceeds its data region."); offsets.push_back(data);
        seek(*input,data+entry.bytes,24); require(input->readInt()==0x08074b50 && static_cast<juce::uint32>(input->readInt())==entry.crc && read64(*input)==entry.bytes && read64(*input)==entry.bytes, "ZIP64 data descriptor mismatch."); nextLocal=input->getPosition();
    }
    require(nextLocal==directoryStart, "ZIP64 data/directory boundary mismatch.");
}
std::unique_ptr<juce::InputStream> Reader::open(int index) const {
    require(index>=0 && static_cast<size_t>(index)<rows.size(), "Invalid backup entry index.");
    if(legacy) return std::unique_ptr<juce::InputStream>(legacy->createStreamForEntry(index));
    auto input=file.createInputStream(); require(input!=nullptr, "Could not open backup data stream.");
    return std::make_unique<juce::SubregionStream>(input.release(),offsets[static_cast<size_t>(index)],rows[static_cast<size_t>(index)].bytes,true);
}
}
