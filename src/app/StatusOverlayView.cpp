#include "StatusOverlayView.hpp"
#include "StatusOverlay.hpp"
#include "render/RenderList.hpp"
#include "render/Text.hpp"

namespace gv {

namespace StatusOverlayView {

void appendTo(RenderList& rl,
              const StatusOverlay& overlay,
              int screenW,
              int screenH)
{
    (void)screenW;

    if (!overlay.visible()) return;

    static constexpr uint16_t kFooter = 0x8410; // gray
    static constexpr int kLineStep = 10;
    static constexpr int kPanelX = 8;
    static constexpr int kBottomMargin = 8;
    static constexpr int kFooterGap = 8;

    static Text footerText{ "F1 to hide" };

    const auto& lines = overlay.lines();
    if (lines.empty()) return;

    const int panelH = int(lines.size()) * kLineStep + 8 + kLineStep;
    int y = screenH - panelH - kLineStep - kBottomMargin;

    for (const auto& line : lines) {
        rl.addText(
            &line.text,
            (int16_t)kPanelX,
            (int16_t)y,
            line.color565,
            255,
            false,
            0,
            TextStyle::Background
        );
        y += kLineStep;
    }

    y += kFooterGap;
    rl.addText(
        &footerText,
        (int16_t)kPanelX,
        (int16_t)y,
        kFooter,
        255,
        false,
        0,
        TextStyle::Background
    );
}

} // namespace StatusOverlayView

} // namespace gv