#pragma once

#include <cstddef>
#include "core/Fixed.hpp"
#include "core/Config.hpp"
#include "core/Math.hpp"
#include "game/Level.hpp"

namespace gv {

struct Edge3fx {
    Vec3fx a{};
    Vec3fx b{};
};

template <std::size_t CAP>
struct EdgeBuffer {
    Edge3fx edges[CAP]{};
    std::size_t count = 0;

    void clear() { count = 0; }

    bool push(const Vec3fx& a, const Vec3fx& b) {
        if (count >= CAP) return false;
        edges[count++] = Edge3fx{a, b};
        return true;
    }
};

static inline fx shapeFi(int v) {
    return fx::fromInt(v);
}

template <std::size_t CAP>
static inline void buildCubeLocal(EdgeBuffer<CAP>& out) {
    out.clear();

    const Vec3fx verts[] = {
        { fx::zero(),         fx::zero(),         fx::zero()         },
        { shapeFi(kCellSize), fx::zero(),         fx::zero()         },
        { shapeFi(kCellSize), shapeFi(kCellSize), fx::zero()         },
        { fx::zero(),         shapeFi(kCellSize), fx::zero()         },
        { fx::zero(),         fx::zero(),         shapeFi(kCellSize) },
        { shapeFi(kCellSize), fx::zero(),         shapeFi(kCellSize) },
        { shapeFi(kCellSize), shapeFi(kCellSize), shapeFi(kCellSize) },
        { fx::zero(),         shapeFi(kCellSize), shapeFi(kCellSize) }
    };

    const int indices[] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };

    for (std::size_t i = 0; i < sizeof(indices)/sizeof(indices[0]); i += 2) {
        out.push(verts[indices[i]], verts[indices[i + 1]]);
    }
}

template <std::size_t CAP>
static inline void buildRightTriPrismLocal(EdgeBuffer<CAP>& out) {
    out.clear();

    const Vec3fx verts[] = {
        { shapeFi(kCellSize), shapeFi(kCellSize), fx::zero()         },
        { shapeFi(kCellSize), fx::zero(),         fx::zero()         },
        { fx::zero(),         fx::zero(),         fx::zero()         },
        { shapeFi(kCellSize), shapeFi(kCellSize), shapeFi(kCellSize) },
        { shapeFi(kCellSize), fx::zero(),         shapeFi(kCellSize) },
        { fx::zero(),         fx::zero(),         shapeFi(kCellSize) }
    };

    const int indices[] = {
        0,1, 1,2, 2,0,
        3,4, 4,5, 5,3,
        0,3, 1,4, 2,5
    };

    for (std::size_t i = 0; i < sizeof(indices)/sizeof(indices[0]); i += 2) {
        out.push(verts[indices[i]], verts[indices[i + 1]]);
    }
}

template <std::size_t CAP>
static inline void buildSquarePyramidLocal(EdgeBuffer<CAP>& out, fx apexScale) {
    out.clear();

    const Vec3fx verts[] = {
        { shapeFi(kCellSize / 2), apexScale * shapeFi(kCellSize), shapeFi(kCellSize / 2) },
        { fx::zero(),             fx::zero(),                    shapeFi(kCellSize)       },
        { shapeFi(kCellSize),     fx::zero(),                    shapeFi(kCellSize)       },
        { shapeFi(kCellSize),     fx::zero(),                    fx::zero()               },
        { fx::zero(),             fx::zero(),                    fx::zero()               }
    };

    const int indices[] = {
        0,1, 0,2, 0,3, 0,4,
        1,2, 2,3, 3,4, 4,1
    };

    for (std::size_t i = 0; i < sizeof(indices)/sizeof(indices[0]); i += 2) {
        out.push(verts[indices[i]], verts[indices[i + 1]]);
    }
}

template <std::size_t CAP>
static inline void buildPrimitiveLocal(EdgeBuffer<CAP>& out, ObstacleId sid) {
    switch (sid) {
        case ObstacleId::Square:
            buildCubeLocal(out);
            break;

        case ObstacleId::RightTri:
            buildRightTriPrismLocal(out);
            break;

        case ObstacleId::HalfSpike:
            buildSquarePyramidLocal(out, fx::half());
            break;

        case ObstacleId::FullSpike:
            buildSquarePyramidLocal(out, fx::one());
            break;

        default:
            out.clear();
            break;
    }
}

} // namespace gv