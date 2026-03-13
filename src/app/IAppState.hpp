#pragma once

#include <cstdint>

namespace gv {

class App;
class IDisplay;
struct RenderList;
struct InputState;

class IAppState {
public:
    virtual ~IAppState() = default;

    virtual void onEnter(App& app) {}
    virtual void onExit(App& app) {}

    virtual void update(App& app, const InputState& in, uint32_t dtUs) = 0;
    virtual void render(App& app, IDisplay& display, RenderList& rl) = 0;
};

} // namespace gv