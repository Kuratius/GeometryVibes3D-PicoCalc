#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "core/Config.hpp"

namespace gv {

static constexpr uint8_t kColumnBytes = 7;

// Retained after read_header() so future read_column() calls don't need the header passed in.
inline uint16_t g_levelDataOffset = 16;

enum class ObstacleId : uint8_t {
    Empty      = 0,
    Square     = 1,
    RightTri   = 2,
    HalfSpike  = 3,
    FullSpike  = 4,
    Star       = 5, // future use: 5-point star shape, rotating
    // 6..7 reserved
    AnimGroup1 = 8,
    AnimGroup2 = 9,
    AnimGroup3 = 10,
    AnimGroup4 = 11,
    AnimGroup5 = 12,
    AnimGroup6 = 13,
    AnimGroup7 = 14,
    AnimGroup8 = 15,
};

enum class AnimGroupOffsetMod : uint8_t {
    None            = 0,
    ShiftRight      = 1, // shift anchor right by 1/2 cell
    ShiftDown       = 2, // shift anchor down by 1/2 cell
    ShiftDownRight  = 3, // shift anchor down and right by 1/2 cell
};

// Applies to non-anim obstacles and anim-group primitive members.
enum class ShapeMod : uint8_t {
    None     = 0,
    RotLeft  = 1, // rotate clockwise by 90 degrees
    RotRight = 2, // rotate counterclockwise by 90 degrees
    Invert   = 3  // invert in X and Y (rotate 180 degrees)
};

enum class AnimStepType : uint8_t {
    Rotate = 0,
    Hold   = 1,
};

#pragma pack(push, 1)

// 16-byte file header
struct LevelHeader {
    char     magic[3];                // "GVL"
    uint8_t  version;                 // 2 = with anim defs section support

    uint16_t width;                   // length of level in columns
    int8_t   portalDx;                // portal offset in columns from the right edge of the level
    uint16_t backgroundColor565;      // future use: background color for the level

    uint8_t  animDefCount;            // number of animated defs in anim defs section
    uint8_t  animDefMaxPrimitiveCount;// max primitive count of any single anim def

    uint16_t levelDataOffset;         // offset to packed column data from start of file
    uint8_t  reserved[3];             // reserved for future use, should be zero
};
#pragma pack(pop)

static_assert(sizeof(LevelHeader) == 16, "LevelHeader must be 16 bytes");

// 56-bit column payload stored as 7 bytes LE
struct Column56 {
    uint8_t b[kColumnBytes];

    uint64_t to_u64() const {
        uint64_t v = 0;
        v |= uint64_t(b[0]) << 0;
        v |= uint64_t(b[1]) << 8;
        v |= uint64_t(b[2]) << 16;
        v |= uint64_t(b[3]) << 24;
        v |= uint64_t(b[4]) << 32;
        v |= uint64_t(b[5]) << 40;
        v |= uint64_t(b[6]) << 48;
        return v;
    }

    // Returns a packed 6-bit cell value for row y (0..8)
    uint8_t cell6(int y) const {
        const uint64_t v = to_u64();
        return uint8_t((v >> (y * 6)) & 0x3FULL);
    }

    ObstacleId obstacle(int y) const {
        return ObstacleId(cell6(y) & 0x0F);
    }

    ShapeMod shapeMod(int y) const {
        return ShapeMod((cell6(y) >> 4) & 0x03);
    }

    AnimGroupOffsetMod groupMod(int y) const {
        return AnimGroupOffsetMod((cell6(y) >> 4) & 0x03);
    }
};

// -----------------------------
// Animated defs section
//
// Layout:
//   [AnimGroupDefHeader]
//   [primitiveCount * AnimPrimitiveDef]
//   [stepCount * AnimStepDef]
//
// Repeated animDefCount times, starting immediately after LevelHeader,
// ending at levelDataOffset.
//
// The source cell containing AnimGroupN is the placement anchor.
// The anchor point is the CENTER of that source cell, optionally nudged
// by AnimGroupOffsetMod in half-cell increments.
// Group-local member coordinates and pivot coordinates are expressed in
// half-cell units relative to that anchor point.
// -----------------------------

#pragma pack(push, 1)

struct AnimPrimitiveDef {
    // Local position in half-cell units relative to the anchor cell center.
    // Example:
    //   hx = -2 means 1 full cell left of anchor center
    //   hy = +1 means 1/2 cell down from anchor center
    int8_t     hx;
    int8_t     hy;

