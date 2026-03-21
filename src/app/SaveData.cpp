#include "SaveData.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gv {

namespace {

static uint8_t clampPercent(uint8_t v) {
    return (v <= 100) ? v : 100;
}

static uint8_t clampUnlockedCount(uint8_t v) {
    if (v < 1) return 1;
    if (v > SaveData::kLevelCount) return uint8_t(SaveData::kLevelCount);
    return v;
}

struct LegacySaveEntryV1 {
    char name[SaveData::kNameBytes]{};
    uint8_t unlockedCount = 1;
    uint8_t percentComplete[SaveData::kLevelCount]{};
    uint8_t stars[SaveData::kLevelCount]{};
};

} // namespace

uint8_t SaveData::clampDifficultyIndex(uint8_t value) {
    return (value < kDifficultyCount) ? value : uint8_t(Difficulty::Rookie);
}

const SaveData::SaveEntry* SaveData::entryAt(std::size_t i) const {
    return (i < entries_.size()) ? &entries_[i] : nullptr;
}

SaveData::SaveEntry* SaveData::entryAt(std::size_t i) {
    return (i < entries_.size()) ? &entries_[i] : nullptr;
}

const SaveData::DifficultyProgress* SaveData::progressAt(std::size_t index, Difficulty difficulty) const {
    const SaveEntry* e = entryAt(index);
    if (!e) return nullptr;
    return &e->progress[difficultyIndex(difficulty)];
}

SaveData::DifficultyProgress* SaveData::progressAt(std::size_t index, Difficulty difficulty) {
    SaveEntry* e = entryAt(index);
    if (!e) return nullptr;
    return &e->progress[difficultyIndex(difficulty)];
}

void SaveData::clear() {
    entries_.clear();
    lastPlayedIndex_ = kNoSelection;
}

void SaveData::normalizeName(char (&dst)[kNameBytes], const char* src) {
    for (std::size_t i = 0; i < kNameBytes; ++i) {
        dst[i] = '\0';
    }

    if (!src) {
        return;
    }

    std::size_t out = 0;
    while (src[out] != '\0' && out + 1 < kNameBytes) {
        dst[out] = src[out];
        ++out;
    }
    dst[out] = '\0';
}

void SaveData::sanitizeEntry(SaveEntry& e) {
    e.name[kNameBytes - 1] = '\0';
    e.selectedDifficulty = clampDifficultyIndex(e.selectedDifficulty);

    for (std::size_t d = 0; d < kDifficultyCount; ++d) {
        DifficultyProgress& p = e.progress[d];
        p.unlockedCount = clampUnlockedCount(p.unlockedCount);

        for (std::size_t i = 0; i < kLevelCount; ++i) {
            p.percentComplete[i] = clampPercent(p.percentComplete[i]);
            p.stars[i] = uint8_t(p.stars[i] & 0x07u);
        }
    }
}

