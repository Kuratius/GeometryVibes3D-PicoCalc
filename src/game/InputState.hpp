#pragma once

namespace gv {
    
struct InputState {
    bool thrust = false;

    bool upPressed = false;
    bool downPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;

    bool confirm = false;
    bool back = false;
    bool pausePressed = false;
    bool thrustPressed = false;

    bool overlayTogglePressed = false;
};

} // namespace gv