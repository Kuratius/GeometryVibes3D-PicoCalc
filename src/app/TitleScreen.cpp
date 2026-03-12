#include "TitleScreen.hpp"

namespace gv {

static inline uint16_t blend565(uint16_t dst, uint16_t src, uint8_t a) {
    const uint32_t ia = 255u - a;

    const uint32_t dr = (dst >> 11) & 0x1F;
    const uint32_t dg = (dst >> 5)  & 0x3F;
    const uint32_t db =  dst        & 0x1F;

    const uint32_t sr = (src >> 11) & 0x1F;
    const uint32_t sg = (src >> 5)  & 0x3F;
    const uint32_t sb =  src        & 0x1F;

    const uint32_t r = (dr * ia + sr * a + 127u) / 255u;
    const uint32_t g = (dg * ia + sg * a + 127u) / 255u;
    const uint32_t b = (db * ia + sb * a + 127u) / 255u;

    return uint16_t((r << 11) | (g << 5) | b);
}

fx TitleScreen::easeInOutCubic(fx t) {
    // t in [0..1]
    const fx half = fx::half();

    if (t < half) {
        // 4*t^3
        const fx t2 = t * t;
        const fx t3 = t2 * t;
        return fx::fromInt(4) * t3;
    }

    // 1 - pow(-2t + 2, 3)/2
    const fx two = fx::fromInt(2);
    const fx v = two - two * t;      // v in (0..1]
    const fx v2 = v * v;
    const fx v3 = v2 * v;
    return fx::one() - (v3 / two);
}

void TitleScreen::buildAlphaLut() {
    // Build a 0..1..0 ping-pong through easeInOutCubic over the full period.
    for (int i = 0; i < kFramesPerPeriod; ++i) {
        const fx p = fx::fromRatio(i, kFramesPerPeriod - 1); // 0..1
        fx u = p + p; // 0..2

        // Triangle wave 0..1..0
        if (u > fx::one()) {
            u = fx::fromInt(2) - u;
        }

        const fx e = easeInOutCubic(u);
        int a = gv::mulInt(e, 255).roundToInt();
        if (a < 0) a = 0;
        if (a > 255) a = 255;
        alphaLut_[i] = static_cast<uint8_t>(a);
    }

    phaseFrame_ = 0;
}

bool TitleScreen::load(IFileSystem& fs, const char* path) {
    unload();

    if (!path) return false;

    file_ = fs.openRead(path);
    accepted_ = false;
    drawnFull_ = false;

    buildAlphaLut();
    return file_ != nullptr;
}

void TitleScreen::unload() {
    if (file_) {
        file_->close();
        file_ = nullptr;
    }
    accepted_ = false;
    drawnFull_ = false;
    phaseFrame_ = 0;
}

void TitleScreen::update(const InputState& in) {
    if (!ready()) return;
    if (in.thrustPressed || in.confirm) {
        accepted_ = true;
    }
}

void TitleScreen::draw(IDisplay& display) {
    if (!ready()) return;

    // Draw full title image once.
    if (!drawnFull_) {
        if (!file_->seek(0)) return;

        for (int y = 0; y < H; y += SLAB_ROWS) {
            const int rows = ((y + SLAB_ROWS) <= H) ? SLAB_ROWS : (H - y);
            const size_t bytesToRead = size_t(W * rows * sizeof(uint16_t));

            size_t got = 0;
            if (!file_->read(slab_.data(), bytesToRead, got) || got != bytesToRead) {
                return;
            }

            display.drawBitmap565(0, y, W, rows, slab_.data());
        }

        drawnFull_ = true;
    }

    // Pulse alpha (easeInOutCubic over 4s @ 30fps => 120 frames).
    const uint8_t alpha = alphaLut_[phaseFrame_];
    ++phaseFrame_;
    if (phaseFrame_ >= kFramesPerPeriod) phaseFrame_ = 0;

    const int promptW = prompt_.width();
    const int promptH = prompt_.height();
    if (promptW <= 0 || promptH <= 0) return;

    int x0 = (display.width() - promptW) / 2;
    int y0 = display.height() - 24; // center-bottom-ish with margin

    if (x0 < 0) x0 = 0;
    if (x0 > (W - promptW)) x0 = W - promptW;
    if (y0 < 0) y0 = 0;
    if (y0 > (H - promptH)) y0 = H - promptH;

    // Read the background strip (full width, promptH rows) from the title image.
    const size_t stripOffset = size_t(y0) * size_t(W) * sizeof(uint16_t);
    const size_t stripBytes  = size_t(W * promptH * sizeof(uint16_t));

    if (!file_->seek(stripOffset)) return;

    size_t got = 0;
    if (!file_->read(slab_.data(), stripBytes, got) || got != stripBytes) {
        return;
    }

    // Blend prompt text over the strip.
    const uint16_t fg = 0xFFFF; // white
    for (int row = 0; row < promptH; ++row) {
        uint16_t* dstRow = slab_.data() + row * W;
        for (int col = 0; col < promptW; ++col) {
            if (prompt_.testPixel(col, row)) {
                const int x = x0 + col;
                dstRow[x] = blend565(dstRow[x], fg, alpha);
            }
        }
    }

    // Blit the updated strip back to the display.
    display.drawBitmap565(0, y0, W, promptH, slab_.data());
}

} // namespace gv