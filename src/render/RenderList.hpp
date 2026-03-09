#pragma once
#include <cstdint>
#include <vector>

namespace gv {

struct Line2D {
    int16_t x0, y0, x1, y1;
    uint16_t color565;
};

// A sprite "definition" references a sub-rect within a texture.
// These can be stored in ROM and referenced by ID from the per-frame list.
struct SpriteDef {
    uint8_t  texId = 0;
    uint16_t srcX = 0;
    uint16_t srcY = 0;
    uint16_t w = 0;
    uint16_t h = 0;
};

// A sprite instance is what goes into the per-frame render list.
struct SpriteInst {
    uint16_t spriteId = 0;  // index into spriteDefs
    int16_t  x = 0;         // screen-space destination
    int16_t  y = 0;         // screen-space destination
};

class RenderList {
public:
    void clear() { lines_.clear(); }

    void addLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c565) {
        lines_.push_back(Line2D{ x0, y0, x1, y1, c565 });
    }

    const std::vector<Line2D>& lines() const { return lines_; }

private:
    std::vector<Line2D> lines_;
    std::vector<SpriteInst> sprites_;
};

} // namespace gv