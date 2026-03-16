#include "Ili9488Display.hpp"
#include "core/Fixed.hpp"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include <cstdio>
#include <cstring>

namespace gv {

static uint g_baud = 0;
static int  g_dma_tx = -1;
static dma_channel_config g_dma_cfg;

static constexpr uint32_t TAG_FRAME = 0xF00D0000;
static constexpr uint32_t TAG_DONE  = 0xD00E0000;

static uint16_t s_slabBuf[2][Ili9488Display::W * Ili9488Display::SLAB_ROWS];

Ili9488Display::Frame Ili9488Display::s_frame[2];
volatile bool Ili9488Display::s_slotReady[2] = {false, false};
volatile int  Ili9488Display::s_prod = 0;
Ili9488Display* Ili9488Display::s_active = nullptr;

Ili9488Display::Ili9488Display() {}
Ili9488Display::~Ili9488Display() {}

static inline void start_dma_slab(const void* src, int pixelWords) {
    dma_channel_configure(
        g_dma_tx,
        &g_dma_cfg,
        &spi_get_hw(spi1)->dr,
        src,
        pixelWords,
        true
    );
}

static inline void wait_for_spi_dma_idle() {
    dma_channel_wait_for_finish_blocking(g_dma_tx);
    while (spi_get_hw(spi1)->sr & SPI_SSPSR_BSY_BITS) {
        tight_loop_contents();
    }
}

static inline int outcode(int x, int y, int xmin, int ymin, int xmax, int ymax) {
    int c = 0;
    if (x < xmin) c |= 1; else if (x > xmax) c |= 2;
    if (y < ymin) c |= 4; else if (y > ymax) c |= 8;
    return c;
}

static inline int iabs_i32(int v) {
    return (v < 0) ? -v : v;
}

static inline int min_i32(int a, int b) {
    return (a < b) ? a : b;
}

static inline int max_i32(int a, int b) {
    return (a > b) ? a : b;
}

static inline int floorFxToInt(gv::fx a) {
    return a.raw() >> gv::fx::SHIFT;
}

static inline int ceilFxToInt(gv::fx a) {
    const int32_t raw = a.raw();
    const int32_t mask = (1 << gv::fx::SHIFT) - 1;
    if ((raw & mask) == 0) return raw >> gv::fx::SHIFT;
    return (raw >= 0) ? ((raw >> gv::fx::SHIFT) + 1) : (raw >> gv::fx::SHIFT);
}

static inline uint16_t scale565(uint16_t c, uint8_t a) {
    const uint32_t r = (c >> 11) & 0x1F;
    const uint32_t g = (c >> 5)  & 0x3F;
    const uint32_t b = c & 0x1F;

    const uint32_t rs = (r * a + 127) / 255;
    const uint32_t gs = (g * a + 127) / 255;
    const uint32_t bs = (b * a + 127) / 255;

    return uint16_t((rs << 11) | (gs << 5) | bs);
}

void Ili9488Display::beginFrame() {
    initIfNeeded();

    while (multicore_fifo_rvalid()) {
        (void)multicore_fifo_pop_blocking();
    }
}

void Ili9488Display::draw(const RenderList& rl) {
    initIfNeeded();

    Frame& f = s_frame[(int)s_prod];
    f.lineCount = 0;
    f.fillRectCount = 0;
    f.textCount = 0;

    for (const auto& ln : rl.lines()) {
        if (f.lineCount >= MAX_LINES) break;

        int x0 = ln.x0;
        int y0 = ln.y0;
        int x1 = ln.x1;
        int y1 = ln.y1;

        if (!clipLineToRect(x0, y0, x1, y1, 0, 0, W - 1, H - 1))
            continue;

        Line& out = f.lines[f.lineCount++];
        out.x0 = (int16_t)x0;
        out.y0 = (int16_t)y0;
        out.x1 = (int16_t)x1;
        out.y1 = (int16_t)y1;
        out.c565 = ln.color565;
    }

    for (const auto& fr : rl.fillRects()) {
        if (f.fillRectCount >= MAX_FILLRECTS) break;
        f.fillRects[f.fillRectCount++] = fr;
    }

    for (const auto& txt : rl.texts()) {
        if (f.textCount >= MAX_TEXTS) break;
        if (!txt.text) continue;
        if (txt.text->empty()) continue;
        f.texts[f.textCount++] = txt;
    }

    binFrameLines(f);
    binFrameFillRects(f);
    binFrameTexts(f);
    #ifdef GV3D_TESTING
    lastLines  = f.lineCount;
    lastBinned = f.lineBinnedTotal;
    lastTexts  = f.textCount;
    #endif
}

void Ili9488Display::drawBitmap565(int x, int y, int w, int h, const uint16_t* pixels) {
    initIfNeeded();

    if (!pixels || w <= 0 || h <= 0) return;

    const int dstX0 = x;
    const int dstY0 = y;
    const int dstX1 = x + w - 1;
    const int dstY1 = y + h - 1;

    if (dstX1 < 0 || dstX0 >= W || dstY1 < 0 || dstY0 >= H) {
        return;
    }

    const int clipX0 = (dstX0 < 0) ? 0 : dstX0;
    const int clipY0 = (dstY0 < 0) ? 0 : dstY0;
    const int clipX1 = (dstX1 >= W) ? (W - 1) : dstX1;
    const int clipY1 = (dstY1 >= H) ? (H - 1) : dstY1;

    const int clipW = clipX1 - clipX0 + 1;
    const int clipH = clipY1 - clipY0 + 1;

    const int srcX0 = clipX0 - dstX0;
    const int srcY0 = clipY0 - dstY0;

    setAddrWindow(clipX0, clipY0, clipX1, clipY1);

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Fast path: no horizontal cropping, source rows contiguous.
    if (srcX0 == 0 && clipW == w) {
        const uint16_t* src = pixels + srcY0 * w;
        start_dma_slab(src, clipW * clipH);
        wait_for_spi_dma_idle();
    } else {
        // Generic clipped path: send one row at a time.
        for (int row = 0; row < clipH; ++row) {
            const uint16_t* src = pixels + (srcY0 + row) * w + srcX0;
            start_dma_slab(src, clipW);
            wait_for_spi_dma_idle();
        }
    }

    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(PIN_CS, 1);
}

void Ili9488Display::endFrame() {
    if (!inited) return;

    const int slot = (int)s_prod;

    __dmb();
    s_slotReady[slot] = true;
    __dmb();
    multicore_fifo_push_blocking(TAG_FRAME | (uint32_t)slot);

    s_prod ^= 1;

    while (s_slotReady[(int)s_prod]) {
        const uint32_t msg = multicore_fifo_pop_blocking();
        if ((msg & 0xFFFF0000u) == TAG_DONE) {
        }
    }

    #ifdef GV3D_TESTING
    static uint32_t frames = 0;
    static uint64_t t0 = 0;
    if (t0 == 0) t0 = time_us_64();
    ++frames;

    const uint64_t now = time_us_64();
    if (now - t0 >= 1000000) {
        std::printf("SPI:%u FPS:%u Lines:%d Binned:%d\n",
                    g_baud, frames, lastLines, lastBinned);
        frames = 0;
        t0 = now;
    }
    #endif
}

void Ili9488Display::lcdFillBlack() {
    setAddrWindow(0, 0, W - 1, H - 1);

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);

    spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    std::memset(s_slabBuf[0], 0, sizeof(s_slabBuf[0]));

    for (int slab = 0; slab < NUM_SLABS; ++slab) {
        const int y0 = slab * SLAB_ROWS;
        int y1 = y0 + SLAB_ROWS - 1;
        if (y1 >= H) y1 = H - 1;
        const int rows = y1 - y0 + 1;

        start_dma_slab(s_slabBuf[0], W * rows);
        wait_for_spi_dma_idle();
    }

    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(PIN_CS, 1);
}

void Ili9488Display::initIfNeeded() {
    if (inited) return;

    g_baud = spi_init(spi1, SPI_BAUD_HZ);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS, GPIO_OUT);  gpio_put(PIN_CS, 1);
    gpio_init(PIN_DC);  gpio_set_dir(PIN_DC, GPIO_OUT);  gpio_put(PIN_DC, 1);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT); gpio_put(PIN_RST, 1);

    lcdReset();
    lcdInit();

    g_dma_tx = dma_claim_unused_channel(true);
    g_dma_cfg = dma_channel_get_default_config(g_dma_tx);
    channel_config_set_transfer_data_size(&g_dma_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&g_dma_cfg, true);
    channel_config_set_write_increment(&g_dma_cfg, false);
    channel_config_set_dreq(&g_dma_cfg, spi_get_dreq(spi1, true));

    s_active = this;

    lcdFillBlack();

    s_slotReady[0] = false;
    s_slotReady[1] = false;
    s_prod = 0;

    multicore_launch_core1(core1_entry);

    inited = true;
}

