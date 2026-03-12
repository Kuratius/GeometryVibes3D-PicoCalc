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

struct FillRectInst {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;
    uint16_t color565 = 0;
};

struct RenderList {
    static constexpr std::size_t LINE_CAP      = 2048;
    static constexpr std::size_t FILLRECT_CAP  = 128;
    static constexpr std::size_t SPRITE_CAP    = 128;
    static constexpr std::size_t TEXT_CAP      = 32;

    void clear() {
        lines_.clear();
        fillRects_.clear();
        texts_.clear();
    }

    bool addLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color565) {
        return lines_.push_back(Line2D{ x0, y0, x1, y1, color565 });
    }

    bool addFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color565) {
        if (w <= 0 || h <= 0) return false;
        return fillRects_.push_back(FillRectInst{ x, y, w, h, color565 });
    }

    bool addText(const Text* text, int16_t x, int16_t y, uint16_t color565, uint8_t alpha = 255, bool inverted = false) {
        return text && texts_.push_back(TextInst{ text, x, y, color565, alpha, inverted });
    }

    const gv::StaticVector<Line2D, LINE_CAP>& lines() const { return lines_; }
    const gv::StaticVector<FillRectInst, FILLRECT_CAP>& fillRects() const { return fillRects_; }
    const gv::StaticVector<TextInst, TEXT_CAP>& texts() const { return texts_; }

private:
    gv::StaticVector<Line2D, LINE_CAP> lines_{};
    gv::StaticVector<FillRectInst, FILLRECT_CAP> fillRects_{};
    gv::StaticVector<TextInst, TEXT_CAP> texts_{};
};

} // namespace gv