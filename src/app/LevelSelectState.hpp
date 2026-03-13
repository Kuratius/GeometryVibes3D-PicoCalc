#pragma once

#include "IAppState.hpp"
#include "render/Text.hpp"
#include <array>
#include <cstddef>

namespace gv {

class LevelSelectState final : public IAppState {
public:
    void onEnter(App& app) override;
    void onExit(App& app) override;

    void update(App& app, const InputState& in, uint32_t dtUs) override;
    void render(App& app, IDisplay& display, RenderList& rl) override;

private:
    static constexpr std::size_t kLevelTextCap = 8;

    void rebuildTexts(App& app);
    void buildMenu(App& app, RenderList& rl) const;

private:
    std::size_t selectedLevel_ = 0;
    std::size_t levelCount_ = 0;

    Text menuTitle_{ "SELECT LEVEL" };
    std::array<Text, kLevelTextCap> levelTexts_{};
};

} // namespace gv