void Ili9488Display::lcdReset() {
    gpio_put(PIN_RST, 0);
    sleep_ms(20);
    gpio_put(PIN_RST, 1);
    sleep_ms(120);
}

void Ili9488Display::writeCmd(uint8_t cmd) {
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(spi1, &cmd, 1);
    gpio_put(PIN_CS, 1);
    gpio_put(PIN_DC, 1);
}

void Ili9488Display::writeData(const uint8_t* data, size_t n) {
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(spi1, data, (int)n);
    gpio_put(PIN_CS, 1);
}

void Ili9488Display::writeDataByte(uint8_t b) {
    writeData(&b, 1);
}

void Ili9488Display::lcdInit() {
    writeCmd(0x01);
    sleep_ms(150);

    writeCmd(0x11);
    sleep_ms(120);

    writeCmd(0x21);

    writeCmd(0x3A);
    writeDataByte(0x55);

    writeCmd(0x36);
    writeDataByte(0x48);

    writeCmd(0x29);
}

void Ili9488Display::setAddrWindow(int x0, int y0, int x1, int y1) {
    writeCmd(0x2A);
    const uint8_t col[4] = {
        uint8_t(x0 >> 8), uint8_t(x0 & 0xFF),
        uint8_t(x1 >> 8), uint8_t(x1 & 0xFF)
    };
    writeData(col, 4);

    writeCmd(0x2B);
    const uint8_t row[4] = {
        uint8_t(y0 >> 8), uint8_t(y0 & 0xFF),
        uint8_t(y1 >> 8), uint8_t(y1 & 0xFF)
    };
    writeData(row, 4);

    writeCmd(0x2C);
}

bool Ili9488Display::clipLineToRect(int& x0, int& y0, int& x1, int& y1,
                                    int xmin, int ymin, int xmax, int ymax) {
    int c0 = outcode(x0, y0, xmin, ymin, xmax, ymax);
    int c1 = outcode(x1, y1, xmin, ymin, xmax, ymax);

    while (true) {
        if (!(c0 | c1)) return true;
        if (c0 & c1) return false;

        const int cx = c0 ? c0 : c1;
        const int dx = x1 - x0;
        const int dy = y1 - y0;

        int x = 0;
        int y = 0;

        if (cx & 8) {
            if (dy == 0) return false;
            y = ymax;
            x = x0 + (int)((int64_t)dx * (ymax - y0) / dy);
        } else if (cx & 4) {
            if (dy == 0) return false;
            y = ymin;
            x = x0 + (int)((int64_t)dx * (ymin - y0) / dy);
        } else if (cx & 2) {
            if (dx == 0) return false;
            x = xmax;
            y = y0 + (int)((int64_t)dy * (xmax - x0) / dx);
        } else {
            if (dx == 0) return false;
            x = xmin;
            y = y0 + (int)((int64_t)dy * (xmin - x0) / dx);
        }

        if (cx == c0) {
            x0 = x;
            y0 = y;
            c0 = outcode(x0, y0, xmin, ymin, xmax, ymax);
        } else {
            x1 = x;
            y1 = y;
            c1 = outcode(x1, y1, xmin, ymin, xmax, ymax);
        }
    }
}

