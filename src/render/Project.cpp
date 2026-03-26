#include "Project.hpp"
#include <cstdint>

namespace gv {

#define unlikely(x) __builtin_expect(!!(x), 0)

// Set to 1 to use the fast approximate normalization path.
// Set to 0 to use the original divide-by-length normalization.
#define GV_FAST_NORMALIZE 1

// ---- small fixed-point helpers (local) ----
static inline Vec3fx sub3(const Vec3fx& a, const Vec3fx& b) {
    return Vec3fx{ a.x - b.x, a.y - b.y, a.z - b.z };
}

static inline uint32_t uxth(uint32_t a) {
    //this is a workaround for a compiler bug affecting up to GCC 15.2
    if (__builtin_constant_p(a))
        return (uint16_t)a;

    uint32_t b;
    asm("uxth    %[b], %[a]"
    :[b]"=l"(b)
    :[a]"l"(a)
    ://no clobber
    );
    return b;
}

static inline int64_t smul48_i16(int32_t n, int16_t m) {
//signed 32x16->48-bit multiply
    int64_t temp=(int64_t)((n>>16)*m)<<16;
    temp+=(int32_t)uxth(n)*m;
    return temp;
}

static inline uint64_t umul48_u16(uint32_t n, uint16_t m) {
//unsigned 32x16->48-bit multiply
    uint64_t temp=uxth(n)*m;
    temp+=(uint64_t)((n>>16)*m)<<16;
    return temp;
}

static inline int32_t roundNearest(int32_t a, const int shift) {
    //asrs doesnt update carry flag for shift by 0
    //this branch should get pruned, but it is important
    //if you want to reuse this code elsewhere
    if (shift<=0)
        return a << -shift;
    //compiler can't run assembly code
    if (__builtin_constant_p(a))
        return (int32_t)(((int64_t)a+(1<<(shift-1)))>>shift);

    //round to nearest, ties to positive infinity
    asm(".syntax unified\n\t"
        "asrs    %[a], %[a], %[shift]\n\t"
        "adcs    %[a], %[a], %[zero]"
    :[a]"+&l"(a)//output
    :[shift]"lI"(shift), [zero]"l"(0)//inputs
    :"cc" //clobber
    );
    //note:
    //asrs sets the carry flag based on the last digit
    //that was shifted out
    //"lI" means the compiler
    //can choose between immediate value and  a low register (r0-r7)
    //adc on thumb does not support an immediate value
    //hence the "l" constraint instead of I
    //+l means the register is both input and output
    //+&l prevents input regs from overlapping with it
    //(important for multi-line statements)
    return a;
}

#if GV_FAST_NORMALIZE

static uint32_t sqrtCore(uint32_t xyy, uint32_t y) {
    //this can be reused for a sqrt instead of rsqrt
    //by setting y to the correct value
    xyy >>= 1;
    uint32_t t = xyy + (xyy >> 1);
    if (t < (1u << 31)) { //first iteration is special cased
        t += t >> 1;
        if (t < (1u << 31)) {
            xyy = t;
            y += y >> 1;
        }
    }
    //you can probably get 10-20% more speed out of this with inline assembler
    //but thumb assembly is torture compared to arm
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 6
#endif
    for (uint32_t i = 2; i < 8; i += 1) {
        uint32_t t = xyy + (xyy >> i);
        t += t >> i;
        if (t < (1u << 31)) {
            xyy = t;
            y += y >> i;
        }
    }
    //the following steps perform a hidden newton iteration
    //the calculation above guarantees xyy wont have bit 31 set
    //so we only need to shift down by 15
    y >>= 16;
    uint32_t hi = (xyy >> 15) * y;
    y <<= 16;
    //using the upper bits of a 32x32->64 multiply
    //improves accuracy, but
    //the lower 16 digits are noise anyway
    //and we dont have widening multiply on cortex-m0plus
    y += y >> 1; //this can overflow
    return (y - (hi >> 1)); //but then this underflows so they cancel
}

static inline uint32_t sqrt64Helper(uint64_t m, int* exp) {
    int clz = __builtin_clzll(m);
    int ilog2 = 63 - clz;
    ilog2 &= ~1; // round down to even
    int shift = 30 - ilog2;
    m = (shift >= 0) ? (m << shift) : (m >> -shift);
    int shift2 = 30 - (shift >> 1);
    *exp = shift2;
    return sqrtCore((uint32_t)m, 1u << 30);
}

static void normalize32(int32_t* a) {
    uint64_t squared_magnitude = 0;
    for (int i = 0; i < 3; i++) {
        uint32_t abs = (a[i] >= 0) ? (uint32_t)a[i] : (uint32_t)-a[i];
        if (unlikely((abs >> 16) == 0)) {
            squared_magnitude += abs * abs;
        } else {
            squared_magnitude += (uint64_t)abs * abs;
        }
    }
    if (unlikely(squared_magnitude == 0))
        return;

    int exp = 0;
    uint32_t res = sqrt64Helper(squared_magnitude, &exp) >> 16;
    exp += -16;

    for (int i = 0; i < 3; i++) {
        int32_t t = a[i];
        uint32_t tu = (uint32_t)t;
        if (t < 0)
            tu = -tu;
        //32x16->48 bit multiply
        uint64_t prod=umul48_u16(tu, res);
        prod = (exp >= 0) ? (prod >> exp) : (prod << -exp);
        int32_t sprod = (int32_t)prod;
        if (t < 0)
            sprod = -sprod;

        a[i] = sprod;
    }
}

static inline Vec3fx normalize3(const Vec3fx& v) {
    int32_t a[3] = { v.x.raw(), v.y.raw(), v.z.raw() };
    normalize32(a);

    Vec3fx out;
    out.x = fx::fromRaw(a[0]);
    out.y = fx::fromRaw(a[1]);
    out.z = fx::fromRaw(a[2]);
    return out;
}

#else

static inline int64_t iabs64(int64_t x) { return x < 0 ? -x : x; }

// Integer sqrt for uint64 (floor)
static inline uint32_t isqrt_u64(uint64_t x) {
    uint64_t op = x;
    uint64_t res = 0;
    uint64_t one = 1ULL << 62; // highest power of four <= 2^64

    while (one > op) one >>= 2;
    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return (uint32_t)res;
}

static inline fx dot3(const Vec3fx& a, const Vec3fx& b) {
    int64_t sx = (int64_t)a.x.raw() * (int64_t)b.x.raw();
    int64_t sy = (int64_t)a.y.raw() * (int64_t)b.y.raw();
    int64_t sz = (int64_t)a.z.raw() * (int64_t)b.z.raw();
    return fx::fromRaw((int32_t)((sx + sy + sz) >> fx::SHIFT));
}

static inline Vec3fx cross3(const Vec3fx& a, const Vec3fx& b) {
    int64_t ax = a.x.raw(), ay = a.y.raw(), az = a.z.raw();
    int64_t bx = b.x.raw(), by = b.y.raw(), bz = b.z.raw();

    Vec3fx r;
    r.x = fx::fromRaw((int32_t)(((ay * bz) - (az * by)) >> fx::SHIFT));
    r.y = fx::fromRaw((int32_t)(((az * bx) - (ax * bz)) >> fx::SHIFT));
    r.z = fx::fromRaw((int32_t)(((ax * by) - (ay * bx)) >> fx::SHIFT));
    return r;
}

static inline Vec3fx normalize3(const Vec3fx& v) {
    int64_t x = v.x.raw();
    int64_t y = v.y.raw();
    int64_t z = v.z.raw();
    uint64_t xx = (uint64_t)(iabs64(x)) * (uint64_t)(iabs64(x));
    uint64_t yy = (uint64_t)(iabs64(y)) * (uint64_t)(iabs64(y));
    uint64_t zz = (uint64_t)(iabs64(z)) * (uint64_t)(iabs64(z));
    uint64_t sum = xx + yy + zz;
    if (sum == 0)
        return Vec3fx{ fx::zero(), fx::zero(), fx::zero() };
    // sum is Q32.32, sqrt -> Q16.16 in raw units
    uint32_t lenRaw = isqrt_u64(sum);

    Vec3fx out;
    out.x = fx::fromRaw((int32_t)(((int64_t)v.x.raw() << fx::SHIFT) / (int64_t)lenRaw));
    out.y = fx::fromRaw((int32_t)(((int64_t)v.y.raw() << fx::SHIFT) / (int64_t)lenRaw));
    out.z = fx::fromRaw((int32_t)(((int64_t)v.z.raw() << fx::SHIFT) / (int64_t)lenRaw));
    return out;
}

#endif

// Preserve 4 extra bits from `normalVector` versus the old path.
// Old behavior: `tb = normalVector[i] >> 8;` and `return sum;`
// New behavior: `tb = normalVector[i] >> 4;` and `return sum >> 4;`
// Net scale stays the same, but the multiply keeps more normal-vector precision.
static int32_t projectToNormal(const int32_t* a, const int32_t* normalVector) {
    int32_t sum = 0;
    for (int i = 0; i < 3; i++) {
        int32_t ta = a[i] >> 8;
        int32_t tb = normalVector[i] >> 4;
        sum += ta * tb;
    }
    return sum>>4;
}

static inline fx projectNormal3(const Vec3fx& v1, const Vec3fx& v2) {
    int32_t a[3] = { v1.x.raw(), v1.y.raw(), v1.z.raw() };
    int32_t b[3] = { v2.x.raw(), v2.y.raw(), v2.z.raw() };

    return fx::fromRaw(projectToNormal(&a[0], &b[0]));
}

static void normalCross(const int32_t* a, const int32_t* b, int32_t* resultVector) {
    int32_t ta[3] = { a[0] >> 1, a[1] >> 1, a[2] >> 1 };
    int32_t tb[3] = { b[0] >> 1, b[1] >> 1, b[2] >> 1 };
    //the above is just so we dont need the restrict qualifier
    //otherwise the compiler assumes they might overlap with the result
    resultVector[0] = ((int32_t)ta[1] * tb[2] - (int32_t)ta[2] * tb[1]) >> 14;
    resultVector[1] = ((int32_t)ta[2] * tb[0] - (int32_t)ta[0] * tb[2]) >> 14;
    resultVector[2] = ((int32_t)ta[0] * tb[1] - (int32_t)ta[1] * tb[0]) >> 14;
}

static inline Vec3fx cross3Normal(const Vec3fx& v1, const Vec3fx& v2) {
    int32_t a[3] = { v1.x.raw(), v1.y.raw(), v1.z.raw() };
    int32_t b[3] = { v2.x.raw(), v2.y.raw(), v2.z.raw() };
    int32_t result[3];
    normalCross(&a[0], &b[0], &result[0]);

    Vec3fx out;
    out.x = fx::fromRaw(result[0]);
    out.y = fx::fromRaw(result[1]);
    out.z = fx::fromRaw(result[2]);
    return out;
}

void buildCameraBasis(Camera& cam) {
    Vec3fx tgt = cam.target;
    cam.fwd   = normalize3(sub3(tgt, cam.pos));
    //cam.up    = normalize3(cam.up); // Assume cam.up is already normalized.
    cam.right = cross3Normal(cam.fwd, cam.up);
    cam.up2   = cross3Normal(cam.right, cam.fwd);
}

bool projectPoint(const Camera& cam, const Vec3fx& world, Vec2i& out) {
    // Transform world -> view using precomputed basis
    Vec3fx v = sub3(world, cam.pos);
    fx z = projectNormal3(v, cam.fwd);
    int32_t zt=z.raw();
    zt>>=16;
    //early return
    if (unlikely(zt < 1)) return false;

    fx x = projectNormal3(v, cam.right);
    fx y = projectNormal3(v, cam.up2);
    int32_t focal=cam.focal.raw()>>16;//focal is an integer

    int32_t xt=x.raw();
#ifdef USE_HIGH_ACCURACY
    int32_t xtf=(smul48_i16(xt,focal)<<16)/z.raw();
    out.x = (int16_t)roundNearest(cam.cx.raw() + xtf, 16);
#else
    //this branch requires focal < 1024
    int32_t xtf=(((xt>>10)*focal))/zt;
    out.x = (int16_t)roundNearest((cam.cx.raw()>>10) + xtf, 6);
#endif

    int32_t yt=y.raw();
#ifdef USE_HIGH_ACCURACY
    int32_t ytf=(smul48_i16(yt,focal)<<16)/z.raw();
    out.y = (int16_t)roundNearest(cam.cy.raw() - ytf, 16);
#else
    //this branch requires focal < 1024
    int32_t ytf=(((yt>>10)*focal))/zt;
    out.y = (int16_t)roundNearest((cam.cy.raw()>>10) - ytf, 6);
#endif
    return true;
}


} // namespace gv
