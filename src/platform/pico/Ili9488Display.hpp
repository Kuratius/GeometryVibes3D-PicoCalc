#pragma once

#include "../IDisplay.hpp"
#include "render/Text.hpp"
#include "render/RenderList.hpp"
#include <cstdint>
#include <cstddef>

namespace gv {

class Ili9488Display final : public IDisplay {
public:
    Ili9488Display();
    ~Ili9488Display();

    int width() const override { return W; }
    int height() const override { return H; }

    void beginFrame() override;
    void draw(const RenderList& rl) override;
    void drawBitmap565(int x, int y, int w, int h, const uint16_t* pixels) override;
    void endFrame() override;

    static bool serialOutputEnabled();
    static void setSerialOutputEnabled(bool enabled);
    
    static constexpr int W = 320;
    static constexpr int H = 320;
    // Number of screen rows in one render slab.
    // Higher values reduce slab count and binning overhead, but increase temporary
    // slab-buffer size and per-slab work. Tune this based on typical scene complexity and available memory.
    static constexpr int SLAB_ROWS = 8;

private:
    static constexpr unsigned SPI_BAUD_HZ = 62'500'000;

    static constexpr int PIN_SCK  = 10;
    static constexpr int PIN_MOSI = 11;
    static constexpr int PIN_CS   = 13;
    static constexpr int PIN_DC   = 14;
    static constexpr int PIN_RST  = 15;

    static constexpr int NUM_SLABS = (H + SLAB_ROWS - 1) / SLAB_ROWS;

    // These *_BINNED capacities are heuristic budgets for slab references, not exact bounds.
    // If primitives start disappearing or partial-frame artifacts appear under heavier scenes,
    // increase these values.
    static constexpr int MAX_LINES            = RenderList::LINE_CAP;
    static constexpr int MAX_LINES_BINNED     = MAX_LINES * 2;
    static constexpr int MAX_FILLRECTS        = RenderList::FILLRECT_CAP;
    static constexpr int MAX_FILLRECTS_BINNED = MAX_FILLRECTS * 4;
    static constexpr int MAX_TEXTS            = RenderList::TEXT_CAP;
    static constexpr int MAX_TEXT_BINNED      = MAX_TEXTS * 8 / 3;

    struct Line {
        int16_t x0, y0, x1, y1;
        uint16_t c565;
    };

    struct Frame {
        int lineCount = 0;
        Line lines[MAX_LINES];

        uint16_t lineSlabCount[NUM_SLABS]{};
        uint16_t lineSlabOffset[NUM_SLABS + 1]{};
        uint16_t lineSlabCursor[NUM_SLABS]{};
        uint16_t lineSlabIndices[MAX_LINES_BINNED]{};
        int lineBinnedTotal = 0;

        int fillRectCount = 0;
        FillRectInst fillRects[MAX_FILLRECTS];

        uint16_t fillRectSlabCount[NUM_SLABS]{};
        uint16_t fillRectSlabOffset[NUM_SLABS + 1]{};
        uint16_t fillRectSlabCursor[NUM_SLABS]{};
        uint16_t fillRectSlabIndices[MAX_FILLRECTS_BINNED]{};
        int fillRectBinnedTotal = 0;

        int textCount = 0;
        TextInst texts[MAX_TEXTS];

        uint16_t textSlabCount[NUM_SLABS]{};
        uint16_t textSlabOffset[NUM_SLABS + 1]{};
        uint16_t textSlabCursor[NUM_SLABS]{};
        uint16_t textSlabIndices[MAX_TEXT_BINNED]{};
        int textBinnedTotal = 0;
    };

    bool inited = false;

    static Frame s_frame[2];
    static volatile bool s_slotReady[2];
    static volatile int s_prod;
    static Ili9488Display* s_active;

    int lastLines = 0;
    int lastBinned = 0;
    int lastTexts = 0;

    static bool s_serialOutputEnabled;

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
    static void binFrameFillRects(Frame& f);
    static void binFrameTexts(Frame& f);

    static void core1_entry();
    void renderAndFlushFrame(const Frame& f);

    static inline void plotSlab(uint16_t* slab, int x, int yLocal, uint16_t c565);

    void drawLineIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln);
    void drawLineXMajorIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln);
    void drawLineYMajorIntoSlab(uint16_t* slab, int slabY0, int slabY1, const Line& ln);

    void drawFillRectIntoSlab(uint16_t* slab, int slabY0, int slabY1, const FillRectInst& inst);
    void drawTextIntoSlab(uint16_t* slab, int slabY0, int slabY1, const TextInst& inst);
};

} // namespace gv