void Ili9488Display::binFrameLines(Frame& f) {
    std::memset(f.lineSlabCount, 0, sizeof(f.lineSlabCount));
    f.lineBinnedTotal = 0;

    const int n = f.lineCount;

    for (int i = 0; i < n; ++i) {
        int y0 = f.lines[i].y0;
        int y1 = f.lines[i].y1;
        if (y0 > y1) {
            const int t = y0;
            y0 = y1;
            y1 = t;
        }

        if (y1 < 0 || y0 >= H) continue;
        if (y0 < 0) y0 = 0;
        if (y1 >= H) y1 = H - 1;

        const int s0 = y0 / SLAB_ROWS;
        const int s1 = y1 / SLAB_ROWS;

        for (int s = s0; s <= s1; ++s) {
            if (f.lineSlabCount[s] != 0xFFFF) {
                ++f.lineSlabCount[s];
            }
        }
    }

    f.lineSlabOffset[0] = 0;
    for (int s = 0; s < NUM_SLABS; ++s) {
        int next = (int)f.lineSlabOffset[s] + (int)f.lineSlabCount[s];
        if (next > MAX_LINE_BINNED) next = MAX_LINE_BINNED;
        f.lineSlabOffset[s + 1] = (uint16_t)next;
    }
    f.lineBinnedTotal = f.lineSlabOffset[NUM_SLABS];

    for (int s = 0; s < NUM_SLABS; ++s) {
        f.lineSlabCursor[s] = f.lineSlabOffset[s];
    }

    for (int i = 0; i < n; ++i) {
        int y0 = f.lines[i].y0;
        int y1 = f.lines[i].y1;
        if (y0 > y1) {
            const int t = y0;
            y0 = y1;
            y1 = t;
        }

        if (y1 < 0 || y0 >= H) continue;
        if (y0 < 0) y0 = 0;
        if (y1 >= H) y1 = H - 1;

        const int s0 = y0 / SLAB_ROWS;
        const int s1 = y1 / SLAB_ROWS;

        for (int s = s0; s <= s1; ++s) {
            const uint16_t idx = f.lineSlabCursor[s];
            if (idx < f.lineSlabOffset[s + 1] && idx < MAX_LINE_BINNED) {
                f.lineSlabIndices[idx] = (uint16_t)i;
                f.lineSlabCursor[s] = (uint16_t)(idx + 1);
            }
        }
    }
}

void Ili9488Display::binFrameFillRects(Frame& f) {
    std::memset(f.fillRectSlabCount, 0, sizeof(f.fillRectSlabCount));
    f.fillRectBinnedTotal = 0;

    for (int i = 0; i < f.fillRectCount; ++i) {
        const FillRectInst& r = f.fillRects[i];

        const int x0 = r.x;
        const int y0 = r.y;
        const int x1 = x0 + r.w - 1;
        const int y1 = y0 + r.h - 1;

        if (x1 < 0 || x0 >= W || y1 < 0 || y0 >= H) continue;

        int cy0 = y0;
        int cy1 = y1;
        if (cy0 < 0) cy0 = 0;
        if (cy1 >= H) cy1 = H - 1;

        const int s0 = cy0 / SLAB_ROWS;
        const int s1 = cy1 / SLAB_ROWS;

        for (int s = s0; s <= s1; ++s) {
            if (f.fillRectSlabCount[s] != 0xFFFF) {
                ++f.fillRectSlabCount[s];
            }
        }
    }

    f.fillRectSlabOffset[0] = 0;
    for (int s = 0; s < NUM_SLABS; ++s) {
        int next = (int)f.fillRectSlabOffset[s] + (int)f.fillRectSlabCount[s];
        if (next > MAX_FILLRECT_BINNED) next = MAX_FILLRECT_BINNED;
        f.fillRectSlabOffset[s + 1] = (uint16_t)next;
    }
    f.fillRectBinnedTotal = f.fillRectSlabOffset[NUM_SLABS];

    for (int s = 0; s < NUM_SLABS; ++s) {
        f.fillRectSlabCursor[s] = f.fillRectSlabOffset[s];
    }

    for (int i = 0; i < f.fillRectCount; ++i) {
        const FillRectInst& r = f.fillRects[i];

        const int x0 = r.x;
        const int y0 = r.y;
        const int x1 = x0 + r.w - 1;
        const int y1 = y0 + r.h - 1;

        if (x1 < 0 || x0 >= W || y1 < 0 || y0 >= H) continue;

        int cy0 = y0;
        int cy1 = y1;
        if (cy0 < 0) cy0 = 0;
        if (cy1 >= H) cy1 = H - 1;

        const int s0 = cy0 / SLAB_ROWS;
        const int s1 = cy1 / SLAB_ROWS;

        for (int s = s0; s <= s1; ++s) {
            const uint16_t idx = f.fillRectSlabCursor[s];
            if (idx < f.fillRectSlabOffset[s + 1] && idx < MAX_FILLRECT_BINNED) {
                f.fillRectSlabIndices[idx] = (uint16_t)i;
                f.fillRectSlabCursor[s] = (uint16_t)(idx + 1);
            }
        }
    }
}

