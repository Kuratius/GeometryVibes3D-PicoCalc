#pragma once

#include "IAppState.hpp"
#include "TitleScreen.hpp"

namespace gv {

class TitleState final : public IAppState {
public:
    void onEnter(App& app) override;
    void onExit(App& app) override;

    void update(App& app, const InputState& in, uint32_t dtUs) override;
    void render(App& app, IDisplay& display, RenderList& rl) override;

private:
    TitleScreen titleScreen_{};
};

} // namespace gv