#pragma once

#include "render/Text.hpp"
#include "render/Fixed.hpp"
#include <cstdint>

namespace gv {

class Hud {
public:
    void clear();
    void update(fx dt);

    void setEvent(const char* path);
    void setLevelLabel(const char* path);

    const Text& controlsHint() const { return controlsHint_; }
    const Text& levelLabel()   const { return levelLabel_; }
    const Text& progress()     const { return progress_; }
    const Text& eventText()    const { return eventText_; }

    bool eventVisible() const { return eventVisible_; }
    uint16_t eventColor() const;

private:
    static uint16_t gray565FromByte(uint8_t v);

private:
    Text controlsHint_{};
    Text levelLabel_{};
    Text progress_{};
    Text eventText_{};

    fx eventAge_{};
    bool eventVisible_ = false;
};

} // namespace gv