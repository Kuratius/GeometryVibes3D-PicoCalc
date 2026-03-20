#include "NewGameState.hpp"
#include "App.hpp"
#include "render/RenderList.hpp"
#include "StatusOverlayView.hpp"

namespace gv {

void NewGameState::onEnter(App& app) {
    (void)app;
    len_ = 0;
    name_[0] = '\0';
    rebuildNameText();
}

void NewGameState::rebuildNameText() {
    if (len_ == 0) {
        nameText_.setText("_");
    } else {
        char buf[kMaxNameLen + 2]{};
        for (std::size_t i = 0; i < len_; ++i) {
            buf[i] = name_[i];
        }
        buf[len_] = '_';
        buf[len_ + 1] = '\0';
        nameText_.setText(buf);
    }
}

bool NewGameState::appendChar(char c) {
    if (len_ >= kMaxNameLen) return false;
    name_[len_++] = c;
    name_[len_] = '\0';
    rebuildNameText();
    return true;
}

void NewGameState::backspace() {
    if (len_ == 0) return;
    --len_;
    name_[len_] = '\0';
    rebuildNameText();
}

void NewGameState::update(App& app, const InputState& in, uint32_t dtUs) {
    (void)dtUs;

    if (in.back) {
        app.showHomeMenu();
        return;
    }

    if (in.backspacePressed) {
        backspace();
        return;
    }

    if (in.confirm) {
        if (len_ == 0) {
            app.statusOverlay().addWarning("Enter a name");
            return;
        }

        if (!app.createNewSave(name_.data())) {
            app.statusOverlay().addWarning("Could not create save");
            return;
        }

        app.showLevelSelect();
        return;
    }

    if (in.typedChar != '\0') {
        (void)appendChar(in.typedChar);
    }
}

void NewGameState::render(App& app, IDisplay& display, RenderList& rl) {
    static constexpr uint16_t kWhite = 0xFFFF;
    static constexpr uint16_t kGray  = 0x8410;

    rl.clear();

    const int w = app.displayWidth();

    rl.addText(&title_, (int16_t)((w - title_.width()) / 2), 12, kWhite);
    rl.addText(&prompt_, 16, 48, kWhite);
    rl.addText(&nameText_, 16, 64, kWhite, 255, true);

    rl.addText(&help1_, 16, 96, kGray);
    rl.addText(&help2_, 16, 108, kGray);
    rl.addText(&help3_, 16, 120, kGray);

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