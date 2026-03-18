#include "LevelSelectState.hpp"
#include "App.hpp"
#include "platform/IPlatform.hpp"
#include "render/RenderList.hpp"
#include "StatusOverlayView.hpp"
#include <cstdio>

namespace gv {

void LevelSelectState::onEnter(App& app) {
    selectedLevel_ = app.selectedLevel();
    rebuildTexts(app);
}

void LevelSelectState::onExit(App& app) {
    (void)app;
}

void LevelSelectState::rebuildTexts(App& app) {
    levelCount_ = app.levelCount();
    if (levelCount_ > kLevelTextCap) {
        levelCount_ = kLevelTextCap;
    }

    const SaveData::SaveEntry* save = app.activeSave();

    for (std::size_t i = 0; i < levelCount_; ++i) {
        char buf[48]{};

        if (app.isLevelUnlocked(i)) {
            unsigned percent = 0;
            if (save && i < SaveData::kLevelCount) {
                percent = save->percentComplete[i];
                if (percent > 100) percent = 100;
            }

            std::snprintf(buf, sizeof(buf), "%s %3u%%", app.levelName(i), percent);
        } else {
            std::snprintf(buf, sizeof(buf), "%s [Locked]", app.levelName(i));
        }

        levelTexts_[i].setText(buf);
    }

    if (selectedLevel_ >= levelCount_) {
        selectedLevel_ = (levelCount_ > 0) ? (levelCount_ - 1) : 0;
    }

    if (!app.isLevelUnlocked(selectedLevel_)) {
        selectedLevel_ = 0;
        while ((selectedLevel_ + 1) < levelCount_ && !app.isLevelUnlocked(selectedLevel_)) {
            ++selectedLevel_;
        }
    }
}

void LevelSelectState::buildMenu(App& app, RenderList& rl) const {
    static constexpr uint16_t kWhite = 0xFFFF;
    static constexpr uint16_t kGray  = 0x8410;

    const int w = app.displayWidth();

    const int titleX = (w - menuTitle_.width()) / 2;
    rl.addText(&menuTitle_, (int16_t)titleX, 12, kWhite);

    const int startY = 40;
    const int lineStep = 12;
    const int itemX = 16;

    for (std::size_t i = 0; i < levelCount_; ++i) {
        const bool unlocked = app.isLevelUnlocked(i);
        const bool selected = unlocked && (i == selectedLevel_);
        const uint16_t color = unlocked ? kWhite : kGray;

        rl.addText(
            &levelTexts_[i],
            (int16_t)itemX,
            (int16_t)(startY + int(i) * lineStep),
            color,
            255,
            selected
        );
    }
    
    rl.addText(&help_, 16, 180, kGray);
}

void LevelSelectState::update(App& app, const InputState& in, uint32_t dtUs) {
    (void)dtUs;

    if (in.overlayTogglePressed) {
        app.statusOverlay().toggleVisible();
    }

    if (in.upPressed && selectedLevel_ > 0) {
        --selectedLevel_;
        app.setSelectedLevel(selectedLevel_);
    } else if (in.downPressed && (selectedLevel_ + 1) < app.unlockedLevelCount()) {
        ++selectedLevel_;
        app.setSelectedLevel(selectedLevel_);
    }

    if (in.back) {
        app.showHomeMenu();
        return;
    }

    if (in.confirm || in.thrustPressed) {
        app.startLevel(selectedLevel_);
    }
}

void LevelSelectState::render(App& app, IDisplay& display, RenderList& rl) {
    rl.clear();
    buildMenu(app, rl);
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