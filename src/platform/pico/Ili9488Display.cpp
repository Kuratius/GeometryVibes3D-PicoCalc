#include "Ili9488Display.hpp"
#include "render/Fixed.hpp"
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

    for (const auto& ln : rl.lines()) {
        if (f.lineCount >= MAX_LINES) break;

        int x0 = ln.x0;
        int y0 = ln.y0;
        int x1 = ln.x1;
        int y1 = ln.y1;

        // Cull to the screen once so slab rendering only deals with visible spans.
        if (!clipLineToRect(x0, y0, x1, y1, 0, 0, W - 1, H - 1))
            continue;

        Line& out = f.lines[f.lineCount++];
        out.x0 = (int16_t)x0;
        out.y0 = (int16_t)y0;
        out.x1 = (int16_t)x1;
        out.y1 = (int16_t)y1;
        out.c565 = ln.color565;
    }

    binFrameLines(f);

    lastLines  = f.lineCount;
    lastBinned = f.binnedTotal;
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
    std::memset(f.slabCount, 0, sizeof(f.slabCount));
    f.binnedTotal = 0;

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
            if (f.slabCount[s] != 0xFFFF) {
                ++f.slabCount[s];
            }
        }
    }

    f.slabOffset[0] = 0;
    for (int s = 0; s < NUM_SLABS; ++s) {
        int next = (int)f.slabOffset[s] + (int)f.slabCount[s];
        if (next > MAX_BINNED_ENTRIES) next = MAX_BINNED_ENTRIES;
        f.slabOffset[s + 1] = (uint16_t)next;
    }
    f.binnedTotal = f.slabOffset[NUM_SLABS];

    for (int s = 0; s < NUM_SLABS; ++s) {
        f.slabCursor[s] = f.slabOffset[s];
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
            const uint16_t idx = f.slabCursor[s];
            if (idx < f.slabOffset[s + 1] && idx < MAX_BINNED_ENTRIES) {
                f.slabIndices[idx] = (uint16_t)i;
                f.slabCursor[s] = (uint16_t)(idx + 1);
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
    using gv::fx;

    int x0 = ln.x0;
    int y0 = ln.y0;
    int x1 = ln.x1;
    int y1 = ln.y1;

    // Normalize the walk so x always increases.
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

    const fx x0f = fx::fromInt(x0);
    const fx y0f = fx::fromInt(y0);
    const fx slope = fx::fromRatio(dy, dx);

    int xStart = x0;
    int xEnd   = x1;

    if (dy == 0) {
        if (y0 < slabY0 || y0 > slabY1) return;
    } else {
        // Restrict the x walk to positions that can round into this slab's rows.
        const fx invSlope = fx::fromRatio(dx, dy);
        const fx bandLo = fx::fromInt(slabY0) - fx::half();
        const fx bandHi = fx::fromInt(slabY1) + fx::half();

        const fx xa = x0f + (bandLo - y0f) * invSlope;
        const fx xb = x0f + (bandHi - y0f) * invSlope;

        const fx xLo = (xa < xb) ? xa : xb;
        const fx xHi = (xa > xb) ? xa : xb;

        xStart = max_i32(x0, floorFxToInt(xLo) - 1);
        xEnd   = min_i32(x1, ceilFxToInt(xHi) + 1);
        if (xStart > xEnd) return;
    }

    for (int x = xStart; x <= xEnd; ++x) {
        const fx y = y0f + gv::mulInt(slope, x - x0);
        const int yi = y.roundToInt();
        if ((unsigned)(yi - slabY0) < (unsigned)SLAB_ROWS) {
            plotSlab(slab, x, yi - slabY0, ln.c565);
        }
    }
}

void Ili9488Display::drawLineYMajorIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln) {
    using gv::fx;

    int x0 = ln.x0;
    int y0 = ln.y0;
    int x1 = ln.x1;
    int y1 = ln.y1;

    // Normalize the walk so y always increases.
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

    const fx x0f = fx::fromInt(x0);
    const fx slope = fx::fromRatio(dx, dy);

    for (int y = yStart; y <= yEnd; ++y) {
        const fx x = x0f + gv::mulInt(slope, y - y0);
        const int xi = x.roundToInt();
        plotSlab(slab, xi, y - slabY0, ln.c565);
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

        const uint16_t a = f.slabOffset[slabIndex];
        const uint16_t b = f.slabCursor[slabIndex];
        for (uint16_t k = a; k < b; ++k) {
            const Line& ln = f.lines[f.slabIndices[k]];
            drawLineIntoSlab(slab, slabY0, slabY1, ln);
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