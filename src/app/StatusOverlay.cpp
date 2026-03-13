#include "StatusOverlay.hpp"

namespace gv {

void StatusOverlay::clear() {
    lines_.clear();
    visible_ = false;
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

    // Drop oldest and shift up.
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
    return addMessage(s, 0xFFFF); // white
}

bool StatusOverlay::addWarning(const char* s) {
    return addMessage(s, 0xFFE0); // yellow
}

bool StatusOverlay::addError(const char* s) {
    return addMessage(s, 0xF800); // red
}

} // namespace gv