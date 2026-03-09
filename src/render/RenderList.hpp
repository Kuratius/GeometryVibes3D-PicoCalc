#pragma once

#include "Text.hpp"
#include "core/StaticVector.hpp"
#include <cstddef>
#include <cstdint>

namespace gv {

struct Line2D {
    int16_t x0, y0, x1, y1;
    uint16_t color565;
};

struct SpriteDef {
    uint8_t  texId = 0;
    uint16_t srcX = 0;
    uint16_t srcY = 0;
    uint16_t w = 0;
    uint16_t h = 0;
};

struct SpriteInst {
    uint16_t spriteId = 0;
    int16_t  x = 0;
    int16_t  y = 0;
};

struct RenderList {
    static constexpr std::size_t LINE_CAP   = 2048;
    static constexpr std::size_t SPRITE_CAP = 128;
    static constexpr std::size_t TEXT_CAP   = 32;

    void clear() {
        lines_.clear();
        sprites_.clear();
        texts_.clear();
    }

    bool addLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color565) {
        return lines_.push_back(Line2D{ x0, y0, x1, y1, color565 });
    }

    bool addSprite(uint16_t spriteId, int16_t x, int16_t y) {
        return sprites_.push_back(SpriteInst{ spriteId, x, y });
    }

    bool addText(const CachedText* text, int16_t x, int16_t y, uint16_t color565) {
        return text && texts_.push_back(TextInst{ text, x, y, color565 });
    }

    const gv::StaticVector<Line2D, LINE_CAP>& lines() const { return lines_; }
    const gv::StaticVector<SpriteInst, SPRITE_CAP>& sprites() const { return sprites_; }
    const gv::StaticVector<TextInst, TEXT_CAP>& texts() const { return texts_; }

private:
    gv::StaticVector<Line2D, LINE_CAP> lines_{};
    gv::StaticVector<SpriteInst, SPRITE_CAP> sprites_{};
    gv::StaticVector<TextInst, TEXT_CAP> texts_{};
};

} // namespace gv