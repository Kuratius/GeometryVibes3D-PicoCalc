#include "HomeMenuState.hpp"
#include "App.hpp"
#include "render/RenderList.hpp"
#include "StatusOverlayView.hpp"

namespace gv {

void HomeMenuState::onEnter(App& app) {
    (void)app;

    selected_ = 0;
    if (!itemEnabled(app, selected_)) {
        moveSelection(app, +1);
    }
}

bool HomeMenuState::itemEnabled(const App& app, std::size_t i) const {
    switch (i) {
        case Continue:   return app.canContinue();
        case SavedGames: return app.hasAnySave();
        case NewGame:    return !app.saveData().full();
        case Options:    return true;
        default:         return false;
    }
}

void HomeMenuState::moveSelection(const App& app, int dir) {
    for (std::size_t step = 0; step < Count; ++step) {
        if (dir > 0) {
            selected_ = (selected_ + 1) % Count;
        } else {
            selected_ = (selected_ + Count - 1) % Count;
        }

        if (itemEnabled(app, selected_)) {
            return;
        }
    }
}

void HomeMenuState::update(App& app, const InputState& in, uint32_t dtUs) {
    (void)dtUs;

    if (in.overlayTogglePressed) {
        app.statusOverlay().toggleVisible();
    }

    if (in.upPressed) {
        moveSelection(app, -1);
    } else if (in.downPressed) {
        moveSelection(app, +1);
    }

    if (!(in.confirm || in.thrustPressed)) {
        return;
    }

    if (!itemEnabled(app, selected_)) {
        return;
    }

    switch (selected_) {
        case Continue:
            if (!app.continueGame()) {
                app.statusOverlay().addWarning("Could not continue");
            }
            break;

        case SavedGames:
            app.showSavedGames();
            break;

        case NewGame:
            app.showNewGame();
            break;

        case Options:
            app.showOptions();
            break;

        default:
            break;
    }
}

void HomeMenuState::render(App& app, IDisplay& display, RenderList& rl) {
    static constexpr uint16_t kWhite = 0xFFFF;
    static constexpr uint16_t kGray  = 0x8410;

    rl.clear();

    const int w = app.displayWidth();
    const int titleX = (w - title_.width()) / 2;
    rl.addText(&title_, (int16_t)titleX, 12, kWhite);

    const int startY = 52;
    const int stepY = 14;
    const int itemX = 16;

    for (std::size_t i = 0; i < Count; ++i) {
        const bool enabled = itemEnabled(app, i);
        const bool selected = enabled && (i == selected_);
        rl.addText(
            &items_[i],
            (int16_t)itemX,
            (int16_t)(startY + int(i) * stepY),
            enabled ? kWhite : kGray,
            255,
            selected
        );
    }

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