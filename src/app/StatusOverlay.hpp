#pragma once

#include "render/Text.hpp"
#include "core/StaticVector.hpp"
#include "render/Colors.hpp"
#include <cstdint>

namespace gv {

struct OverlayLine {
    Text text{};
    uint16_t color565 = gv::color::White;
};

class StatusOverlay {
public:
    static constexpr std::size_t LINE_CAP = 8;

public:
    void clear();

    bool addMessage(const char* s, uint16_t color565 = gv::color::White);
    bool addInfo(const char* s);
    bool addWarning(const char* s);
    bool addError(const char* s);

    void setFooterRight(const char* s, uint16_t color565 = gv::color::White);
    void clearFooterRight();

    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    void toggleVisible() { visible_ = !visible_; }

    bool visible() const { return visible_ && (!lines_.empty() || hasFooterRight_); }
    bool empty() const { return lines_.empty() && !hasFooterRight_; }

    const gv::StaticVector<OverlayLine, LINE_CAP>& lines() const { return lines_; }

    const Text& footerRightText() const { return footerRight_.text; }
    uint16_t footerRightColor() const { return footerRight_.color565; }
    bool hasFooterRight() const { return hasFooterRight_; }

private:
    gv::StaticVector<OverlayLine, LINE_CAP> lines_{};
    OverlayLine footerRight_{};
    bool hasFooterRight_ = false;
    bool visible_ = false;
};

} // namespace gv