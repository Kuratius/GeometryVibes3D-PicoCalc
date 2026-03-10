#include "Hud.hpp"
#include <cstddef>

namespace gv {

namespace {

static constexpr const char* kControlsHintStart = "START[SPACE]";
static constexpr const char* kEventPrefix = "File loaded: ";
static constexpr const char* kLevelPrefix = "LEVEL ";

static constexpr fx kEventHold = fx::fromInt(2);         // full white for 2.0 s
static constexpr fx kEventFade = fx::fromRatio(3, 2);    // fade over 1.5 s

static const char* baseNameOf(const char* path) {
    if (!path) return "";

    const char* base = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

static bool parseLevelNumber(const char* s, int& outLevel) {
    if (!s) return false;

    for (const char* p = s; *p; ++p) {
        if ((p[0] == 'L' || p[0] == 'l') &&
            (p[1] >= '0' && p[1] <= '9') &&
            (p[2] >= '0' && p[2] <= '9')) {
            outLevel = (p[1] - '0') * 10 + (p[2] - '0');
            return true;
        }
    }

    return false;
}

static std::size_t appendChars(char* dst, std::size_t out, std::size_t cap, const char* src) {
    if (!dst || !src) return out;
    while (*src && out < cap) {
        dst[out++] = *src++;
    }
    return out;
}

static std::size_t appendUInt(char* dst, std::size_t out, std::size_t cap, unsigned value) {
    char tmp[10];
    std::size_t n = 0;

    do {
        tmp[n++] = char('0' + (value % 10));
        value /= 10;
    } while (value && n < sizeof(tmp));

    while (n > 0 && out < cap) {
        dst[out++] = tmp[--n];
    }

    return out;
}

} // namespace

void Hud::clear() {
    controlsHint_.setText(kControlsHintStart);
    levelLabel_.clear();
    progress_.clear();
    eventText_.clear();

    eventAge_ = fx::zero();
    eventVisible_ = false;
}

void Hud::update(fx dt) {
    if (!eventVisible_) return;

    eventAge_ += dt;

    if (eventAge_ >= (kEventHold + kEventFade)) {
        eventVisible_ = false;
    }
}

void Hud::setControlsHint(const char *s)
{
    controlsHint_.setText(s);
}

void Hud::setEvent(const char *path)
{
    const char* base = baseNameOf(path);

    char buf[Text::CHAR_CAP + 1]{};
    std::size_t out = 0;

    out = appendChars(buf, out, Text::CHAR_CAP, kEventPrefix);
    out = appendChars(buf, out, Text::CHAR_CAP, base);
    buf[out] = '\0';

    eventText_.setText(buf);
    eventAge_ = fx::zero();
    eventVisible_ = !eventText_.empty();
}

void Hud::setLevelLabel(const char* path) {
    const char* base = baseNameOf(path);

    char buf[Text::CHAR_CAP + 1]{};
    std::size_t out = 0;

    int levelNum = 0;
    if (parseLevelNumber(base, levelNum)) {
        out = appendChars(buf, out, Text::CHAR_CAP, kLevelPrefix);
        out = appendUInt(buf, out, Text::CHAR_CAP, static_cast<unsigned>(levelNum));
    } else {
        out = appendChars(buf, out, Text::CHAR_CAP, base);
    }

    buf[out] = '\0';
    levelLabel_.setText(buf);
}

void Hud::setProgressPercent(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    char buf[Text::CHAR_CAP + 1]{};
    std::size_t out = 0;

    out = appendUInt(buf, out, Text::CHAR_CAP, static_cast<unsigned>(percent));
    if (out < Text::CHAR_CAP) {
        buf[out++] = '%';
    }
    buf[out] = '\0';

    progress_.setText(buf);
}

uint16_t Hud::eventColor() const {
    if (!eventVisible_) return 0;

    if (eventAge_ <= kEventHold) {
        return 0xFFFF;
    }

    const fx fadeAge = eventAge_ - kEventHold;
    if (fadeAge >= kEventFade) {
        return 0;
    }

    const fx brightness = fx::one() - (fadeAge / kEventFade);
    int v = gv::mulInt(brightness, 255).roundToInt();

    if (v < 0) v = 0;
    if (v > 255) v = 255;

    return gray565FromByte(static_cast<uint8_t>(v));
}

uint16_t Hud::gray565FromByte(uint8_t v) {
    const uint16_t r = uint16_t((uint32_t(v) * 31u + 127u) / 255u);
    const uint16_t g = uint16_t((uint32_t(v) * 63u + 127u) / 255u);
    const uint16_t b = uint16_t((uint32_t(v) * 31u + 127u) / 255u);
    return uint16_t((r << 11) | (g << 5) | b);
}

} // namespace gv