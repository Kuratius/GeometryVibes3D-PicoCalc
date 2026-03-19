#include "Project.hpp"
#include <cstdint>

#define unlikely(x) __builtin_expect((x), 0)

namespace gv {

// ---- small fixed helpers (local) ----
static inline int64_t iabs64(int64_t x) { return x < 0 ? -x : x; }

// Integer sqrt for uint64 (floor)
static uint32_t isqrt_u64(uint64_t x) {
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
    return res;
}

static fx dot3(const Vec3fx& a, const Vec3fx& b) {
    int64_t sx = (int64_t)a.x.raw() * (int64_t)b.x.raw();
    int64_t sy = (int64_t)a.y.raw() * (int64_t)b.y.raw();
    int64_t sz = (int64_t)a.z.raw() * (int64_t)b.z.raw();
    return fx::fromRaw((int32_t)((sx + sy + sz) >> fx::SHIFT));
}

static Vec3fx cross3(const Vec3fx& a, const Vec3fx& b) {
    int64_t ax = a.x.raw(), ay = a.y.raw(), az = a.z.raw();
    int64_t bx = b.x.raw(), by = b.y.raw(), bz = b.z.raw();

    Vec3fx r;
    r.x = fx::fromRaw((int32_t)(((ay * bz) - (az * by)) >> fx::SHIFT));
    r.y = fx::fromRaw((int32_t)(((az * bx) - (ax * bz)) >> fx::SHIFT));
    r.z = fx::fromRaw((int32_t)(((ax * by) - (ay * bx)) >> fx::SHIFT));
    return r;
}

static inline Vec3fx sub3(const Vec3fx& a, const Vec3fx& b) {
    return Vec3fx{ a.x - b.x, a.y - b.y, a.z - b.z };
}


static uint32_t sqrt_core(uint32_t xyy, uint32_t y)
{
    //this can be reused for a sqrt instead of rsqrt
    //by setting y to the correct value
    xyy>>=1;
    uint32_t t=xyy+(xyy>>1);
    if (t < (1u<<31)){ //first iteration is special cased
        t+=t>>1;
        if (t < (1u<<31)) {
            xyy=t;
            y+=y>>1;
        }
    }
    //you can probably get 10-20% more speed out of this with inline assembler
    //or an unroll
    //but thumb assembly is torture compared to arm
    for (uint32_t i =2; i<8; i+=1)
    {
        uint32_t t=xyy+(xyy>>i);
        t+=t>>i;
        if (t < (1u<<31)) {
            xyy=t;
            y+=y>>i;
        }
    }
    //the following steps perform a hidden newton iteration
    //the calculation above guarantees x wont have bit 31 set
    //so we only need to shift down by 15
    y>>=16;
    uint32_t hi =(xyy>>15)*(y);
    y<<=16;
    //using the upper bits of a 32x32->64 multiply
    //improves accuracy, but
    //the lower 16 digits are noise anyway
    //and we dont have widening multiply on cortex-m0plus
    y+=y>>1; //this can overflow
    return ((y)-(hi>>1) ); //but then this underflows so they cancel
}


static inline uint32_t sqrt64_helper(uint64_t m, int * exp)
{
    int clz=__builtin_clzll(m);
    int ilog2=63-clz;
    ilog2>>=1;
    ilog2<<=1;
    int shift=30-ilog2;
    m= shift>=0 ? m<<shift : m>>-shift;
    int shift2=30-(shift>>1);
    *exp=shift2;
    return sqrt_core(m, 1<<30);
}

void normalize_32(int32_t * a)
{
    uint64_t squared_magnitude = 0;
    for (int i=0;i<3;i++)
    {
        uint32_t abs=a[i]>=0 ? a[i] : -a[i] ;
        if (unlikely((abs>>16)==0))
        {
            squared_magnitude+=abs*abs;

        } else
        {
            squared_magnitude+=(uint64_t)abs*abs;
        }
    }
    if (unlikely(squared_magnitude==0))
        return;
    int exp;
    uint32_t res=sqrt64_helper(squared_magnitude, &exp)>>16;
    exp+=-16;
    for(int i=0; i<3;i++)
    {
        int32_t t=a[i];
        uint32_t tu=t;
        if (t<0)
            tu=-tu;
        //32x16->48 bit multiply
        uint64_t prod=((tu<<16)>>16)*res;
        prod+=(uint64_t)((tu>>16)*res)<<16;
        prod= exp >=0 ? prod>>exp : prod<<-exp;
        int32_t sprod=prod;
        if (t<0)
            sprod=-sprod;

        a[i]=sprod;
    }
    return;
}
#if 0
Vec3fx normalize3(const Vec3fx& v) {
    int64_t x = v.x.raw(), y = v.y.raw(), z = v.z.raw();
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
#else
static inline Vec3fx normalize3(const Vec3fx& v)
{
	int32_t a[3]={v.x.raw(), v.y.raw(), v.z.raw()};
	normalize_32(&a[0]);
	Vec3fx out;
	out.x=fx::fromRaw(a[0]);
	out.y=fx::fromRaw(a[1]);
	out.z=fx::fromRaw(a[2]);
	return out;
}
#endif




static int32_t projectToNormal(int32_t* a, int32_t *normalVector)
{
    int64_t sum=0;
    for (int i=0; i<3;i++)
    {
        int32_t ta=a[i]>>8;
        int32_t tb=normalVector[i]>>8;
        //workaround until I figure out a better way
        sum+=ta*tb;
    }
    return sum;
}

fx static inline projectNormal3(Vec3fx v1, Vec3fx v2){

    int32_t a[3]={v1.x.raw(),v1.y.raw(), v1.z.raw()};
    int32_t b[3]={v2.x.raw(),v2.y.raw(), v2.z.raw()};

    return fx::fromRaw(projectToNormal(&a[0], &b[0]));
}




static void normal_cross(int32_t* a, int32_t *b,int32_t * resultVector)
{
    int32_t ta[3] = {a[0]>>1, a[1]>>1, a[2]>>1};
    int32_t tb[3] = {b[0]>>1, b[1]>>1, b[2]>>1};
    //the above is just so we dont need the restrict qualifier
    //otherwise the compiler assumes they might overlap with the result
    resultVector[0] = ((int32_t)ta[1] * tb[2] + (int32_t)-tb[1] * ta[2]) >> 14;
    resultVector[1] = ((int32_t)ta[2] * tb[0] + (int32_t)tb[2] * -ta[0]) >> 14;
    resultVector[2] = ((int32_t)-ta[0] * -tb[1] + (int32_t)-tb[0] * ta[1]) >> 14;
}

Vec3fx static inline cross3_normal(Vec3fx v1, Vec3fx v2)
{
    int32_t a[3]={v1.x.raw(),v1.y.raw(), v1.z.raw()};
    int32_t b[3]={v2.x.raw(),v2.y.raw(), v2.z.raw()};
    int32_t result[3];
    normal_cross(&a[0], &b[0], &result[0]);
	Vec3fx out;
	out.x=fx::fromRaw(result[0]);
	out.y=fx::fromRaw(result[1]);
	out.z=fx::fromRaw(result[2]);
    return out;
}


void buildCameraBasis(Camera& cam) {
    Vec3fx tgt = cam.target;
//to do: probably use 2.14 or 4.12 format for normalized vectors
//to avoid 64 bit multiplications and do 32-bit muls instead
    cam.fwd   = normalize3(sub3(tgt, cam.pos));
    //cam.up    = normalize3(cam.up); //this probably isnt necessary?
    cam.right = cross3_normal(cam.fwd, cam.up);
    cam.up2   = cross3_normal(cam.right, cam.fwd);
}





bool projectPoint(const Camera& cam, const Vec3fx& world, Vec2i& out)
{
    // Transform world -> view using precomputed basis
    Vec3fx v = sub3(world, cam.pos);
    fx x = projectNormal3(v, cam.right);
    fx y = projectNormal3(v, cam.up2);
    fx z = projectNormal3(v, cam.fwd);

    if (z.raw() <= (1 << fx::SHIFT) / 8) return false;

    fx invz = cam.focal / z;
    fx sx = cam.cx + x * invz;
    fx sy = cam.cy - y * invz;

    out.x = (int16_t)sx.toInt();
    out.y = (int16_t)sy.toInt();
    return true;
}

} // namespace gv