void Ili9488Display::binFrameTexts(Frame& f) {
    std::memset(f.textSlabCount, 0, sizeof(f.textSlabCount));
    f.textBinnedTotal = 0;

    for (int i = 0; i < f.textCount; ++i) {
        const TextInst& t = f.texts[i];
        if (!t.text) continue;

        const int w = t.text->width();
        const int h = t.text->height();
        if (w <= 0 || h <= 0) continue;

        const int x0 = t.x;
        const int y0 = t.y;
        const int x1 = x0 + w - 1;
        const int y1 = y0 + h - 1;

        if (x1 < 0 || x0 >= W || y1 < 0 || y0 >= H) continue;

        int cy0 = y0;
        int cy1 = y1;
        if (cy0 < 0) cy0 = 0;
        if (cy1 >= H) cy1 = H - 1;

        const int s0 = cy0 / SLAB_ROWS;
        const int s1 = cy1 / SLAB_ROWS;

        for (int s = s0; s <= s1; ++s) {
            if (f.textSlabCount[s] != 0xFFFF) {
                ++f.textSlabCount[s];
            }
        }
    }

    f.textSlabOffset[0] = 0;
    for (int s = 0; s < NUM_SLABS; ++s) {
        int next = (int)f.textSlabOffset[s] + (int)f.textSlabCount[s];
        if (next > MAX_TEXT_BINNED) next = MAX_TEXT_BINNED;
        f.textSlabOffset[s + 1] = (uint16_t)next;
    }
    f.textBinnedTotal = f.textSlabOffset[NUM_SLABS];

    for (int s = 0; s < NUM_SLABS; ++s) {
        f.textSlabCursor[s] = f.textSlabOffset[s];
    }

    for (int i = 0; i < f.textCount; ++i) {
        const TextInst& t = f.texts[i];
        if (!t.text) continue;

        const int w = t.text->width();
        const int h = t.text->height();
        if (w <= 0 || h <= 0) continue;

        const int x0 = t.x;
        const int y0 = t.y;
        const int x1 = x0 + w - 1;
        const int y1 = y0 + h - 1;

        if (x1 < 0 || x0 >= W || y1 < 0 || y0 >= H) continue;

        int cy0 = y0;
        int cy1 = y1;
        if (cy0 < 0) cy0 = 0;
        if (cy1 >= H) cy1 = H - 1;

        const int s0 = cy0 / SLAB_ROWS;
        const int s1 = cy1 / SLAB_ROWS;

        for (int s = s0; s <= s1; ++s) {
            const uint16_t idx = f.textSlabCursor[s];
            if (idx < f.textSlabOffset[s + 1] && idx < MAX_TEXT_BINNED) {
                f.textSlabIndices[idx] = (uint16_t)i;
                f.textSlabCursor[s] = (uint16_t)(idx + 1);
            }
        }
    }
}

inline void Ili9488Display::plotSlab(uint16_t* slab, int x, int yLocal, uint16_t c565) {
    if ((unsigned)x >= (unsigned)W) return;
    if ((unsigned)yLocal >= (unsigned)SLAB_ROWS) return;
    slab[yLocal * W + x] = c565;
}

void Ili9488Display::drawLineIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln) {
    const int dx = iabs_i32((int)ln.x1 - (int)ln.x0);
    const int dy = iabs_i32((int)ln.y1 - (int)ln.y0);

    if (dx >= dy) {
        drawLineXMajorIntoSlab(slab, slabY0, slabY1, ln);
    } else {
        drawLineYMajorIntoSlab(slab, slabY0, slabY1, ln);
    }
}