bool SaveData::load(IFileSystem& fs) {
    clear();

    if (!fs.exists(kSavePath)) {
        return true;
    }

    IFile* f = fs.openRead(kSavePath);
    if (!f) {
        return false;
    }

    bool ok = true;

    FileHeader hdr{};
    size_t got = 0;
    if (!f->read(&hdr, sizeof(hdr), got) || got != sizeof(hdr)) {
        ok = false;
    }

    if (ok) {
        if (hdr.magic[0] != kMagic0 ||
            hdr.magic[1] != kMagic1 ||
            hdr.magic[2] != kMagic2) {
            ok = false;
        }
    }

    if (ok && hdr.version != 1 && hdr.version != kVersion) {
        ok = false;
    }

    const std::size_t count = ok
        ? ((hdr.entryCount <= kMaxEntries) ? hdr.entryCount : kMaxEntries)
        : 0;

    for (std::size_t i = 0; ok && i < count; ++i) {
        SaveEntry e{};

        if (hdr.version == 1) {
            LegacySaveEntryV1 old{};
            got = 0;
            if (!f->read(&old, sizeof(old), got) || got != sizeof(old)) {
                ok = false;
                break;
            }

            normalizeName(e.name, old.name);
            e.selectedDifficulty = uint8_t(Difficulty::Rookie);
            e.progress[difficultyIndex(Difficulty::Rookie)].unlockedCount =
                clampUnlockedCount(old.unlockedCount);

            for (std::size_t level = 0; level < kLevelCount; ++level) {
                e.progress[difficultyIndex(Difficulty::Rookie)].percentComplete[level] =
                    clampPercent(old.percentComplete[level]);
                e.progress[difficultyIndex(Difficulty::Rookie)].stars[level] =
                    uint8_t(old.stars[level] & 0x07u);
            }
        } else {
            got = 0;
            if (!f->read(&e, sizeof(e), got) || got != sizeof(e)) {
                ok = false;
                break;
            }
        }

        sanitizeEntry(e);
        if (!entries_.push_back(e)) {
            ok = false;
            break;
        }
    }

    f->close();

    if (!ok) {
        clear();
        return false;
    }

    if (hdr.lastPlayedIndex < entries_.size()) {
        lastPlayedIndex_ = hdr.lastPlayedIndex;
    } else {
        lastPlayedIndex_ = kNoSelection;
    }

    return true;
}

bool SaveData::save(IFileSystem& fs) const {
    IFile* f = fs.openWrite(kSavePath, true);
    if (!f) {
        return false;
    }

    FileHeader hdr{};
    hdr.magic[0] = kMagic0;
    hdr.magic[1] = kMagic1;
    hdr.magic[2] = kMagic2;
    hdr.version = kVersion;
    hdr.entryCount = static_cast<uint8_t>(entries_.size());
    hdr.lastPlayedIndex = (lastPlayedIndex_ < entries_.size()) ? lastPlayedIndex_ : kNoSelection;

    size_t wrote = 0;
    bool ok = f->write(&hdr, sizeof(hdr), wrote) && wrote == sizeof(hdr);

    for (std::size_t i = 0; ok && i < entries_.size(); ++i) {
        wrote = 0;
        ok = f->write(&entries_[i], sizeof(entries_[i]), wrote) && wrote == sizeof(entries_[i]);
    }

    f->close();
    return ok;
}

bool SaveData::createEntry(const char* name, uint8_t& outIndex) {
    outIndex = kNoSelection;

    if (entries_.full()) {
        return false;
    }

    SaveEntry e{};
    normalizeName(e.name, name);
    e.selectedDifficulty = uint8_t(Difficulty::Rookie);

    if (e.name[0] == '\0') {
        return false;
    }

    if (!entries_.push_back(e)) {
        return false;
    }

    outIndex = static_cast<uint8_t>(entries_.size() - 1);
    lastPlayedIndex_ = outIndex;
    return true;
}

bool SaveData::deleteEntry(std::size_t index) {
    if (index >= entries_.size()) {
        return false;
    }

    for (std::size_t i = index + 1; i < entries_.size(); ++i) {
        entries_[i - 1] = entries_[i];
    }
    entries_.pop_back();

    if (entries_.empty()) {
        lastPlayedIndex_ = kNoSelection;
        return true;
    }

    if (lastPlayedIndex_ == kNoSelection) {
        return true;
    }

    if (lastPlayedIndex_ == index) {
        if (index < entries_.size()) {
            lastPlayedIndex_ = static_cast<uint8_t>(index);
        } else {
            lastPlayedIndex_ = static_cast<uint8_t>(entries_.size() - 1);
        }
    } else if (lastPlayedIndex_ > index) {
        --lastPlayedIndex_;
    }

    if (lastPlayedIndex_ >= entries_.size()) {
        lastPlayedIndex_ = static_cast<uint8_t>(entries_.size() - 1);
    }

    return true;
}

