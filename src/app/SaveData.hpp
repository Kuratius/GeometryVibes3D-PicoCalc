#pragma once

#include "core/StaticVector.hpp"
#include "platform/IFileSystem.hpp"
#include <cstddef>
#include <cstdint>

namespace gv {

class SaveData {
public:
    static constexpr std::size_t kMaxEntries = 10;
    static constexpr std::size_t kLevelCount = 10;
    static constexpr std::size_t kNameBytes = 16;
    static constexpr uint8_t kNoSelection = 0xFF;

    struct SaveEntry {
        char name[kNameBytes]{};
        uint8_t unlockedCount = 1;
        uint8_t percentComplete[kLevelCount]{};
        uint8_t stars[kLevelCount]{};
    };

public:
    bool load(IFileSystem& fs);
    bool save(IFileSystem& fs) const;

    void clear();

    std::size_t entryCount() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }
    bool full() const { return entries_.full(); }

    uint8_t lastPlayedIndex() const { return lastPlayedIndex_; }
    bool hasLastPlayed() const { return lastPlayedIndex_ != kNoSelection && lastPlayedIndex_ < entries_.size(); }

    const SaveEntry* entry(std::size_t i) const;
    SaveEntry* entry(std::size_t i);

    bool createEntry(const char* name, uint8_t& outIndex);
    bool deleteEntry(std::size_t index);

    bool setLastPlayedIndex(uint8_t index);
    void clearLastPlayed();

    bool renameEntry(std::size_t index, const char* name);

    bool setUnlockedCount(std::size_t index, uint8_t unlockedCount);
    bool setLevelPercent(std::size_t index, std::size_t levelIndex, uint8_t percent);
    bool setLevelStars(std::size_t index, std::size_t levelIndex, uint8_t stars);

    uint8_t levelStars(std::size_t index, std::size_t levelIndex) const;
    bool hasLevelStar(std::size_t index, std::size_t levelIndex, uint8_t starBit) const;
    bool collectLevelStar(std::size_t index, std::size_t levelIndex, uint8_t starBit);;

    static void sanitizeEntry(SaveEntry& e);
    static void normalizeName(char (&dst)[kNameBytes], const char* src);

private:
    struct FileHeader {
        char magic[3];
        uint8_t version;
        uint8_t entryCount;
        uint8_t lastPlayedIndex;
    };

private:
    static constexpr char kMagic0 = 'G';
    static constexpr char kMagic1 = 'V';
    static constexpr char kMagic2 = 'S';
    static constexpr uint8_t kVersion = 1;
    static constexpr const char* kSavePath = "saves/gv3d.sav";

private:
    StaticVector<SaveEntry, kMaxEntries> entries_{};
    uint8_t lastPlayedIndex_ = kNoSelection;
};

} // namespace gv