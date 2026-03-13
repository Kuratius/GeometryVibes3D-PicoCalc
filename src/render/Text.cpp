#include "Text.hpp"
#include "Font8x8.hpp"

namespace gv {

bool Text::setText(std::string_view s) {
    if (s.size() > CHAR_CAP) {
        s = s.substr(0, CHAR_CAP);
    }

    bool changed = (s.size() != len_);
    if (!changed) {
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (text_[i] != s[i]) {
                changed = true;
                break;
            }
        }
    }

    if (!changed) {
        return false;
    }

    len_ = s.size();
    for (std::size_t i = 0; i < len_; ++i) {
        text_[i] = s[i];
    }
    text_[len_] = '\0';

    rebuild();
    return true;
}

bool Text::setText(const char* s) {
    if (!s) {
        const bool wasNonEmpty = (len_ != 0);
        clear();
        return wasNonEmpty;
    }
    return setText(std::string_view{s});
}

void Text::clear() {
    text_[0] = '\0';
    len_ = 0;
    width_ = 0;
    height_ = 0;
    bitBytesUsed_ = 0;
    bits_.fill(0);
}

bool Text::testPixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return false;

    const int bitIndex = y * MAX_W + x;
    const int byteIndex = bitIndex >> 3;
    const int bitInByte = 7 - (bitIndex & 7);

    return (bits_[byteIndex] & (1u << bitInByte)) != 0;
}

void Text::setBit(std::array<uint8_t, BIT_CAP>& bits, int x, int y, bool on) {
    const int bitIndex = y * MAX_W + x;
    const int byteIndex = bitIndex >> 3;
    const int bitInByte = 7 - (bitIndex & 7);
    const uint8_t mask = uint8_t(1u << bitInByte);

    if (on) {
        bits[byteIndex] |= mask;
    } else {
        bits[byteIndex] &= uint8_t(~mask);
    }
}

void Text::rebuild() {
    bits_.fill(0);

    width_ = int(len_) * GLYPH_W;
    height_ = (len_ > 0) ? GLYPH_H : 0;
    bitBytesUsed_ = (std::size_t(MAX_W) * std::size_t(height_) + 7u) / 8u;

    for (std::size_t i = 0; i < len_; ++i) {
        const auto& glyph = Font8x8::glyph(text_[i]);
        const int xBase = int(i) * GLYPH_W;

        for (int row = 0; row < GLYPH_H; ++row) {
            const uint8_t rowBits = glyph[row];
            for (int col = 0; col < GLYPH_W; ++col) {
                const bool on = (rowBits & (0x80u >> col)) != 0;
                if (on) {
                    setBit(bits_, xBase + col, row, true);
                }
            }
        }
    }
}

} // namespace gv