bool SaveData::setLastPlayedIndex(uint8_t index) {
    if (index >= entries_.size()) {
        return false;
    }
    lastPlayedIndex_ = index;
    return true;
}

void SaveData::clearLastPlayed() {
    lastPlayedIndex_ = kNoSelection;
}

const char* SaveData::entryName(std::size_t index) const {
    const SaveEntry* e = entryAt(index);
    return e ? e->name : "";
}

bool SaveData::renameEntry(std::size_t index, const char* name) {
    SaveEntry* e = entryAt(index);
    if (!e || !name || !*name) {
        return false;
    }

    normalizeName(e->name, name);
    return e->name[0] != '\0';
}

Difficulty SaveData::selectedDifficulty(std::size_t index) const {
    const SaveEntry* e = entryAt(index);
    if (!e) {
        return Difficulty::Rookie;
    }
    return Difficulty(clampDifficultyIndex(e->selectedDifficulty));
}

bool SaveData::setSelectedDifficulty(std::size_t index, Difficulty difficulty) {
    SaveEntry* e = entryAt(index);
    if (!e) {
        return false;
    }

    e->selectedDifficulty = clampDifficultyIndex(static_cast<uint8_t>(difficulty));
    return true;
}

uint8_t SaveData::unlockedCount(std::size_t index, Difficulty difficulty) const {
    const DifficultyProgress* p = progressAt(index, difficulty);
    return p ? p->unlockedCount : 0;
}

bool SaveData::setUnlockedCount(std::size_t index, Difficulty difficulty, uint8_t unlockedCount) {
    DifficultyProgress* p = progressAt(index, difficulty);
    if (!p) {
        return false;
    }

    p->unlockedCount = clampUnlockedCount(unlockedCount);
    return true;
}

uint8_t SaveData::levelPercent(std::size_t index, Difficulty difficulty, std::size_t levelIndex) const {
    const DifficultyProgress* p = progressAt(index, difficulty);
    if (!p || levelIndex >= kLevelCount) {
        return 0;
    }
    return p->percentComplete[levelIndex];
}

bool SaveData::setLevelPercent(std::size_t index, Difficulty difficulty, std::size_t levelIndex, uint8_t percent) {
    DifficultyProgress* p = progressAt(index, difficulty);
    if (!p || levelIndex >= kLevelCount) {
        return false;
    }

    p->percentComplete[levelIndex] = clampPercent(percent);
    return true;
}

uint8_t SaveData::levelStars(std::size_t index, Difficulty difficulty, std::size_t levelIndex) const {
    const DifficultyProgress* p = progressAt(index, difficulty);
    if (!p || levelIndex >= kLevelCount) {
        return 0;
    }
    return uint8_t(p->stars[levelIndex] & 0x07u);
}

bool SaveData::setLevelStars(std::size_t index, Difficulty difficulty, std::size_t levelIndex, uint8_t stars) {
    DifficultyProgress* p = progressAt(index, difficulty);
    if (!p || levelIndex >= kLevelCount) {
        return false;
    }

    p->stars[levelIndex] = uint8_t(stars & 0x07u);
    return true;
}

bool SaveData::hasLevelStar(std::size_t index, Difficulty difficulty, std::size_t levelIndex, uint8_t starBit) const {
    if (starBit >= 3) {
        return false;
    }

    const uint8_t mask = levelStars(index, difficulty, levelIndex);
    return ((mask >> starBit) & 1u) != 0;
}

bool SaveData::collectLevelStar(std::size_t index, Difficulty difficulty, std::size_t levelIndex, uint8_t starBit) {
    DifficultyProgress* p = progressAt(index, difficulty);
    if (!p || levelIndex >= kLevelCount || starBit >= 3) {
        return false;
    }

    p->stars[levelIndex] = uint8_t((p->stars[levelIndex] | uint8_t(1u << starBit)) & 0x07u);
    return true;
}

} // namespace gv