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

} // namespace

const SaveData::SaveEntry* SaveData::entry(std::size_t i) const {
    return (i < entries_.size()) ? &entries_[i] : nullptr;
}

SaveData::SaveEntry* SaveData::entry(std::size_t i) {
    return (i < entries_.size()) ? &entries_[i] : nullptr;
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
    e.unlockedCount = clampUnlockedCount(e.unlockedCount);

    for (std::size_t i = 0; i < kLevelCount; ++i) {
        e.percentComplete[i] = clampPercent(e.percentComplete[i]);
        // stars are left as raw bytes for now
    }
}

bool SaveData::load(IFileSystem& fs) {
    clear();

    if (!fs.exists(kSavePath)) {
        return true; // no save file yet is not an error
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

    if (ok && hdr.version != kVersion) {
        ok = false;
    }

    const std::size_t count = ok
        ? ((hdr.entryCount <= kMaxEntries) ? hdr.entryCount : kMaxEntries)
        : 0;

    for (std::size_t i = 0; ok && i < count; ++i) {
        SaveEntry e{};
        got = 0;
        if (!f->read(&e, sizeof(e), got) || got != sizeof(e)) {
            ok = false;
            break;
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
    e.unlockedCount = 1;

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

bool SaveData::renameEntry(std::size_t index, const char* name) {
    SaveEntry* e = entry(index);
    if (!e || !name || !*name) {
        return false;
    }

    normalizeName(e->name, name);
    return e->name[0] != '\0';
}

bool SaveData::setUnlockedCount(std::size_t index, uint8_t unlockedCount) {
    SaveEntry* e = entry(index);
    if (!e) {
        return false;
    }

    e->unlockedCount = clampUnlockedCount(unlockedCount);
    return true;
}

bool SaveData::setLevelPercent(std::size_t index, std::size_t levelIndex, uint8_t percent) {
    SaveEntry* e = entry(index);
    if (!e || levelIndex >= kLevelCount) {
        return false;
    }

    e->percentComplete[levelIndex] = clampPercent(percent);
    return true;
}

bool SaveData::setLevelStars(std::size_t index, std::size_t levelIndex, uint8_t stars) {
    SaveEntry* e = entry(index);
    if (!e || levelIndex >= kLevelCount) {
        return false;
    }

    e->stars[levelIndex] = stars;
    return true;
}

} // namespace gv