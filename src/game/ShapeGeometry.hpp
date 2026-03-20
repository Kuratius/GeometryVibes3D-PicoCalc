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

static constexpr std::size_t kCubeEdgeCount = 12;
static constexpr std::size_t kRightTriPrismEdgeCount = 9;
static constexpr std::size_t kSquarePyramidEdgeCount = 8;
static constexpr std::size_t kStarEdgeCount = 5;
static constexpr std::size_t kMaxPrimitiveEdgeCount = kCubeEdgeCount;

using PrimitiveEdgeBuffer = EdgeBuffer<kMaxPrimitiveEdgeCount>;

static constexpr fx shapeFi(int v) {
    return fx::fromInt(v);
}

template <std::size_t CAP>
static inline void buildCubeLocal(EdgeBuffer<CAP>& out) {
    out.clear();

    const Vec3fx verts[8] = {
        { fx::zero(),         fx::zero(),         fx::zero()         },
        { shapeFi(kCellSize), fx::zero(),         fx::zero()         },
        { shapeFi(kCellSize), shapeFi(kCellSize), fx::zero()         },
        { fx::zero(),         shapeFi(kCellSize), fx::zero()         },
        { fx::zero(),         fx::zero(),         shapeFi(kCellSize) },
        { shapeFi(kCellSize), fx::zero(),         shapeFi(kCellSize) },
        { shapeFi(kCellSize), shapeFi(kCellSize), shapeFi(kCellSize) },
        { fx::zero(),         shapeFi(kCellSize), shapeFi(kCellSize) }
    };

    out.push(verts[0], verts[1]);
    out.push(verts[1], verts[2]);
    out.push(verts[2], verts[3]);
    out.push(verts[3], verts[0]);
    out.push(verts[4], verts[5]);
    out.push(verts[5], verts[6]);
    out.push(verts[6], verts[7]);
    out.push(verts[7], verts[4]);
    out.push(verts[0], verts[4]);
    out.push(verts[1], verts[5]);
    out.push(verts[2], verts[6]);
    out.push(verts[3], verts[7]);
}

template <std::size_t CAP>
static inline void buildRightTriPrismLocal(EdgeBuffer<CAP>& out) {
    out.clear();

    const Vec3fx verts[6] = {
        { shapeFi(kCellSize), shapeFi(kCellSize), fx::zero()         },
        { shapeFi(kCellSize), fx::zero(),         fx::zero()         },
        { fx::zero(),         fx::zero(),         fx::zero()         },
        { shapeFi(kCellSize), shapeFi(kCellSize), shapeFi(kCellSize) },
        { shapeFi(kCellSize), fx::zero(),         shapeFi(kCellSize) },
        { fx::zero(),         fx::zero(),         shapeFi(kCellSize) }
    };

    out.push(verts[0], verts[1]);
    out.push(verts[1], verts[2]);
    out.push(verts[2], verts[0]);
    out.push(verts[3], verts[4]);
    out.push(verts[4], verts[5]);
    out.push(verts[5], verts[3]);
    out.push(verts[0], verts[3]);
    out.push(verts[1], verts[4]);
    out.push(verts[2], verts[5]);
}

template <std::size_t CAP>
static inline void buildSquarePyramidLocal(EdgeBuffer<CAP>& out, fx apexScale) {
    out.clear();

    const Vec3fx verts[5] = {
        { shapeFi(kCellSize / 2), apexScale * shapeFi(kCellSize), shapeFi(kCellSize / 2) },
        { fx::zero(),             fx::zero(),                     shapeFi(kCellSize)     },
        { shapeFi(kCellSize),     fx::zero(),                     shapeFi(kCellSize)     },
        { shapeFi(kCellSize),     fx::zero(),                     fx::zero()             },
        { fx::zero(),             fx::zero(),                     fx::zero()             }
    };

    out.push(verts[0], verts[1]);
    out.push(verts[0], verts[2]);
    out.push(verts[0], verts[3]);
    out.push(verts[0], verts[4]);
    out.push(verts[1], verts[2]);
    out.push(verts[2], verts[3]);
    out.push(verts[3], verts[4]);
    out.push(verts[4], verts[1]);
}

template <std::size_t CAP>
static inline void buildStarLocal(EdgeBuffer<CAP>& out) {
    out.clear();

    // Upright 5-point star centered in the XY plane at Z = 0.
    // Sized to 80% of a cell overall, so radius = 40% of cell.
    const fx r = shapeFi(kCellSize) * fx::fromRatio(2, 5);

    const Vec3fx verts[5] = {
        { fx::zero(),                     r,                             fx::zero() },
        { r * fx::fromRatio( 588, 1000), -r * fx::fromRatio( 809, 1000), fx::zero() },
        { r * fx::fromRatio(-951, 1000),  r * fx::fromRatio( 309, 1000), fx::zero() },
        { r * fx::fromRatio( 951, 1000),  r * fx::fromRatio( 309, 1000), fx::zero() },
        { r * fx::fromRatio(-588, 1000), -r * fx::fromRatio( 809, 1000), fx::zero() },
    };

    out.push(verts[0], verts[1]);
    out.push(verts[1], verts[2]);
    out.push(verts[2], verts[3]);
    out.push(verts[3], verts[4]);
    out.push(verts[4], verts[0]);
}

static inline void buildPrimitiveLocal(PrimitiveEdgeBuffer& out, ObstacleId sid) {
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

        case ObstacleId::Star:
            buildStarLocal(out);
            break;

        default:
            out.clear();
            break;
    }
}

} // namespace gv