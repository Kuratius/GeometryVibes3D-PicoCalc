#include "TitleState.hpp"
#include "App.hpp"
#include "render/RenderList.hpp"
#include "render/Colors.hpp"
#include "core/Config.hpp"

namespace gv {

namespace {

// Keep the title scratch slab out of the App/TitleState object.
// This avoids bloating App with a large per-instance buffer.
static uint16_t s_titleSlab[320 * 16];

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

} // namespace

fx TitleState::easeInOutCubic(fx t) {
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

void TitleState::buildAlphaLut() {
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

bool TitleState::loadTitleAsset(IFileSystem& fs, const char* path) {
    unloadTitleAsset();

    if (!path) return false;

    file_ = fs.openRead(path);
    drawnFull_ = false;
    assetMissing_ = (file_ == nullptr);

    buildAlphaLut();
    return file_ != nullptr;
}

void TitleState::unloadTitleAsset() {
    if (file_) {
        file_->close();
        file_ = nullptr;
    }

    drawnFull_ = false;
    assetMissing_ = false;
    phaseFrame_ = 0;
}

void TitleState::onEnter(App& app) {
    if (!loadTitleAsset(app.platform().fs(), GV_ASSETS_DIR "/title.rgb565")) {
        app.statusOverlay().addWarning("File missing: title.rgb565");
        app.statusOverlay().addInfo("Is SD card inserted?");
    }
}

void TitleState::onExit(App& app) {
    (void)app;
    unloadTitleAsset();
}

void TitleState::update(App& app, const InputState& in, uint32_t dtUs) {
    (void)dtUs;

    if (in.thrustPressed || in.confirm) {
        app.showHomeMenu();
    }
}

void TitleState::renderDirect(App& app, IDisplay& display) {
    (void)app;

    auto buildPlaceholderSlab = [&](int y, int rows) {
        for (int row = 0; row < rows; ++row) {
            uint16_t* dst = s_titleSlab + row * kAssetW;
            for (int x = 0; x < kAssetW; ++x) {
                const bool checker = (((x >> 4) ^ ((y + row) >> 4)) & 1) != 0;
                dst[x] = checker ? gv::color::White : gv::color::Blue;
            }
        }
    };

    if (!drawnFull_) {
        if (!assetMissing_ && file_) {
            if (!file_->seek(0)) {
                assetMissing_ = true;
            }
        }

        for (int y = 0; y < kAssetH; y += kSlabRows) {
            const int rows = ((y + kSlabRows) <= kAssetH) ? kSlabRows : (kAssetH - y);

            if (!assetMissing_ && file_) {
                const size_t bytesToRead = size_t(kAssetW * rows * sizeof(uint16_t));
                size_t got = 0;

                if (!file_->read(s_titleSlab, bytesToRead, got) || got != bytesToRead) {
                    assetMissing_ = true;
                    buildPlaceholderSlab(y, rows);
                }
            } else {
                buildPlaceholderSlab(y, rows);
            }

            display.drawBitmap565(0, y, kAssetW, rows, s_titleSlab);
        }

        drawnFull_ = true;
    }

    const uint8_t alpha = alphaLut_[phaseFrame_];
    ++phaseFrame_;
    if (phaseFrame_ >= kFramesPerPeriod) {
        phaseFrame_ = 0;
    }

    const int displayW = display.width();
    const int displayH = display.height();

    const int promptW = prompt_.width();
    const int promptH = prompt_.height();
    if (promptW <= 0 || promptH <= 0) return;

    // This overlay path uses the title slab as a temporary strip buffer.
    // Keep it bounded to the slab capacity.
    if (promptW > kAssetW || promptH > kAssetH || promptH > kSlabRows) {
        return;
    }

    int x0 = (displayW - promptW) / 2;
    int y0 = displayH - 24;

    if (x0 < 0) x0 = 0;
    if (x0 > (kAssetW - promptW)) x0 = kAssetW - promptW;
    if (y0 < 0) y0 = 0;
    if (y0 > (kAssetH - promptH)) y0 = kAssetH - promptH;

    bool usingAssetPrompt = false;

    if (!assetMissing_ && file_) {
        const size_t stripOffset = size_t(y0) * size_t(kAssetW) * sizeof(uint16_t);
        const size_t stripBytes  = size_t(kAssetW * promptH * sizeof(uint16_t));

        if (!file_->seek(stripOffset)) {
            assetMissing_ = true;
        } else {
            size_t got = 0;
            if (!file_->read(s_titleSlab, stripBytes, got) || got != stripBytes) {
                assetMissing_ = true;
            } else {
                usingAssetPrompt = true;
            }
        }
    }

    if (!usingAssetPrompt) {
        buildPlaceholderSlab(y0, promptH);
    }

    const uint16_t fg = usingAssetPrompt ? gv::color::White : gv::color::Red;
    for (int row = 0; row < promptH; ++row) {
        uint16_t* dstRow = s_titleSlab + row * kAssetW;
        for (int col = 0; col < promptW; ++col) {
            if (prompt_.testPixel(col, row)) {
                const int x = x0 + col;
                dstRow[x] = blend565(dstRow[x], fg, alpha);
            }
        }
    }

    display.drawBitmap565(0, y0, kAssetW, promptH, s_titleSlab);
}

} // namespace gv