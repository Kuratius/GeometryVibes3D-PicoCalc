#pragma once
#include "platform/IPlatform.hpp"
#include "game/Game.hpp"
#include "render/Renderer.hpp"
#include "render/RenderList.hpp"

namespace gv {

class App {
public:
    // Owns init + main loop
    int run(IPlatform& platform);

private:
    void init(IPlatform& platform);
    void tick(const InputState& in, uint32_t dtUs);

    const RenderList& renderList() const { return rl; }

private:
    IPlatform* plat = nullptr;
    Game game;
    Renderer renderer;
    RenderList rl;
    int w{}, h{};
};

} // namespace gv