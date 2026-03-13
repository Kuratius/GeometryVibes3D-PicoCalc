#pragma once

#include "render/Text.hpp"
#include "core/StaticVector.hpp"
#include <cstdint>

namespace gv {

struct OverlayLine {
    Text text{};
    uint16_t color565 = 0xFFFF;
};

class StatusOverlay {
public:
    static constexpr std::size_t LINE_CAP = 8;

public:
    void clear();

    bool addMessage(const char* s, uint16_t color565 = 0xFFFF);
    bool addInfo(const char* s);
    bool addWarning(const char* s);
    bool addError(const char* s);

    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    void toggleVisible() { visible_ = !visible_; }

    bool visible() const { return visible_ && !lines_.empty(); }
    bool empty() const { return lines_.empty(); }

    const gv::StaticVector<OverlayLine, LINE_CAP>& lines() const { return lines_; }

private:
    gv::StaticVector<OverlayLine, LINE_CAP> lines_{};
    bool visible_ = false;
};

} // namespace gv