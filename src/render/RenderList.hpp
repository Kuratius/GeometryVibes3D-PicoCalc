#pragma once

#include "Text.hpp"
#include "core/StaticVector.hpp"
#include <cstddef>
#include <cstdint>

namespace gv {

enum TextStyle : uint8_t {
    None       = 0,
    Inverted   = 1u << 0,
    Background = 1u << 1,
};

struct Line2D {
    int16_t x0, y0, x1, y1;
    uint16_t color565;
};

struct FillRectInst {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;
    uint16_t color565 = 0;
};

struct TextInst {
    const Text* text = nullptr;
    int16_t x = 0;
    int16_t y = 0;
    uint16_t color565 = 0xFFFF;
    uint16_t bgColor565 = 0x0000;
    uint8_t alpha = 255;
    uint8_t styleFlags = TextStyle::None;
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
    static constexpr std::size_t LINE_CAP      = 2048;
    static constexpr std::size_t FILLRECT_CAP  = 128;
    static constexpr std::size_t SPRITE_CAP    = 128;
    static constexpr std::size_t TEXT_CAP      = 64;

    void clear() {
        lines_.clear();
        fillRects_.clear();
        sprites_.clear();
        texts_.clear();
    }

    bool addLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color565) {
        return lines_.push_back(Line2D{ x0, y0, x1, y1, color565 });
    }

    bool addFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565) {
        if (w <= 0 || h <= 0) return false;
        return fillRects_.push_back(FillRectInst{ x, y, w, h, color565 });
    }

    bool addSprite(uint16_t spriteId, int16_t x, int16_t y) {
        return sprites_.push_back(SpriteInst{ spriteId, x, y });
    }

    bool addText(const Text* text,
                 int16_t x,
                 int16_t y,
                 uint16_t color565,
                 uint8_t alpha = 255,
                 bool inverted = false,
                 uint16_t bgColor565 = 0x0000,
                 uint8_t styleFlags = TextStyle::None)
    {
        if (!text) return false;

        if (inverted) {
            styleFlags |= TextStyle::Inverted;
        }

        if (bgColor565 != 0x0000) {
            styleFlags |= TextStyle::Background;
        }

        return texts_.push_back(TextInst{
            text, x, y, color565, bgColor565, alpha, styleFlags
        });
    }

    const gv::StaticVector<Line2D, LINE_CAP>& lines() const { return lines_; }
    const gv::StaticVector<FillRectInst, FILLRECT_CAP>& fillRects() const { return fillRects_; }
    const gv::StaticVector<SpriteInst, SPRITE_CAP>& sprites() const { return sprites_; }
    const gv::StaticVector<TextInst, TEXT_CAP>& texts() const { return texts_; }

private:
    gv::StaticVector<Line2D, LINE_CAP> lines_{};
    gv::StaticVector<FillRectInst, FILLRECT_CAP> fillRects_{};
    gv::StaticVector<SpriteInst, SPRITE_CAP> sprites_{};
    gv::StaticVector<TextInst, TEXT_CAP> texts_{};
};

} // namespace gv