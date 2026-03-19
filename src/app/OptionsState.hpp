#pragma once

#include "IAppState.hpp"
#include "render/Text.hpp"
#include <array>
#include <cstddef>

namespace gv {

class OptionsState final : public IAppState {
public:
    void onEnter(App& app) override;
    void onExit(App& app) override;

    void update(App& app, const InputState& in, uint32_t dtUs) override;
    void render(App& app, IDisplay& display, RenderList& rl) override;

private:
    enum Item : std::size_t {
        SerialOutput = 0,
        HighlightCollision,
        Count
    };

private:
    void rebuildTexts(App& app);

private:
    std::size_t selected_ = 0;

    Text title_{ "OPTIONS" };
    Text help_{ "[ENTER/SPACE] Toggle  [BACK] Home" };
    std::array<Text, Count> items_{};
};

} // namespace gv