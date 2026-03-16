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

    void setFooterRight(const char* s, uint16_t color565 = 0xFFFF);
    void clearFooterRight();

    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    void toggleVisible() { visible_ = !visible_; }

    bool visible() const { return visible_ && (!lines_.empty() || hasFooterRight_); }
    bool empty() const { return lines_.empty() && !hasFooterRight_; }

    const gv::StaticVector<OverlayLine, LINE_CAP>& lines() const { return lines_; }

    const Text& footerRightText() const { return footerRightText_; }
    uint16_t footerRightColor() const { return footerRightColor565_; }
    bool hasFooterRight() const { return hasFooterRight_; }

private:
    gv::StaticVector<OverlayLine, LINE_CAP> lines_{};
    Text footerRightText_{};
    uint16_t footerRightColor565_ = 0xFFFF;
    bool hasFooterRight_ = false;
    bool visible_ = false;
};

} // namespace gv