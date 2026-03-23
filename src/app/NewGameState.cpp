#include "NewGameState.hpp"
#include "App.hpp"
#include "render/RenderList.hpp"
#include "render/Colors.hpp"

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

        if (!app.saveSaves() || !app.createNewSave(name_.data())) {
            app.statusOverlay().addWarning("Could not create save");
            app.statusOverlay().addInfo("Is SD card inserted?");
            app.showHomeMenu();
            return;
        }

        app.showLevelSelect();
        return;
    }

    if (in.typedChar != '\0') {
        (void)appendChar(in.typedChar);
    }
}

void NewGameState::render(App& app, RenderList& rl) {
    rl.clear();

    const int w = app.displayWidth();

    rl.addText(&title_, (int16_t)((w - title_.width()) / 2), 12, gv::color::White);
    rl.addText(&prompt_, 16, 48, gv::color::White);
    rl.addText(&nameText_, 16, 64, gv::color::White, 255, true);

    rl.addText(&help1_, 16, 96, gv::color::Gray);
    rl.addText(&help2_, 16, 108, gv::color::Gray);
    rl.addText(&help3_, 16, 120, gv::color::Gray);
}

} // namespace gv