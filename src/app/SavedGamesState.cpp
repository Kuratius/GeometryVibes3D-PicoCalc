#include "SavedGamesState.hpp"
#include "App.hpp"
#include "render/RenderList.hpp"
#include "StatusOverlayView.hpp"

namespace gv {

void SavedGamesState::onEnter(App& app) {
    confirmDelete_ = false;
    selected_ = 0;
    rebuildTexts(app);
    clampSelection(app);
}

void SavedGamesState::rebuildTexts(App& app) {
    count_ = app.saveData().entryCount();
    if (count_ > kTextCap) count_ = kTextCap;

    for (std::size_t i = 0; i < count_; ++i) {
        saveTexts_[i].setText(app.saveData().entryName(i));
    }
}

void SavedGamesState::clampSelection(App& app) {
    (void)app;
    if (count_ == 0) {
        selected_ = 0;
    } else if (selected_ >= count_) {
        selected_ = count_ - 1;
    }
}

void SavedGamesState::update(App& app, const InputState& in, uint32_t dtUs) {
    (void)dtUs;

    if (count_ == 0) {
        app.showHomeMenu();
        return;
    }

    if (confirmDelete_) {
        if (in.confirm) {
            if (!app.deleteSave(selected_)) {
                app.statusOverlay().addWarning("Could not delete save");
            }
            confirmDelete_ = false;
            rebuildTexts(app);
            clampSelection(app);

            if (count_ == 0) {
                app.showHomeMenu();
            }
            return;
        }

        if (in.back) {
            confirmDelete_ = false;
            return;
        }

        return;
    }

    if (in.upPressed && selected_ > 0) {
        --selected_;
    } else if (in.downPressed && (selected_ + 1) < count_) {
        ++selected_;
    }

    if (in.deletePressed) {
        confirmDelete_ = true;
        return;
    }

    if (in.confirm || in.thrustPressed) {
        if (!app.selectSave(selected_)) {
            app.statusOverlay().addWarning("Could not load save");
            return;
        }
        app.showLevelSelect();
        return;
    }

    if (in.back) {
        app.showHomeMenu();
        return;
    }
}

void SavedGamesState::render(App& app, IDisplay& display, RenderList& rl) {
    static constexpr uint16_t kWhite = 0xFFFF;
    static constexpr uint16_t kGray  = 0x8410;

    rl.clear();

    const int w = app.displayWidth();
    rl.addText(&title_, (int16_t)((w - title_.width()) / 2), 12, kWhite);

    const int startY = 40;
    const int stepY = 12;
    const int itemX = 16;

    for (std::size_t i = 0; i < count_; ++i) {
        rl.addText(
            &saveTexts_[i],
            (int16_t)itemX,
            (int16_t)(startY + int(i) * stepY),
            kWhite,
            255,
            i == selected_
        );
    }

    if (confirmDelete_) {
        rl.addText(&confirm1_, 16, 180, 0xFFE0);
        rl.addText(&confirm2_, 16, 192, kGray);
    } else {
        rl.addText(&help_, 16, 180, kGray);
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