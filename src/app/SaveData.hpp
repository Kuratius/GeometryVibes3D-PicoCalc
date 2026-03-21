#pragma once

#include "core/StaticVector.hpp"
#include "platform/IFileSystem.hpp"
#include "game/Difficulty.hpp"
#include <cstddef>
#include <cstdint>

namespace gv {

class SaveData {
public:
    static constexpr std::size_t kMaxEntries = 10;
    static constexpr std::size_t kLevelCount = 10;
    static constexpr std::size_t kNameBytes = 16;
    static constexpr uint8_t kNoSelection = 0xFF;

private:
    struct DifficultyProgress {
        uint8_t unlockedCount = 1;
        uint8_t percentComplete[kLevelCount]{};
        uint8_t stars[kLevelCount]{};
    };

    struct SaveEntry {
        char name[kNameBytes]{};
        uint8_t selectedDifficulty = uint8_t(Difficulty::Rookie);
        DifficultyProgress progress[kDifficultyCount]{};
    };

public:
    bool load(IFileSystem& fs);
    bool save(IFileSystem& fs) const;

    void clear();

    std::size_t entryCount() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }
    bool full() const { return entries_.full(); }

    uint8_t lastPlayedIndex() const { return lastPlayedIndex_; }
    bool hasLastPlayed() const {
        return lastPlayedIndex_ != kNoSelection && lastPlayedIndex_ < entries_.size();
    }

    bool createEntry(const char* name, uint8_t& outIndex);
    bool deleteEntry(std::size_t index);

    bool setLastPlayedIndex(uint8_t index);
    void clearLastPlayed();

    const char* entryName(std::size_t index) const;
    bool renameEntry(std::size_t index, const char* name);

    Difficulty selectedDifficulty(std::size_t index) const;
    bool setSelectedDifficulty(std::size_t index, Difficulty difficulty);

    uint8_t unlockedCount(std::size_t index, Difficulty difficulty) const;
    bool setUnlockedCount(std::size_t index, Difficulty difficulty, uint8_t unlockedCount);

    uint8_t levelPercent(std::size_t index, Difficulty difficulty, std::size_t levelIndex) const;
    bool setLevelPercent(std::size_t index, Difficulty difficulty, std::size_t levelIndex, uint8_t percent);

    uint8_t levelStars(std::size_t index, Difficulty difficulty, std::size_t levelIndex) const;
    bool setLevelStars(std::size_t index, Difficulty difficulty, std::size_t levelIndex, uint8_t stars);
    bool hasLevelStar(std::size_t index, Difficulty difficulty, std::size_t levelIndex, uint8_t starBit) const;
    bool collectLevelStar(std::size_t index, Difficulty difficulty, std::size_t levelIndex, uint8_t starBit);

private:
    static void sanitizeEntry(SaveEntry& e);
    static void normalizeName(char (&dst)[kNameBytes], const char* src);

    static uint8_t clampDifficultyIndex(uint8_t value);

    const SaveEntry* entryAt(std::size_t i) const;
    SaveEntry* entryAt(std::size_t i);

    const DifficultyProgress* progressAt(std::size_t index, Difficulty difficulty) const;
    DifficultyProgress* progressAt(std::size_t index, Difficulty difficulty);

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
    static constexpr uint8_t kVersion = 2;
    static constexpr const char* kSavePath = "saves/gv3d.sav";

private:
    StaticVector<SaveEntry, kMaxEntries> entries_{};
    uint8_t lastPlayedIndex_ = kNoSelection;
};

} // namespace gv