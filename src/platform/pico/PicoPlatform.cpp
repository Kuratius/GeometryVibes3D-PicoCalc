#include "../IPlatform.hpp"
#include "Ili9488Display.hpp"
#include "PicoFileSystem.hpp"
#include "PicoInput.hpp"
#include "PicoSouthbridge.hpp"
#include "pico/stdlib.h"

namespace gv {

class PicoPlatform : public IPlatform {
public:
    void init() override {
        stdio_init_all();
        sleep_ms(500);
        last_ = time_us_64();

        sb_.init();
        kb_.init();

        refreshBatteryCache(true);
    }

    uint64_t nowUs() const override { return time_us_64(); }

    uint32_t dtUs() override {
        uint64_t now = time_us_64();
        uint64_t us = now - last_;
        last_ = now;
        return (uint32_t)us;
    }

    uint8_t batteryLevelPercent() const override {
        refreshBatteryCache(false);
        const uint8_t percent = batteryRaw_ & 0x7F;
        return (percent <= 100) ? percent : 100;
    }

    bool batteryCharging() const override {
        refreshBatteryCache(false);
        return (batteryRaw_ & 0x80u) != 0;
    }

    IDisplay& display() override { return disp_; }
    IFileSystem& fs() override { return fs_; }
    IInput& input() override { return kb_; }

private:
    void refreshBatteryCache(bool force) const {
        static constexpr uint64_t kBatteryCacheUs = 2000000; // 2 seconds

        const uint64_t now = time_us_64();
        if (!force && batteryCacheValid_ && (now - batteryCacheUs_) < kBatteryCacheUs) {
            return;
        }

        batteryRaw_ = sb_.batteryRaw();
        batteryCacheUs_ = now;
        batteryCacheValid_ = true;
    }

private:
    Ili9488Display disp_;
    PicoFileSystem fs_;
    PicoKeyboardInput kb_;
    PicoSouthbridge sb_;
    mutable uint8_t batteryRaw_ = 0;
    mutable uint64_t batteryCacheUs_ = 0;
    mutable bool batteryCacheValid_ = false;
    uint64_t last_{};
};

IPlatform* createPlatform() {
    static PicoPlatform p;
    return &p;
}

} // namespace gv