#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gv {

class CachedText {
public:
    static constexpr size_t CHAR_CAP = 32;
    static constexpr int GLYPH_W = 8;
    static constexpr int GLYPH_H = 8;
    static constexpr int MAX_W = int(CHAR_CAP) * GLYPH_W;
    static constexpr int MAX_H = GLYPH_H;
    static constexpr size_t BIT_CAP = (MAX_W * MAX_H) / 8;

    CachedText() = default;

    bool setText(std::string_view s);
    bool setText(const char* s);

    void clear();

    // Rebuilds the cached bitmap only if marked dirty.
    void rebuildIfDirty();

    const char* c_str() const { return text_.data(); }
    size_t length() const { return len_; }

    int width() const { return width_; }
    int height() const { return height_; }

    const uint8_t* bits() const { return bits_.data(); }
    size_t bitByteCount() const { return bitBytesUsed_; }

    bool dirty() const { return dirty_; }

    // Returns true when the pixel at (x, y) is set.
    bool testPixel(int x, int y) const;

private:
    void rebuild();

    static void setBit(std::array<uint8_t, BIT_CAP>& bits, int x, int y, bool on);

private:
    std::array<char, CHAR_CAP + 1> text_{};
    size_t len_ = 0;

    int width_ = 0;
    int height_ = 0;

    std::array<uint8_t, BIT_CAP> bits_{};
    size_t bitBytesUsed_ = 0;

    bool dirty_ = true;
};

struct TextInst {
    const CachedText* text = nullptr;
    int16_t x = 0;
    int16_t y = 0;
    uint16_t color565 = 0;
};

} // namespace gv