void Ili9488Display::drawLineXMajorIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln) {
    int x0 = ln.x0;
    int y0 = ln.y0;
    int x1 = ln.x1;
    int y1 = ln.y1;

    // Normalize so x always increases.
    if (x0 > x1) {
        const int tx = x0; x0 = x1; x1 = tx;
        const int ty = y0; y0 = y1; y1 = ty;
    }

    const int dx = x1 - x0;
    const int dy = y1 - y0;

    if (dx == 0) {
        if ((unsigned)x0 < (unsigned)W) {
            const int ys = max_i32(min_i32(y0, y1), slabY0);
            const int ye = min_i32(max_i32(y0, y1), slabY1);
            for (int y = ys; y <= ye; ++y) {
                plotSlab(slab, x0, y - slabY0, ln.c565);
            }
        }
        return;
    }

    const int minY = min_i32(y0, y1);
    const int maxY = max_i32(y0, y1);
    if (maxY < slabY0 || minY > slabY1) return;

    int xStart = x0;
    int xEnd   = x1;

    if (dy == 0) {
        if (y0 < slabY0 || y0 > slabY1) return;
    } else {
        // Keep the slab narrowing idea, but do it with integer math.
        // These match the old half-pixel band intent:
        //   y in [slabY0 - 0.5, slabY1 + 0.5]
        // Multiply through by 2*dy to avoid fixed point.

        const int ady = iabs_i32(dy);

        // Conservative integer bounds. We intentionally pad by 1 pixel
        // on both sides, just like the old code did.
        auto floor_div = [](int64_t n, int64_t d) -> int {
            int64_t q = n / d;
            int64_t r = n % d;
            if (r != 0 && ((r < 0) != (d < 0))) --q;
            return (int)q;
        };

        auto ceil_div = [](int64_t n, int64_t d) -> int {
            int64_t q = n / d;
            int64_t r = n % d;
            if (r != 0 && ((r > 0) == (d > 0))) ++q;
            return (int)q;
        };

        // Solve for x where the ideal line enters/leaves the slab's
        // half-pixel vertical band.
        //
        // y = y0 + dy*(x-x0)/dx
        // 2*(y - y0)*dx = 2*dy*(x - x0)
        //
        // Band:
        //   2*slabY0 - 1 <= 2*y <= 2*slabY1 + 1
        //
        // Rearranged to x bounds.
        const int64_t twoDx = int64_t(dx) * 2;
        const int64_t nLo = int64_t(2 * slabY0 - 1 - 2 * y0) * dx;
        const int64_t nHi = int64_t(2 * slabY1 + 1 - 2 * y0) * dx;
        const int64_t d   = int64_t(2 * dy);

        int xLo, xHi;
        if (dy > 0) {
            xLo = x0 + floor_div(nLo, d);
            xHi = x0 + ceil_div (nHi, d);
        } else {
            xLo = x0 + floor_div(nHi, d);
            xHi = x0 + ceil_div (nLo, d);
        }

        xStart = max_i32(x0, xLo - 1);
        xEnd   = min_i32(x1, xHi + 1);
        if (xStart > xEnd) return;

        (void)ady;
    }

    const int sy = (dy >= 0) ? 1 : -1;
    const int ady = iabs_i32(dy);

    // Advance the global x-major rasterizer from x0 to xStart so this slab
    // continues the exact same line walk the full line would have used.
    int y = y0;
    int err = dx >> 1;

    for (int x = x0; x < xStart; ++x) {
        err -= ady;
        if (err < 0) {
            y += sy;
            err += dx;
        }
    }

    for (int x = xStart; x <= xEnd; ++x) {
        if ((unsigned)(y - slabY0) < (unsigned)SLAB_ROWS) {
            plotSlab(slab, x, y - slabY0, ln.c565);
        }

        err -= ady;
        if (err < 0) {
            y += sy;
            err += dx;
        }
    }
}

void Ili9488Display::drawLineYMajorIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln) {
    int x0 = ln.x0;
    int y0 = ln.y0;
    int x1 = ln.x1;
    int y1 = ln.y1;

    // Normalize so y always increases.
    if (y0 > y1) {
        const int tx = x0; x0 = x1; x1 = tx;
        const int ty = y0; y0 = y1; y1 = ty;
    }

    const int dx = x1 - x0;
    const int dy = y1 - y0;

    if (dy == 0) {
        if (y0 < slabY0 || y0 > slabY1) return;
        const int xs = min_i32(x0, x1);
        const int xe = max_i32(x0, x1);
        for (int x = xs; x <= xe; ++x) {
            plotSlab(slab, x, y0 - slabY0, ln.c565);
        }
        return;
    }

    const int yStart = max_i32(y0, slabY0);
    const int yEnd   = min_i32(y1, slabY1);
    if (yStart > yEnd) return;

    // Optional narrowing in X for the slab. This keeps the spirit of the old
    // code, though for y-major lines the vertical slab clip already does most
    // of the useful narrowing.
    const int minX = min_i32(x0, x1);
    const int maxX = max_i32(x0, x1);
    if (maxX < 0 || minX >= W) return;

    const int sx = (dx >= 0) ? 1 : -1;
    const int adx = iabs_i32(dx);

    // Advance the global y-major rasterizer from y0 up to yStart so this slab
    // matches the full-line rasterization exactly.
    int x = x0;
    int err = dy >> 1;

    for (int y = y0; y < yStart; ++y) {
        err -= adx;
        if (err < 0) {
            x += sx;
            err += dy;
        }
    }

    for (int y = yStart; y <= yEnd; ++y) {
        plotSlab(slab, x, y - slabY0, ln.c565);

        err -= adx;
        if (err < 0) {
            x += sx;
            err += dy;
        }
    }
}

