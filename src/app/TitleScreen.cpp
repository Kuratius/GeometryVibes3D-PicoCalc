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
    const fx half = fx::half();

    if (t < half) {
        const fx t2 = t * t;
        const fx t3 = t2 * t;
        return fx::fromInt(4) * t3;
    }

    const fx two = fx::fromInt(2);
    const fx v = two - two * t;
    const fx v2 = v * v;
    const fx v3 = v2 * v;
    return fx::one() - (v3 / two);
}

void TitleScreen::buildAlphaLut() {
    for (int i = 0; i < kFramesPerPeriod; ++i) {
        const fx p = fx::fromRatio(i, kFramesPerPeriod - 1);
        fx u = p + p;

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

    const int displayW = display.width();
    const int displayH = display.height();

    // This title asset is currently authored as a raw 320x320 RGB565 image.
    // We draw it 1:1 at the top-left.
    if (!drawnFull_) {
        if (!file_->seek(0)) return;

        for (int y = 0; y < kAssetH; y += kSlabRows) {
            const int rows = ((y + kSlabRows) <= kAssetH) ? kSlabRows : (kAssetH - y);
            const size_t bytesToRead = size_t(kAssetW * rows * sizeof(uint16_t));

            size_t got = 0;
            if (!file_->read(slab_.data(), bytesToRead, got) || got != bytesToRead) {
                return;
            }

            display.drawBitmap565(0, y, kAssetW, rows, slab_.data());
        }

        drawnFull_ = true;
    }

    const uint8_t alpha = alphaLut_[phaseFrame_];
    ++phaseFrame_;
    if (phaseFrame_ >= kFramesPerPeriod) phaseFrame_ = 0;

    const int promptW = prompt_.width();
    const int promptH = prompt_.height();
    if (promptW <= 0 || promptH <= 0) return;

    int x0 = (displayW - promptW) / 2;
    int y0 = displayH - 24;

    if (x0 < 0) x0 = 0;
    if (x0 > (kAssetW - promptW)) x0 = kAssetW - promptW;
    if (y0 < 0) y0 = 0;
    if (y0 > (kAssetH - promptH)) y0 = kAssetH - promptH;

    const size_t stripOffset = size_t(y0) * size_t(kAssetW) * sizeof(uint16_t);
    const size_t stripBytes  = size_t(kAssetW * promptH * sizeof(uint16_t));

    if (!file_->seek(stripOffset)) return;

    size_t got = 0;
    if (!file_->read(slab_.data(), stripBytes, got) || got != stripBytes) {
        return;
    }

    const uint16_t fg = 0xFFFF;
    for (int row = 0; row < promptH; ++row) {
        uint16_t* dstRow = slab_.data() + row * kAssetW;
        for (int col = 0; col < promptW; ++col) {
            if (prompt_.testPixel(col, row)) {
                const int x = x0 + col;
                dstRow[x] = blend565(dstRow[x], fg, alpha);
            }
        }
    }

    display.drawBitmap565(0, y0, kAssetW, promptH, slab_.data());
}

} // namespace gv