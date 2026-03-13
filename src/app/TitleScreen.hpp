#pragma once

#include "platform/IFileSystem.hpp"
#include "platform/IDisplay.hpp"
#include "game/InputState.hpp"
#include "render/Text.hpp"
#include "render/Fixed.hpp"
#include <array>
#include <cstdint>

namespace gv {

class TitleScreen {
public:
    bool load(IFileSystem& fs, const char* path);
    void unload();
    void update(const InputState& in);
    void draw(IDisplay& display);

    bool ready() const { return file_ != nullptr; }
    bool accepted() const { return accepted_; }

private:
    static constexpr int W = 320;
    static constexpr int H = 320;
    static constexpr int SLAB_ROWS = 16;

    static constexpr int kFps = 30;
    static constexpr int kPeriodSeconds = 4;
    static constexpr int kFramesPerPeriod = kFps * kPeriodSeconds; // 120

private:
    void buildAlphaLut();

    static fx easeInOutCubic(fx t);

private:
    IFile* file_ = nullptr;
    bool accepted_ = false;
    bool drawnFull_ = false;

    Text prompt_{ "Press [SPACE]" };

    std::array<uint16_t, W * SLAB_ROWS> slab_{};
    std::array<uint8_t, kFramesPerPeriod> alphaLut_{};
    int phaseFrame_ = 0;
};

} // namespace gv