    ObstacleId obstacle; // must be a primitive: Square/RightTri/HalfSpike/FullSpike/Star
    ShapeMod   mod;      // primitive-local orientation
};

static_assert(sizeof(AnimPrimitiveDef) == 4, "AnimPrimitiveDef must be 4 bytes");

struct AnimStepDef {
    uint8_t  type;               // AnimStepType
    int8_t   deltaQuarterTurns;  // signed: +1=+90, -3=-270; ignored for Hold
    uint16_t durationMs;         // segment duration in milliseconds
};

static_assert(sizeof(AnimStepDef) == 4, "AnimStepDef must be 4 bytes");

struct AnimGroupDefHeader {
    uint8_t  primitiveCount;     // number of AnimPrimitiveDef records that follow
    int8_t   pivotHx;            // pivot in half-cell units relative to anchor center
    int8_t   pivotHy;            // pivot in half-cell units relative to anchor center
    uint8_t  stepCount;          // number of AnimStepDef records that follow primitives
    uint16_t totalDurationMs;    // optional cached loop duration; 0 means compute at load/read
};

static_assert(sizeof(AnimGroupDefHeader) == 6, "AnimGroupDefHeader must be 6 bytes");

#pragma pack(pop)

// -----------------------------
// Helpers
// -----------------------------

inline bool is_anim_group(ObstacleId id) {
    return uint8_t(id) >= uint8_t(ObstacleId::AnimGroup1) &&
           uint8_t(id) <= uint8_t(ObstacleId::AnimGroup8);
}

inline uint8_t anim_group_index(ObstacleId id) {
    const uint8_t v = uint8_t(id);
    if (v < uint8_t(ObstacleId::AnimGroup1) || v > uint8_t(ObstacleId::AnimGroup8)) {
        return 0xFF;
    }
    return uint8_t(v - uint8_t(ObstacleId::AnimGroup1));
}

inline bool is_primitive_obstacle(ObstacleId id) {
    return uint8_t(id) >= uint8_t(ObstacleId::Empty) &&
        uint8_t(id) <= uint8_t(ObstacleId::Star);
}

inline bool is_anim_primitive_obstacle(ObstacleId id) {
    return uint8_t(id) >= uint8_t(ObstacleId::Square) && uint8_t(id) <= uint8_t(ObstacleId::Star);
}

inline uint16_t anim_group_def_size_bytes(const AnimGroupDefHeader& h) {
    return uint16_t(
        sizeof(AnimGroupDefHeader) +
        uint16_t(h.primitiveCount) * uint16_t(sizeof(AnimPrimitiveDef)) +
        uint16_t(h.stepCount)      * uint16_t(sizeof(AnimStepDef))
    );
}

inline uint16_t anim_group_def_total_duration_ms(const AnimGroupDefHeader& h,
                                                 const AnimStepDef* steps) {
    if (h.totalDurationMs != 0) {
        return h.totalDurationMs;
    }

    uint32_t total = 0;
    if (!steps) return 0;

    for (uint8_t i = 0; i < h.stepCount; ++i) {
        total += steps[i].durationMs;
    }

    return (total > 0xFFFFu) ? 0xFFFFu : uint16_t(total);
}

// -----------------------------
// File IO
// -----------------------------

inline bool read_header(FILE* f, LevelHeader& out) {
    if (!f) return false;
    if (std::fseek(f, 0, SEEK_SET) != 0) return false;
    if (std::fread(&out, 1, sizeof(LevelHeader), f) != sizeof(LevelHeader)) return false;

    if (std::memcmp(out.magic, "GVL", 3) != 0) return false;
    if (out.version != 2) return false;

    if (out.width == 0) return false;
    if (out.animDefCount > 8) return false;
    if (out.levelDataOffset < sizeof(LevelHeader)) return false;

    g_levelDataOffset = out.levelDataOffset;
    return true;
}

// Read animation-def header by direct def index (0..animDefCount-1) by walking
// the variable-sized anim-def section from sizeof(LevelHeader) up to levelDataOffset.
inline bool read_anim_group_def_header(FILE* f,
                                       const LevelHeader& h,
                                       uint8_t defIndex,
                                       AnimGroupDefHeader& out,
                                       long* outDefOffset = nullptr) {
    if (!f) return false;
    if (defIndex >= h.animDefCount) return false;

    long offset = long(sizeof(LevelHeader));
    for (uint8_t i = 0; i <= defIndex; ++i) {
        if (offset + long(sizeof(AnimGroupDefHeader)) > long(h.levelDataOffset)) {
            return false;
        }

        if (std::fseek(f, offset, SEEK_SET) != 0) return false;
        if (std::fread(&out, 1, sizeof(AnimGroupDefHeader), f) != sizeof(AnimGroupDefHeader)) {
            return false;
        }

        const uint16_t defSize = anim_group_def_size_bytes(out);
        if (defSize < sizeof(AnimGroupDefHeader)) return false;
        if (offset + long(defSize) > long(h.levelDataOffset)) return false;

        if (i == defIndex) {
            if (outDefOffset) *outDefOffset = offset;
            return true;
        }

        offset += long(defSize);
    }

    return false;
}

inline bool read_anim_group_primitives(FILE* f,
                                       const LevelHeader& h,
                                       uint8_t defIndex,
                                       const AnimGroupDefHeader& defHdr,
                                       AnimPrimitiveDef* out,
                                       uint8_t cap) {
    if (!f || !out) return false;
    if (defHdr.primitiveCount > cap) return false;

    long defOffset = 0;
    AnimGroupDefHeader tmp{};
    if (!read_anim_group_def_header(f, h, defIndex, tmp, &defOffset)) return false;

    const long primOffset = defOffset + long(sizeof(AnimGroupDefHeader));
    if (std::fseek(f, primOffset, SEEK_SET) != 0) return false;

    const size_t bytes = size_t(defHdr.primitiveCount) * sizeof(AnimPrimitiveDef);
    return std::fread(out, 1, bytes, f) == bytes;
}

inline bool read_anim_group_steps(FILE* f,
                                  const LevelHeader& h,
                                  uint8_t defIndex,
                                  const AnimGroupDefHeader& defHdr,
                                  AnimStepDef* out,
                                  uint8_t cap) {
    if (!f || !out) return false;
    if (defHdr.stepCount > cap) return false;

    long defOffset = 0;
    AnimGroupDefHeader tmp{};
    if (!read_anim_group_def_header(f, h, defIndex, tmp, &defOffset)) return false;

    const long stepOffset =
        defOffset +
        long(sizeof(AnimGroupDefHeader)) +
        long(defHdr.primitiveCount) * long(sizeof(AnimPrimitiveDef));

    if (std::fseek(f, stepOffset, SEEK_SET) != 0) return false;

    const size_t bytes = size_t(defHdr.stepCount) * sizeof(AnimStepDef);
    return std::fread(out, 1, bytes, f) == bytes;
}

// Convenience: read a full anim def into caller-provided buffers.
// Either buffer may be null if that part is not needed.
// Caps must be >= counts if the corresponding buffer is provided.
inline bool read_anim_group_def(FILE* f,
                                const LevelHeader& h,
                                uint8_t defIndex,
                                AnimGroupDefHeader& outHdr,
                                AnimPrimitiveDef* primOut,
                                uint8_t primCap,
                                AnimStepDef* stepOut,
                                uint8_t stepCap) {
    if (!read_anim_group_def_header(f, h, defIndex, outHdr)) return false;

    if (primOut) {
        if (!read_anim_group_primitives(f, h, defIndex, outHdr, primOut, primCap)) return false;
    } else if (outHdr.primitiveCount != 0) {
        return false;
    }

    if (stepOut) {
        if (!read_anim_group_steps(f, h, defIndex, outHdr, stepOut, stepCap)) return false;
    } else if (outHdr.stepCount != 0) {
        return false;
    }

    return true;
}

// Column i is located at: levelDataOffset + i*7
inline bool read_column(FILE* f, uint16_t i, Column56& out) {
    if (!f) return false;
    const long offset = long(g_levelDataOffset) + long(i) * long(kColumnBytes);
    if (std::fseek(f, offset, SEEK_SET) != 0) return false;
    return std::fread(out.b, 1, kColumnBytes, f) == kColumnBytes;
}

// Portal absolute X is (width-1) + portalDx
inline int portal_abs_x(const LevelHeader& h) {
    return int(h.width) - 1 + int(h.portalDx);
}

} // namespace gv