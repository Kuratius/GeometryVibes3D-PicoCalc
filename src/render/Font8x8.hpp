#pragma once

#include <array>
#include <cstdint>

namespace gv::Font8x8 {

using Glyph = std::array<uint8_t, 8>;

inline constexpr uint8_t kFirstChar = 32;
inline constexpr uint8_t kLastChar  = 126;
inline constexpr int kGlyphCount    = kLastChar - kFirstChar + 1;
inline constexpr int kGlyphWidth    = 8;
inline constexpr int kGlyphHeight   = 8;

// Bit 7 is the leftmost pixel of each row.
extern const std::array<Glyph, kGlyphCount> kGlyphs;

// Returns '?' for any codepoint outside the supported ASCII range.
const Glyph& glyph(char c);

} // namespace gv::Font8x8