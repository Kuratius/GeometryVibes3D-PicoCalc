#pragma once

#include "render/RenderList.hpp"
#include <cstdint>

namespace gv {

class IDisplay {
public:
    virtual ~IDisplay() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;

    virtual void beginFrame() = 0;
    virtual void draw(const RenderList& rl) = 0;
    virtual void drawBitmap565(int x, int y, int w, int h, const uint16_t* pixels) = 0;
    virtual void endFrame() = 0;
};

} // namespace gv