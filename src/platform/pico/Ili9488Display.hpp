#pragma once

#include "../IDisplay.hpp"
#include <cstddef>
#include <cstdint>

namespace gv {

class Ili9488Display final : public IDisplay {
public:
    Ili9488Display();
    ~Ili9488Display();

    int width() const override  { return W; }
    int height() const override { return H; }

    void beginFrame() override;
    void draw(const RenderList& rl) override;
    void endFrame() override;

    static constexpr int W = 320;
    static constexpr int H = 320;
    static constexpr int SLAB_ROWS = 8;

private:
    static constexpr unsigned SPI_BAUD_HZ = 62'500'000;

    static constexpr int PIN_SCK  = 10;
    static constexpr int PIN_MOSI = 11;
    static constexpr int PIN_CS   = 13;
    static constexpr int PIN_DC   = 14;
    static constexpr int PIN_RST  = 15;

    static constexpr int NUM_SLABS = (H + SLAB_ROWS - 1) / SLAB_ROWS;

    static constexpr int MAX_LINES          = 2048;
    static constexpr int MAX_BINNED_ENTRIES = 8192;

    struct Line {
        int16_t x0, y0, x1, y1;
        uint16_t c565;
    };

    struct Frame {
        int lineCount = 0;
        Line lines[MAX_LINES];

        // Per-slab bins for the lines that touch each vertical slab.
        uint16_t slabCount[NUM_SLABS]{};
        uint16_t slabOffset[NUM_SLABS + 1]{};
        uint16_t slabCursor[NUM_SLABS]{};
        uint16_t slabIndices[MAX_BINNED_ENTRIES]{};

        int binnedTotal = 0;
    };

    bool inited = false;

    static Frame s_frame[2];
    static volatile bool s_slotReady[2];
    static volatile int s_prod;
    static Ili9488Display* s_active;

    int lastLines = 0;
    int lastBinned = 0;

    void lcdFillBlack();
    void initIfNeeded();
    void lcdReset();
    void lcdInit();
    void setAddrWindow(int x0, int y0, int x1, int y1);
    void writeCmd(uint8_t cmd);
    void writeData(const uint8_t* data, size_t n);
    void writeDataByte(uint8_t b);

    static bool clipLineToRect(int& x0, int& y0, int& x1, int& y1,
                               int xmin, int ymin, int xmax, int ymax);
    static void binFrameLines(Frame& f);

    static void core1_entry();
    void renderAndFlushFrame(const Frame& f);

    static inline void plotSlab(uint16_t* slab, int x, int yLocal, uint16_t c565);

    // Dispatch to the major-axis slab rasterizer.
    void drawLineIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln);

    // Use one global line equation and only walk the span relevant to this slab.
    void drawLineXMajorIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln);
    void drawLineYMajorIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln);
};

} // namespace gv