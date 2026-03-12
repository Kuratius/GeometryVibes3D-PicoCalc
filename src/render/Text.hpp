#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gv {

class Text {
public:
    static constexpr std::size_t CHAR_CAP = 32;
    static constexpr int GLYPH_W = 8;
    static constexpr int GLYPH_H = 8;
    static constexpr int MAX_W = int(CHAR_CAP) * GLYPH_W;
    static constexpr int MAX_H = GLYPH_H;
    static constexpr std::size_t BIT_CAP = (MAX_W * MAX_H) / 8;

public:
    Text() = default;
    explicit Text(std::string_view s) { setText(s); }
    explicit Text(const char* s) { setText(s); }

    bool setText(std::string_view s);
    bool setText(const char* s);
    void clear();

    const char* c_str() const { return text_.data(); }
    std::size_t length() const { return len_; }
    bool empty() const { return len_ == 0; }

    int width() const;
    int height() const;

    const std::array<uint8_t, BIT_CAP>& bits() const;
    std::size_t bitByteCount() const;

    bool testPixel(int x, int y) const;

private:
    void rebuildIfNeeded() const;

    static void setBit(std::array<uint8_t, BIT_CAP>& bits, int x, int y, bool on);

private:
    std::array<char, CHAR_CAP + 1> text_{};
    std::size_t len_ = 0;

    mutable int width_ = 0;
    mutable int height_ = 0;
    mutable std::array<uint8_t, BIT_CAP> bits_{};
    mutable std::size_t bitBytesUsed_ = 0;
    mutable bool dirty_ = true;
};

struct TextInst {
    const Text* text = nullptr;
    int16_t x = 0;
    int16_t y = 0;
    uint16_t color565 = 0;
    uint8_t alpha = 255;
    bool inverted = false;
};

} // namespace gv