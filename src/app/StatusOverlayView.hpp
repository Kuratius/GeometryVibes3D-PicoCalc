#pragma once

#include <cstdint>

namespace gv {

class StatusOverlay;
struct RenderList;

namespace StatusOverlayView {

void appendTo(RenderList& rl,
              const StatusOverlay& overlay,
              int screenW,
              int screenH);

} // namespace StatusOverlayView

} // namespace gv