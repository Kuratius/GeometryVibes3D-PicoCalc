#include "OptionsState.hpp"
#include "App.hpp"
#include "render/RenderList.hpp"
#include "StatusOverlayView.hpp"
#include <cstdio>

namespace gv {

void OptionsState::onEnter(App& app) {
    selected_ = 0;
    rebuildTexts(app);
}

void OptionsState::rebuildTexts(App& app) {
    char buf[48]{};

    std::snprintf(
        buf, sizeof(buf),
        "Enable serial output: %s",
        app.platform().serialOutputEnabled() ? "ON" : "OFF"
    );
    items_[SerialOutput].setText(buf);

    std::snprintf(
        buf, sizeof(buf),
        "Highlight collision: %s",
        app.game().collisionHighlightEnabled() ? "ON" : "OFF"
    );
    items_[HighlightCollision].setText(buf);
}

void OptionsState::update(App& app, const InputState& in, uint32_t dtUs) {
    (void)dtUs;

    if (in.upPressed && selected_ > 0) {
        --selected_;
    } else if (in.downPressed && (selected_ + 1) < Count) {
        ++selected_;
    }

    if (in.back) {
        app.showHomeMenu();
        return;
    }

    if (in.confirm || in.thrustPressed) {
        switch (selected_) {
            case SerialOutput:
                app.platform().setSerialOutputEnabled(!app.platform().serialOutputEnabled());
                break;

            case HighlightCollision:
                app.game().setCollisionHighlightEnabled(!app.game().collisionHighlightEnabled());
                break;

            default:
                break;
        }

        rebuildTexts(app);
    }
}

void OptionsState::render(App& app, IDisplay& display, RenderList& rl) {
    static constexpr uint16_t kWhite = 0xFFFF;
    static constexpr uint16_t kGray  = 0x8410;

    rl.clear();

    const int w = app.displayWidth();
    rl.addText(&title_, (int16_t)((w - title_.width()) / 2), 12, kWhite);

    const int startY = 48;
    const int stepY = 14;
    const int itemX = 16;

    for (std::size_t i = 0; i < Count; ++i) {
        rl.addText(
            &items_[i],
            (int16_t)itemX,
            (int16_t)(startY + int(i) * stepY),
            kWhite,
            255,
            i == selected_
        );
    }

    rl.addText(&help_, 16, 180, kGray);

    StatusOverlayView::appendTo(
        rl,
        app.statusOverlay(),
        app.displayWidth(),
        app.displayHeight()
    );

    display.beginFrame();
    display.draw(rl);
    display.endFrame();
}

} // namespace gv