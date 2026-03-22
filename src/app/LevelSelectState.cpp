#include "LevelSelectState.hpp"
#include "App.hpp"
#include "render/RenderList.hpp"
#include "StatusOverlayView.hpp"
#include "render/Colors.hpp"
#include <cstdio>

namespace gv {

void LevelSelectState::onEnter(App& app) {
    selectedDifficulty_ = app.selectedDifficulty();
    selectedLevel_ = app.selectedLevel();
    focus_ = FocusPane::Difficulty;
    rebuildTexts(app);
}

void LevelSelectState::rebuildTexts(App& app) {
    selectedDifficulty_ = app.selectedDifficulty();
    selectedLevel_ = app.selectedLevel();

    levelCount_ = app.levelCount();
    if (levelCount_ > kLevelTextCap) {
        levelCount_ = kLevelTextCap;
    }

    for (std::size_t i = 0; i < levelCount_; ++i) {
        char buf[48];

        if (app.isLevelUnlocked(i)) {
            const int pct = app.levelPercentComplete(i);
            const uint8_t stars = app.collectedStarsForLevel(i);

            const char s0 = ((stars & 0x01u) != 0) ? '\x7F' : '_';
            const char s1 = ((stars & 0x02u) != 0) ? '\x7F' : '_';
            const char s2 = ((stars & 0x04u) != 0) ? '\x7F' : '_';

            std::snprintf(
                buf,
                sizeof(buf),
                "%s %3d%% %c%c%c",
                app.levelName(i),
                pct,
                s0, s1, s2
            );
        } else {
            std::snprintf(
                buf,
                sizeof(buf),
                "%s [Locked]",
                app.levelName(i)
            );
        }

        levelTexts_[i].setText(buf);
    }

    if (selectedLevel_ >= levelCount_) {
        selectedLevel_ = (levelCount_ > 0) ? (levelCount_ - 1) : 0;
    }
}

void LevelSelectState::buildMenu(App& app, RenderList& rl) const {
    const int w = app.displayWidth();
    const int titleX = (w - menuTitle_.width()) / 2;
    rl.addText(&menuTitle_, (int16_t)titleX, 12, gv::color::White);

    const int startY = 40;
    const int lineStep = 12;
    const int diffX = 16;
    const int itemX = 96;

    for (std::size_t i = 0; i < difficultyTexts_.size(); ++i) {
        const bool selected = (i == difficultyIndex(selectedDifficulty_));

        rl.addText(
            &difficultyTexts_[i],
            (int16_t)diffX,
            (int16_t)(startY + int(i) * lineStep),
            gv::color::White,
            255,
            selected
    );
}

    for (std::size_t i = 0; i < levelCount_; ++i) {
        const bool unlocked = app.isLevelUnlocked(i);
        const bool selected = unlocked &&
                              (i == selectedLevel_) &&
                              (focus_ == FocusPane::Levels);
        const uint16_t color = unlocked ? gv::color::White : gv::color::Gray;

        rl.addText(
            &levelTexts_[i],
            (int16_t)itemX,
            (int16_t)(startY + int(i) * lineStep),
            color,
            255,
            selected
        );
    }

    rl.addText(&help_, 16, 180, gv::color::Gray);
}

void LevelSelectState::update(App& app, const InputState& in, uint32_t dtUs) {
    (void)dtUs;

    if (in.overlayTogglePressed) {
        app.statusOverlay().toggleVisible();
    }

    if (in.leftPressed) {
        focus_ = FocusPane::Difficulty;
    } else if (in.rightPressed) {
        focus_ = FocusPane::Levels;
    }

    if (focus_ == FocusPane::Difficulty) {
        Difficulty next = selectedDifficulty_;

        if (in.upPressed) {
            if (next != Difficulty::Rookie) {
                next = static_cast<Difficulty>(difficultyIndex(next) - 1);
            }
        } else if (in.downPressed) {
            if (next != Difficulty::Expert) {
                next = static_cast<Difficulty>(difficultyIndex(next) + 1);
            }
        }

        if (next != selectedDifficulty_) {
            app.setSelectedDifficulty(next);
            rebuildTexts(app);
        }
    } else {
        if (in.upPressed && selectedLevel_ > 0) {
            --selectedLevel_;
            app.setSelectedLevel(selectedLevel_);
        } else if (in.downPressed && (selectedLevel_ + 1) < app.unlockedLevelCount()) {
            ++selectedLevel_;
            app.setSelectedLevel(selectedLevel_);
        }
        
        if (in.confirm || in.thrustPressed) {
            app.startLevel(selectedLevel_);
        }
    }

    if (in.back) {
        app.showSavedGames();
        return;
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