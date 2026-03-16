#include "StatusOverlay.hpp"

namespace gv {

void StatusOverlay::clear() {
    lines_.clear();
    footerRightText_.clear();
    footerRightColor565_ = 0xFFFF;
    hasFooterRight_ = false;
}

bool StatusOverlay::addMessage(const char* s, uint16_t color565) {
    if (!s || !*s) return false;

    if (!lines_.full()) {
        if (!lines_.emplace_back()) {
            return false;
        }

        OverlayLine& line = lines_.back();
        line.color565 = color565;
        line.text.clear();
        line.text.setText(s);

        visible_ = true;
        return true;
    }

    for (std::size_t i = 1; i < lines_.size(); ++i) {
        lines_[i - 1] = lines_[i];
    }

    OverlayLine& line = lines_.back();
    line.color565 = color565;
    line.text.clear();
    line.text.setText(s);

    visible_ = true;
    return true;
}

bool StatusOverlay::addInfo(const char* s) {
    return addMessage(s, 0xFFFF);
}

bool StatusOverlay::addWarning(const char* s) {
    return addMessage(s, 0xFFE0);
}

bool StatusOverlay::addError(const char* s) {
    return addMessage(s, 0xF800);
}

void StatusOverlay::setFooterRight(const char* s, uint16_t color565) {
    footerRightText_.clear();
    footerRightColor565_ = color565;

    if (s && *s) {
        footerRightText_.setText(s);
        hasFooterRight_ = true;
    } else {
        hasFooterRight_ = false;
    }
}

void StatusOverlay::clearFooterRight() {
    footerRightText_.clear();
    footerRightColor565_ = 0xFFFF;
    hasFooterRight_ = false;
}

} // namespace gv