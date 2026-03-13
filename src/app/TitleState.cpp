#include "TitleState.hpp"
#include "App.hpp"
#include "platform/IPlatform.hpp"
#include "render/RenderList.hpp"

namespace gv {

void TitleState::onEnter(App& app) {
    if (!titleScreen_.load(app.platform().fs(), "assets/title.rgb565")) {
        app.statusOverlay().addError("Error: Could not load title.rgb565.");
        app.statusOverlay().addInfo("Is the SD card inserted?");
        app.showLevelSelect();
    }
}

void TitleState::onExit(App& app) {
    (void)app;
    titleScreen_.unload();
}

void TitleState::update(App& app, const InputState& in, uint32_t dtUs) {
    (void)dtUs;

    titleScreen_.update(in);
    if (titleScreen_.accepted()) {
        app.showLevelSelect();
    }
}

void TitleState::render(App& app, IDisplay& display, RenderList& rl) {
    (void)app;
    (void)rl;
    titleScreen_.draw(display);
}

} // namespace gv