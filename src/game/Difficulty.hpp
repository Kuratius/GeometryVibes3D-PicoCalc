#pragma once

#include <cstddef>
#include <cstdint>

namespace gv {

enum class Difficulty : uint8_t {
    Rookie = 0,
    Pro,
    Expert,
    Count
};

static constexpr std::size_t kDifficultyCount =
    static_cast<std::size_t>(Difficulty::Count);

static inline std::size_t difficultyIndex(Difficulty d) {
    return static_cast<std::size_t>(d);
}

} // namespace gv