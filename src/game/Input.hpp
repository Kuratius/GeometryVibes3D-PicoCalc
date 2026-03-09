#pragma once

namespace gv {
    
struct InputState {
    bool thrust = false;

    bool upPressed = false;
    bool downPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;

    bool confirm = false;        // "pressed this frame"
    bool back = false;           // "pressed this frame"
    bool pausePressed = false;   // "pressed this frame"
    bool thrustPressed = false;  // pressed this frame
};

} // namespace gv