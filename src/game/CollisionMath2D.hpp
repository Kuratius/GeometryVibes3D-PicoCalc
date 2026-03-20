#pragma once

#include "core/Fixed.hpp"
#include "core/Config.hpp"
#include "game/Level.hpp"

namespace gv {

struct Pt2fx {
    fx x{};
    fx y{};
};

struct Poly2fx {
    Pt2fx v[4]{};
    int count = 0;
};

static inline fx cross2(const Pt2fx& a, const Pt2fx& b, const Pt2fx& c) {
    const fx abx = b.x - a.x;
    const fx aby = b.y - a.y;
    const fx acx = c.x - a.x;
    const fx acy = c.y - a.y;
    return abx * acy - aby * acx;
}

static inline bool onSegment(const Pt2fx& a, const Pt2fx& b, const Pt2fx& p) {
    const fx minX = gv::min(a.x, b.x);
    const fx maxX = gv::max(a.x, b.x);
    const fx minY = gv::min(a.y, b.y);
    const fx maxY = gv::max(a.y, b.y);

    return (p.x >= minX && p.x <= maxX &&
            p.y >= minY && p.y <= maxY);
}

static inline int orientSign(const Pt2fx& a, const Pt2fx& b, const Pt2fx& c) {
    const fx cr = cross2(a, b, c);
    if (cr.raw() > 0) return 1;
    if (cr.raw() < 0) return -1;
    return 0;
}

static inline bool segmentsIntersect(const Pt2fx& a, const Pt2fx& b,
                                     const Pt2fx& c, const Pt2fx& d) {
    const int o1 = orientSign(a, b, c);
    const int o2 = orientSign(a, b, d);
    const int o3 = orientSign(c, d, a);
    const int o4 = orientSign(c, d, b);

    if (o1 != o2 && o3 != o4) {
        return true;
    }

    if (o1 == 0 && onSegment(a, b, c)) return true;
    if (o2 == 0 && onSegment(a, b, d)) return true;
    if (o3 == 0 && onSegment(c, d, a)) return true;
    if (o4 == 0 && onSegment(c, d, b)) return true;

    return false;
}

static inline bool pointInConvexPoly(const Pt2fx& p, const Poly2fx& poly) {
    bool sawPos = false;
    bool sawNeg = false;

    for (int i = 0; i < poly.count; ++i) {
        const Pt2fx& a = poly.v[i];
        const Pt2fx& b = poly.v[(i + 1) % poly.count];
        const fx cr = cross2(a, b, p);

        if (cr.raw() > 0) sawPos = true;
        if (cr.raw() < 0) sawNeg = true;

        if (sawPos && sawNeg) {
            return false;
        }
    }

    return true;
}

static inline bool convexPolysOverlap(const Poly2fx& a, const Poly2fx& b) {
    for (int i = 0; i < a.count; ++i) {
        const Pt2fx& a0 = a.v[i];
        const Pt2fx& a1 = a.v[(i + 1) % a.count];

        for (int j = 0; j < b.count; ++j) {
            const Pt2fx& b0 = b.v[j];
            const Pt2fx& b1 = b.v[(j + 1) % b.count];

            if (segmentsIntersect(a0, a1, b0, b1)) {
                return true;
            }
        }
    }

    if (pointInConvexPoly(a.v[0], b)) return true;
    if (pointInConvexPoly(b.v[0], a)) return true;

    return false;
}

static inline void rotatePointAround(Pt2fx& p, fx ox, fx oy, fx c, fx s) {
    const fx dx = p.x - ox;
    const fx dy = p.y - oy;
    p.x = ox + dx * c - dy * s;
    p.y = oy + dx * s + dy * c;
}

static inline void applyShapeModForward(Pt2fx& p, ShapeMod mod, fx ox, fx oy) {
    fx dx = p.x - ox;
    fx dy = p.y - oy;

    switch (mod) {
        case ShapeMod::None:
            break;

        case ShapeMod::RotLeft: {
            fx ndx = -dy;
            fx ndy =  dx;
            dx = ndx;
            dy = ndy;
        } break;

        case ShapeMod::RotRight: {
            fx ndx =  dy;
            fx ndy = -dx;
            dx = ndx;
            dy = ndy;
        } break;

        case ShapeMod::Invert:
            dx = -dx;
            dy = -dy;
            break;
    }

    p.x = ox + dx;
    p.y = oy + dy;
}

static inline void buildPrimitivePoly2D(ObstacleId sid, ShapeMod mod, Poly2fx& poly) {
    const fx k = fx::fromInt(gv::kCellSize);
    const fx half = fx::fromInt(gv::kCellSize / 2);

    poly.count = 0;

    switch (sid) {
        case ObstacleId::Square:
            poly.count = 4;
            poly.v[0] = { fx::zero(), fx::zero() };
            poly.v[1] = { k,          fx::zero() };
            poly.v[2] = { k,          k };
            poly.v[3] = { fx::zero(), k };
            break;

        case ObstacleId::RightTri:
            poly.count = 3;
            poly.v[0] = { fx::zero(), fx::zero() };
            poly.v[1] = { k,          fx::zero() };
            poly.v[2] = { k,          k };
            break;

        case ObstacleId::HalfSpike:
            poly.count = 3;
            poly.v[0] = { fx::zero(), fx::zero() };
            poly.v[1] = { k,          fx::zero() };
            poly.v[2] = { half,       half };
            break;

        case ObstacleId::FullSpike:
            poly.count = 3;
            poly.v[0] = { fx::zero(), fx::zero() };
            poly.v[1] = { k,          fx::zero() };
            poly.v[2] = { half,       k };
            break;

        default:
            return;
    }

    for (int i = 0; i < poly.count; ++i) {
        applyShapeModForward(poly.v[i], mod, half, half);
    }
}

static inline void buildShipPoly2D(fx shipCenterX, fx shipCenterY, fx shipVy, Poly2fx& poly) {
    const fx pad = fx::fromRatio(3, 2);
    const fx halfW = fx::fromInt(gv::kCellSize / 4) - pad;
    const fx halfLen = fx::fromInt(gv::kCellSize) * fx::fromRatio(9, 20) - pad;

    poly.count = 3;

    poly.v[0] = { shipCenterX + halfLen, shipCenterY };
    poly.v[1] = { shipCenterX - halfLen, shipCenterY + halfW };
    poly.v[2] = { shipCenterX - halfLen, shipCenterY - halfW };

    fx c = fx::one();
    fx s = fx::zero();

    if (shipVy.raw() > 0) {
        constexpr fx cos45 = fx::fromRaw(46341);
        c = cos45;
        s = cos45;
    } else if (shipVy.raw() < 0) {
        constexpr fx cos45 = fx::fromRaw(46341);
        c = cos45;
        s = -cos45;
    }

    for (int i = 0; i < poly.count; ++i) {
        rotatePointAround(poly.v[i], shipCenterX, shipCenterY, c, s);
    }
}

} // namespace gv