void Ili9488Display::drawFillRectIntoSlab(uint16_t* slab, int slabY0, int slabY1, const FillRectInst& inst) {
    const int dstX0 = inst.x;
    const int dstY0 = inst.y;
    const int dstX1 = dstX0 + inst.w - 1;
    const int dstY1 = dstY0 + inst.h - 1;

    if (dstX1 < 0 || dstX0 >= W || dstY1 < slabY0 || dstY0 > slabY1) {
        return;
    }

    const int xStart = (dstX0 < 0) ? 0 : dstX0;
    const int xEnd   = (dstX1 >= W) ? (W - 1) : dstX1;
    const int yStart = (dstY0 < slabY0) ? slabY0 : dstY0;
    const int yEnd   = (dstY1 > slabY1) ? slabY1 : dstY1;

    const int count = xEnd - xStart + 1;
    if (count <= 0) return;

    for (int y = yStart; y <= yEnd; ++y) {
        uint16_t* dst = slab + (y - slabY0) * W + xStart;
        for (int i = 0; i < count; ++i) {
            dst[i] = inst.color565;
        }
    }
}

static inline uint16_t blend565(uint16_t dst, uint16_t src, uint8_t a) {
    if (a == 255) return src;
    if (a == 0)   return dst;

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

void Ili9488Display::drawTextIntoSlab(uint16_t* slab, int slabY0, int slabY1, const TextInst& inst) {
    const Text* text = inst.text;
    if (!text) return;

    const int textW = text->width();
    const int textH = text->height();
    if (textW <= 0 || textH <= 0) return;

    const uint8_t alpha = inst.alpha;
    if (alpha == 0) return;

    const bool inverted = (inst.styleFlags & TextStyle::Inverted) != 0;
    const bool hasBg    = (inst.styleFlags & TextStyle::Background) != 0;

    const int dstX0 = inst.x;
    const int dstY0 = inst.y;
    const int dstX1 = dstX0 + textW - 1;
    const int dstY1 = dstY0 + textH - 1;

    if (dstX1 < 0 || dstX0 >= W || dstY1 < slabY0 || dstY0 > slabY1) {
        return;
    }

    const int xStart = (dstX0 < 0) ? 0 : dstX0;
    const int xEnd   = (dstX1 >= W) ? (W - 1) : dstX1;
    const int yStart = (dstY0 < slabY0) ? slabY0 : dstY0;
    const int yEnd   = (dstY1 > slabY1) ? slabY1 : dstY1;

    const uint16_t fg = inst.color565;
    const uint16_t bg = inst.bgColor565;

    for (int y = yStart; y <= yEnd; ++y) {
        const int ty = y - dstY0;
        const int yLocal = y - slabY0;
        uint16_t* row = slab + yLocal * W;

        const uint8_t* bits = text->rowBits(ty);
        if (!bits) continue;

        const int tx0 = xStart - dstX0;
        const int tx1 = xEnd   - dstX0;

        if (alpha == 255) {
            if (inverted) {
                for (int tx = tx0, x = xStart; tx <= tx1; ++tx, ++x) {
                    const uint8_t mask = uint8_t(0x80u >> (tx & 7));
                    const bool on = (bits[tx >> 3] & mask) != 0;
                    row[x] = on ? 0x0000 : fg;
                }
            } else if (hasBg) {
                for (int tx = tx0, x = xStart; tx <= tx1; ++tx, ++x) {
                    const uint8_t mask = uint8_t(0x80u >> (tx & 7));
                    const bool on = (bits[tx >> 3] & mask) != 0;
                    row[x] = on ? fg : bg;
                }
            } else {
                for (int tx = tx0, x = xStart; tx <= tx1; ++tx, ++x) {
                    const uint8_t mask = uint8_t(0x80u >> (tx & 7));
                    if (bits[tx >> 3] & mask) {
                        row[x] = fg;
                    }
                }
            }
        } else {
            if (inverted) {
                for (int tx = tx0, x = xStart; tx <= tx1; ++tx, ++x) {
                    const uint8_t mask = uint8_t(0x80u >> (tx & 7));
                    const bool on = (bits[tx >> 3] & mask) != 0;
                    const uint16_t target = on ? 0x0000 : fg;
                    row[x] = blend565(row[x], target, alpha);
                }
            } else if (hasBg) {
                for (int tx = tx0, x = xStart; tx <= tx1; ++tx, ++x) {
                    const uint8_t mask = uint8_t(0x80u >> (tx & 7));
                    const bool on = (bits[tx >> 3] & mask) != 0;
                    row[x] = on
                        ? blend565(row[x], fg, alpha)
                        : blend565(row[x], bg, alpha);
                }
            } else {
                for (int tx = tx0, x = xStart; tx <= tx1; ++tx, ++x) {
                    const uint8_t mask = uint8_t(0x80u >> (tx & 7));
                    if (bits[tx >> 3] & mask) {
                        row[x] = blend565(row[x], fg, alpha);
                    }
                }
            }
        }
    }
}

void Ili9488Display::renderAndFlushFrame(const Frame& f) {
    setAddrWindow(0, 0, W - 1, H - 1);

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    int ping = 0;

    for (int slabIndex = 0; slabIndex < NUM_SLABS; ++slabIndex) {
        const int slabY0 = slabIndex * SLAB_ROWS;
        int slabY1 = slabY0 + SLAB_ROWS - 1;
        if (slabY1 >= H) slabY1 = H - 1;

        const int rows = slabY1 - slabY0 + 1;
        uint16_t* slab = s_slabBuf[ping];

        std::memset(slab, 0, W * rows * sizeof(uint16_t));

        {
            const uint16_t a = f.lineSlabOffset[slabIndex];
            const uint16_t b = f.lineSlabCursor[slabIndex];
            for (uint16_t k = a; k < b; ++k) {
                const Line& ln = f.lines[f.lineSlabIndices[k]];
                drawLineIntoSlab(slab, slabY0, slabY1, ln);
            }
        }

        {
            const uint16_t a = f.fillRectSlabOffset[slabIndex];
            const uint16_t b = f.fillRectSlabCursor[slabIndex];
            for (uint16_t k = a; k < b; ++k) {
                const FillRectInst& fr = f.fillRects[f.fillRectSlabIndices[k]];
                drawFillRectIntoSlab(slab, slabY0, slabY1, fr);
            }
        }

        {
            const uint16_t a = f.textSlabOffset[slabIndex];
            const uint16_t b = f.textSlabCursor[slabIndex];
            for (uint16_t k = a; k < b; ++k) {
                const TextInst& txt = f.texts[f.textSlabIndices[k]];
                drawTextIntoSlab(slab, slabY0, slabY1, txt);
            }
        }

        if (slabIndex != 0) {
            wait_for_spi_dma_idle();
        }

        start_dma_slab(slab, W * rows);
        ping ^= 1;
    }

    wait_for_spi_dma_idle();

    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(PIN_CS, 1);
}

void Ili9488Display::core1_entry() {
    while (true) {
        const uint32_t msg = multicore_fifo_pop_blocking();
        if ((msg & 0xFFFF0000u) != TAG_FRAME) continue;

        const int slot = (int)(msg & 0xFFFFu);
        Ili9488Display* d = s_active;

        if (!d || slot < 0 || slot > 1 || !s_slotReady[slot]) {
            multicore_fifo_push_blocking(TAG_DONE | (uint32_t)slot);
            continue;
        }

        __dmb();
        const Frame& f = s_frame[slot];
        d->renderAndFlushFrame(f);

        __dmb();
        s_slotReady[slot] = false;
        __dmb();

        multicore_fifo_push_blocking(TAG_DONE | (uint32_t)slot);
    }
}

} // namespace gv