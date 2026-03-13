#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace gv {

class Text {
public:
    static constexpr std::size_t CHAR_CAP = 40;
    static constexpr int GLYPH_W = 8;
    static constexpr int GLYPH_H = 8;
    static constexpr int MAX_W = int(CHAR_CAP) * GLYPH_W;
    static constexpr int MAX_H = GLYPH_H;
    static constexpr std::size_t BIT_CAP = (MAX_W * MAX_H) / 8;

public:
    Text() = default;
    explicit Text(std::string_view s) { setText(s); }
    explicit Text(const char* s) { setText(s); }

    Text(const Text& other) = default;
    Text& operator=(const Text& other) = default;
    Text(Text&& other) noexcept = default;
    Text& operator=(Text&& other) noexcept = default;

    bool setText(std::string_view s);
    bool setText(const char* s);
    void clear();

    const char* c_str() const { return text_.data(); }
    std::size_t length() const { return len_; }
    bool empty() const { return len_ == 0; }

    int width() const { return width_; }
    int height() const { return height_; }

    const std::array<uint8_t, BIT_CAP>& bits() const { return bits_; }
    std::size_t bitByteCount() const { return bitBytesUsed_; }

    bool testPixel(int x, int y) const;

private:
    void rebuild();

    static void setBit(std::array<uint8_t, BIT_CAP>& bits, int x, int y, bool on);

private:
    std::array<char, CHAR_CAP + 1> text_{};
    std::size_t len_ = 0;

    int width_ = 0;
    int height_ = 0;
    std::array<uint8_t, BIT_CAP> bits_{};
    std::size_t bitBytesUsed_ = 0;
};

} // namespace gv