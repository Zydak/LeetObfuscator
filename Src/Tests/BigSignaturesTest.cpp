#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <iostream>

#define LEET_IMPLEMENTATION
#include "../Leet.h"

struct SmallS {
    int a;
    float b;
};

struct MedS {
    int x, y, z;
    double d1, d2;
    char c;
};

struct BigS {
    int arr[8];
    float farr[4];
    double d;
    void* p;
    MedS nested;
};

struct PackedS {
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    int64_t i64;
};

__attribute__((noinline))
static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

__attribute__((noinline))
static inline uint64_t add_f(uint64_t h, float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return mix64(h ^ u);
}

__attribute__((noinline))
static inline uint64_t add_d(uint64_t h, double d) {
    uint64_t u;
    std::memcpy(&u, &d, sizeof(u));
    return mix64(h ^ u);
}

__attribute__((noinline)) static float  radd(float  a, float  b) { return a + b; }
__attribute__((noinline)) static double radd(double a, double b) { return a + b; }

template <typename T>
static inline T strict_sum(T a, T b) { return radd(a, b); }

template <typename T, typename... Rest>
static inline T strict_sum(T a, T b, Rest... rest) { return strict_sum(radd(a, b), rest...); }

__attribute__((noinline))
uint64_t f01(
    int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7,
    int a8, int a9, float b0, float b1, float b2, float b3, float b4,
    float b5, float b6, float b7, double c0, double c1, double c2, double c3,
    int* p0, int* p1, float* p2, double* p3, SmallS s0, SmallS s1,
    MedS m0, MedS m1, BigS* bp, PackedS pk, char ch, short sh, long ln,
    long long ll, unsigned u, bool bl, void* vp, size_t sz)
{
    uint64_t h = 0x123456789abcdef0ULL;
    h = mix64(h ^ (uint64_t)(a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9));
    h = add_f(h, strict_sum(b0, b1, b2, b3, b4, b5, b6, b7));
    h = add_d(h, strict_sum(c0, c1, c2, c3));
    if (p0) h = mix64(h ^ (uint64_t)*p0);
    if (p1) h = mix64(h ^ (uint64_t)*p1);
    if (p2) h = add_f(h, *p2);
    if (p3) h = add_d(h, *p3);
    h = mix64(h ^ (uint64_t)(s0.a + s1.a));
    h = add_f(h, strict_sum(s0.b, s1.b));
    h = mix64(h ^ (uint64_t)(m0.x + m0.y + m0.z + m1.x + m1.y + m1.z));
    h = add_d(h, strict_sum(m0.d1, m0.d2, m1.d1, m1.d2));
    if (bp) {
        for (int i = 0; i < 8; ++i) h = mix64(h ^ (uint64_t)bp->arr[i]);
        for (int i = 0; i < 4; ++i) h = add_f(h, bp->farr[i]);
        h = add_d(h, bp->d);
    }
    h = mix64(h ^ pk.u8 ^ pk.u16 ^ pk.u32 ^ (uint64_t)pk.i64);
    h = mix64(h ^ (uint64_t)ch ^ (uint64_t)sh ^ (uint64_t)ln ^ (uint64_t)ll ^ u ^ (bl ? 1 : 0) ^ sz);
    if (vp) h = mix64(h ^ (uintptr_t)vp);
    return h;
}

__attribute__((noinline))
uint64_t f02(
    double d0, double d1, double d2, double d3, double d4, double d5,
    int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9,
    float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7,
    SmallS* sp0, SmallS* sp1, MedS m, BigS b, PackedS* pkp, int* ip, float* fp,
    double* dp, char* cp, short* shp, long* lp, long long* llp, unsigned* up,
    bool* bp, void** vpp, size_t* szp)
{
    uint64_t h = 0xdeadbeefcafebabeULL;
    h = add_d(h, strict_sum(d0, d1, d2, d3, d4, d5));
    h = mix64(h ^ (uint64_t)(i0*i1 + i2*i3 + i4*i5 + i6*i7 + i8*i9));
    h = add_f(h, strict_sum(f0*f1, f2*f3, f4*f5, f6*f7));
    if (sp0) { h = mix64(h ^ (uint64_t)sp0->a); h = add_f(h, sp0->b); }
    if (sp1) { h = mix64(h ^ (uint64_t)sp1->a); h = add_f(h, sp1->b); }
    h = mix64(h ^ (uint64_t)(m.x ^ m.y ^ m.z)); h = add_d(h, m.d1 * m.d2);
    for (int i = 0; i < 8; ++i) h = mix64(h ^ (uint64_t)b.arr[i]);
    for (int i = 0; i < 4; ++i) h = add_f(h, b.farr[i]);
    h = add_d(h, b.d);
    if (pkp) h = mix64(h ^ pkp->u8 ^ pkp->u16 ^ pkp->u32 ^ (uint64_t)pkp->i64);
    if (ip) h = mix64(h ^ (uint64_t)*ip);
    if (fp) h = add_f(h, *fp);
    if (dp) h = add_d(h, *dp);
    if (cp) h = mix64(h ^ (uint64_t)*cp);
    if (shp) h = mix64(h ^ (uint64_t)*shp);
    if (lp) h = mix64(h ^ (uint64_t)*lp);
    if (llp) h = mix64(h ^ (uint64_t)*llp);
    if (up) h = mix64(h ^ *up);
    if (bp) h = mix64(h ^ (*bp ? 0x55 : 0xaa));
    if (vpp && *vpp) h = mix64(h ^ (uintptr_t)*vpp);
    if (szp) h = mix64(h ^ *szp);
    return h;
}

__attribute__((noinline))
uint64_t f03(
    int v00, int v01, int v02, int v03, int v04, int v05, int v06, int v07,
    int v08, int v09, int v10, int v11, int v12, int v13, int v14, int v15,
    float f00, float f01, float f02, float f03, float f04, float f05, float f06, float f07,
    float f08, float f09, double d00, double d01, double d02, double d03,
    SmallS s, MedS m, BigS* b, PackedS p, int* pi, float* pf, double* pd,
    void* pv, char c, bool bflag)
{
    uint64_t h = 0x0f1e2d3c4b5a6978ULL;
    int sumi = 0;
    for (int i = 0; i < 16; ++i) {
    }
    sumi = v00+v01+v02+v03+v04+v05+v06+v07+v08+v09+v10+v11+v12+v13+v14+v15;
    h = mix64(h ^ (uint64_t)sumi);
    float sumf = strict_sum(f00, f01, f02, f03, f04, f05, f06, f07, f08, f09);
    h = add_f(h, sumf);
    h = add_d(h, strict_sum(d00, d01, d02, d03));
    h = mix64(h ^ (uint64_t)s.a); h = add_f(h, s.b);
    h = mix64(h ^ (uint64_t)(m.x+m.y+m.z)); h = add_d(h, strict_sum(m.d1, m.d2));
    if (b) {
        h = mix64(h ^ (uint64_t)b->arr[0] ^ b->arr[7]);
        h = add_f(h, strict_sum(b->farr[0], b->farr[3]));
        h = add_d(h, b->d);
    }
    h = mix64(h ^ p.u8 ^ p.u16 ^ p.u32 ^ (uint64_t)p.i64);
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    h = mix64(h ^ (uint64_t)c ^ (bflag ? 1ULL : 0ULL));
    return h;
}

__attribute__((noinline))
uint64_t f04(
    PackedS p0, PackedS p1, PackedS p2, PackedS p3,
    BigS b0, BigS b1,
    MedS m0, MedS m1, MedS m2, MedS m3,
    SmallS s0, SmallS s1, SmallS s2, SmallS s3,
    int i0, int i1, int i2, int i3, int i4, int i5,
    float f0, float f1, float f2, float f3,
    double d0, double d1, double d2, double d3,
    int* pi, float* pf, double* pd, void* pv,
    char c0, char c1, short sh, long lg, long long ll, unsigned u,
    bool bl, size_t sz)
{
    uint64_t h = 0xa5a5a5a5a5a5a5a5ULL;
    h = mix64(h ^ p0.u32 ^ p1.u32 ^ p2.u32 ^ p3.u32);
    h = mix64(h ^ (uint64_t)(p0.i64 + p1.i64 + p2.i64 + p3.i64));
    h = mix64(h ^ (uint64_t)(b0.arr[0] + b1.arr[0] + b0.arr[7] + b1.arr[7]));
    h = add_f(h, strict_sum(b0.farr[0], b1.farr[0]));
    h = add_d(h, strict_sum(b0.d, b1.d));
    h = mix64(h ^ (uint64_t)(m0.x + m1.y + m2.z + m3.x));
    h = add_d(h, strict_sum(m0.d1, m1.d2, m2.d1, m3.d2));
    h = mix64(h ^ (uint64_t)(s0.a + s1.a + s2.a + s3.a));
    h = add_f(h, strict_sum(s0.b, s1.b, s2.b, s3.b));
    h = mix64(h ^ (uint64_t)(i0+i1+i2+i3+i4+i5));
    h = add_f(h, strict_sum(f0, f1, f2, f3));
    h = add_d(h, strict_sum(d0, d1, d2, d3));
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    h = mix64(h ^ (uint64_t)c0 ^ (uint64_t)c1 ^ (uint64_t)sh ^ (uint64_t)lg ^ (uint64_t)ll ^ u);
    h = mix64(h ^ (bl ? 0x1111 : 0x2222) ^ sz);
    return h;
}

__attribute__((noinline))
uint64_t f05(
    int* a0, int* a1, int* a2, int* a3, int* a4, int* a5, int* a6, int* a7,
    float* f0, float* f1, float* f2, float* f3, float* f4, float* f5, float* f6, float* f7,
    double* d0, double* d1, double* d2, double* d3,
    SmallS* s0, SmallS* s1, MedS* m0, MedS* m1, BigS* b0, BigS* b1,
    PackedS* p0, PackedS* p1, char* c0, char* c1, short* sh0, short* sh1,
    long* l0, long long* ll0, unsigned* u0, bool* bl0, void* v0, void* v1,
    size_t sz0, size_t sz1)
{
    uint64_t h = 0x1122334455667788ULL;
    auto safe_i = [](int* p) -> int { return p ? *p : 0; };
    auto safe_f = [](float* p) -> float { return p ? *p : 0.f; };
    auto safe_d = [](double* p) -> double { return p ? *p : 0.0; };

    h = mix64(h ^ (uint64_t)(safe_i(a0)+safe_i(a1)+safe_i(a2)+safe_i(a3)+safe_i(a4)+safe_i(a5)+safe_i(a6)+safe_i(a7)));
    h = add_f(h, strict_sum(safe_f(f0), safe_f(f1), safe_f(f2), safe_f(f3), safe_f(f4), safe_f(f5), safe_f(f6), safe_f(f7)));
    h = add_d(h, strict_sum(safe_d(d0), safe_d(d1), safe_d(d2), safe_d(d3)));
    if (s0) { h = mix64(h ^ (uint64_t)s0->a); h = add_f(h, s0->b); }
    if (s1) { h = mix64(h ^ (uint64_t)s1->a); h = add_f(h, s1->b); }
    if (m0) { h = mix64(h ^ (uint64_t)(m0->x+m0->y+m0->z)); h = add_d(h, strict_sum(m0->d1, m0->d2)); }
    if (m1) { h = mix64(h ^ (uint64_t)(m1->x+m1->y+m1->z)); h = add_d(h, strict_sum(m1->d1, m1->d2)); }
    if (b0) {
        h = mix64(h ^ (uint64_t)b0->arr[3]); h = add_f(h, b0->farr[1]); h = add_d(h, b0->d);
    }
    if (b1) {
        h = mix64(h ^ (uint64_t)b1->arr[3]); h = add_f(h, b1->farr[1]); h = add_d(h, b1->d);
    }
    if (p0) h = mix64(h ^ p0->u32 ^ (uint64_t)p0->i64);
    if (p1) h = mix64(h ^ p1->u32 ^ (uint64_t)p1->i64);
    if (c0) h = mix64(h ^ (uint64_t)*c0);
    if (c1) h = mix64(h ^ (uint64_t)*c1);
    if (sh0) h = mix64(h ^ (uint64_t)*sh0);
    if (sh1) h = mix64(h ^ (uint64_t)*sh1);
    if (l0) h = mix64(h ^ (uint64_t)*l0);
    if (ll0) h = mix64(h ^ (uint64_t)*ll0);
    if (u0) h = mix64(h ^ *u0);
    if (bl0) h = mix64(h ^ (*bl0 ? 1 : 0));
    if (v0) h = mix64(h ^ (uintptr_t)v0);
    if (v1) h = mix64(h ^ (uintptr_t)v1);
    h = mix64(h ^ sz0 ^ sz1);
    return h;
}

__attribute__((noinline))
uint64_t f06(
    int x0, float y0, double z0, int x1, float y1, double z1,
    int x2, float y2, double z2, int x3, float y3, double z3,
    int x4, float y4, double z4, int x5, float y5, double z5,
    int x6, float y6, double z6, int x7, float y7, double z7,
    int x8, float y8, double z8, int x9, float y9, double z9,
    SmallS s0, SmallS s1, MedS m0, MedS m1, BigS b, PackedS p,
    int* pi, float* pf, double* pd, void* pv)
{
    uint64_t h = 0x9988776655443322ULL;
    h = mix64(h ^ (uint64_t)(x0+x1+x2+x3+x4+x5+x6+x7+x8+x9));
    h = add_f(h, strict_sum(y0, y1, y2, y3, y4, y5, y6, y7, y8, y9));
    h = add_d(h, strict_sum(z0, z1, z2, z3, z4, z5, z6, z7, z8, z9));
    h = mix64(h ^ (uint64_t)(s0.a ^ s1.a)); h = add_f(h, s0.b * s1.b);
    h = mix64(h ^ (uint64_t)(m0.x + m1.y)); h = add_d(h, strict_sum(m0.d1, m1.d2));
    h = mix64(h ^ (uint64_t)b.arr[0] ^ b.arr[4]); h = add_f(h, b.farr[2]); h = add_d(h, b.d);
    h = mix64(h ^ p.u8 ^ p.u16 ^ p.u32 ^ (uint64_t)p.i64);
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    return h;
}

__attribute__((noinline))
uint64_t f07(
    BigS b0, BigS b1, BigS b2,
    MedS m0, MedS m1, MedS m2, MedS m3,
    SmallS s0, SmallS s1, SmallS s2, SmallS s3, SmallS s4, SmallS s5,
    PackedS p0, PackedS p1, PackedS p2,
    int i0, int i1, int i2, int i3, float f0, float f1, float f2, float f3,
    double d0, double d1, int* pi, float* pf, double* pd, void* pv,
    char c, short sh, long lg, long long ll, unsigned u, bool bl, size_t sz, int pad)
{
    uint64_t h = 0x7f6e5d4c3b2a1908ULL;
    h = mix64(h ^ (uint64_t)(b0.arr[1] + b1.arr[2] + b2.arr[3]));
    h = add_f(h, strict_sum(b0.farr[0], b1.farr[1], b2.farr[2]));
    h = add_d(h, strict_sum(b0.d, b1.d, b2.d));
    h = mix64(h ^ (uint64_t)(m0.x + m1.y + m2.z + m3.x));
    h = add_d(h, strict_sum(m0.d1, m1.d2, m2.d1, m3.d2));
    h = mix64(h ^ (uint64_t)(s0.a + s1.a + s2.a + s3.a + s4.a + s5.a));
    h = add_f(h, strict_sum(s0.b, s1.b, s2.b, s3.b, s4.b, s5.b));
    h = mix64(h ^ p0.u32 ^ p1.u32 ^ p2.u32);
    h = mix64(h ^ (uint64_t)(i0+i1+i2+i3));
    h = add_f(h, strict_sum(f0, f1, f2, f3));
    h = add_d(h, strict_sum(d0, d1));
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    h = mix64(h ^ (uint64_t)c ^ (uint64_t)sh ^ (uint64_t)lg ^ (uint64_t)ll ^ u ^ (bl?1:0) ^ sz ^ (uint64_t)pad);
    return h;
}

__attribute__((noinline))
uint64_t f08(
    int i00, int i01, int i02, int i03, int i04, int i05, int i06, int i07, int i08, int i09,
    int i10, int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19,
    float f00, float f01, float f02, float f03, float f04, float f05, float f06, float f07, float f08, float f09,
    double d00, double d01, double d02, double d03, double d04,
    SmallS s, MedS m, BigS* b, PackedS p, int* pi, float* pf, double* pd, void* pv, char c)
{
    uint64_t h = 0x0123456789abcdefULL;
    int isum = i00+i01+i02+i03+i04+i05+i06+i07+i08+i09+i10+i11+i12+i13+i14+i15+i16+i17+i18+i19;
    h = mix64(h ^ (uint64_t)isum);
    float fsum = strict_sum(f00, f01, f02, f03, f04, f05, f06, f07, f08, f09);
    h = add_f(h, fsum);
    h = add_d(h, strict_sum(d00, d01, d02, d03, d04));
    h = mix64(h ^ (uint64_t)s.a); h = add_f(h, s.b);
    h = mix64(h ^ (uint64_t)(m.x+m.y+m.z)); h = add_d(h, strict_sum(m.d1, m.d2));
    if (b) {
        h = mix64(h ^ (uint64_t)b->arr[5]); h = add_f(h, b->farr[0]); h = add_d(h, b->d);
    }
    h = mix64(h ^ p.u8 ^ p.u16 ^ p.u32 ^ (uint64_t)p.i64);
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    h = mix64(h ^ (uint64_t)c);
    return h;
}

__attribute__((noinline))
uint64_t f09(
    double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7,
    float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9,
    int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9,
    SmallS* s0, SmallS* s1, MedS* m0, MedS* m1, BigS b0, BigS b1,
    PackedS p, int* pi, float* pf, double* pd, void* pv, char c, short sh, long lg)
{
    uint64_t h = 0xfedcba9876543210ULL;
    h = add_d(h, strict_sum(d0, d1, d2, d3, d4, d5, d6, d7));
    h = add_f(h, strict_sum(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9));
    h = mix64(h ^ (uint64_t)(i0+i1+i2+i3+i4+i5+i6+i7+i8+i9));
    if (s0) { h = mix64(h ^ (uint64_t)s0->a); h = add_f(h, s0->b); }
    if (s1) { h = mix64(h ^ (uint64_t)s1->a); h = add_f(h, s1->b); }
    if (m0) { h = mix64(h ^ (uint64_t)m0->x); h = add_d(h, m0->d1); }
    if (m1) { h = mix64(h ^ (uint64_t)m1->y); h = add_d(h, m1->d2); }
    h = mix64(h ^ (uint64_t)(b0.arr[0] + b1.arr[7]));
    h = add_f(h, strict_sum(b0.farr[0], b1.farr[3]));
    h = add_d(h, strict_sum(b0.d, b1.d));
    h = mix64(h ^ p.u32 ^ (uint64_t)p.i64);
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    h = mix64(h ^ (uint64_t)c ^ (uint64_t)sh ^ (uint64_t)lg);
    return h;
}

__attribute__((noinline))
uint64_t f10(
    int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9,
    int i10, int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19,
    int i20, int i21, int i22, int i23, int i24, int i25, int i26, int i27, int i28, int i29,
    float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9)
{
    uint64_t h = 0xabcabcabcabcabcaULL;
    int64_t sum = 0;
    sum += i0; sum += i1; sum += i2; sum += i3; sum += i4;
    sum += i5; sum += i6; sum += i7; sum += i8; sum += i9;
    sum += i10; sum += i11; sum += i12; sum += i13; sum += i14;
    sum += i15; sum += i16; sum += i17; sum += i18; sum += i19;
    sum += i20; sum += i21; sum += i22; sum += i23; sum += i24;
    sum += i25; sum += i26; sum += i27; sum += i28; sum += i29;
    h = mix64(h ^ (uint64_t)sum);
    h = add_f(h, strict_sum(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9));
    int prod = 1;
    prod = (prod * (i0|1)) ^ (i10+1);
    prod = (prod * (i20|1)) ^ (i29+1);
    h = mix64(h ^ (uint64_t)prod);
    return h;
}


__attribute__((noinline))
uint64_t f11(
    SmallS s0, SmallS s1, SmallS s2, SmallS s3, SmallS s4, SmallS s5, SmallS s6, SmallS s7,
    MedS m0, MedS m1, MedS m2, MedS m3,
    BigS b0, BigS b1,
    PackedS p0, PackedS p1, PackedS p2, PackedS p3,
    int i0, int i1, int i2, int i3, float f0, float f1, double d0, double d1,
    int* pi, float* pf, double* pd, void* pv, char c, short sh, long lg, long long ll,
    unsigned u, bool bl, size_t sz, int extra)
{
    uint64_t h = 0x1111111111111111ULL;
    h = mix64(h ^ (uint64_t)(s0.a+s1.a+s2.a+s3.a+s4.a+s5.a+s6.a+s7.a));
    h = add_f(h, strict_sum(s0.b, s1.b, s2.b, s3.b, s4.b, s5.b, s6.b, s7.b));
    h = mix64(h ^ (uint64_t)(m0.x+m1.y+m2.z+m3.x));
    h = add_d(h, strict_sum(m0.d1, m1.d2, m2.d1, m3.d2));
    h = mix64(h ^ (uint64_t)(b0.arr[0]+b1.arr[7]));
    h = add_f(h, strict_sum(b0.farr[0], b1.farr[3])); h = add_d(h, strict_sum(b0.d, b1.d));
    h = mix64(h ^ p0.u32 ^ p1.u32 ^ p2.u32 ^ p3.u32);
    h = mix64(h ^ (uint64_t)(i0+i1+i2+i3)); h = add_f(h, strict_sum(f0, f1)); h = add_d(h, strict_sum(d0, d1));
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    h = mix64(h ^ (uint64_t)c ^ (uint64_t)sh ^ (uint64_t)lg ^ (uint64_t)ll ^ u ^ (bl?1:0) ^ sz ^ (uint64_t)extra);
    return h;
}

__attribute__((noinline))
uint64_t f12(
    int* p00, int* p01, int* p02, int* p03, int* p04, int* p05, int* p06, int* p07, int* p08, int* p09,
    float* f00, float* f01, float* f02, float* f03, float* f04, float* f05, float* f06, float* f07, float* f08, float* f09,
    double* d00, double* d01, double* d02, double* d03, double* d04,
    SmallS* s0, SmallS* s1, MedS* m0, MedS* m1, BigS* b0, BigS* b1,
    PackedS* pk, char* c, short* sh, long* lg, long long* ll, unsigned* u, bool* bl, void* v, size_t sz)
{
    uint64_t h = 0x2222222222222222ULL;
    auto si = [](int* p){ return p?*p:0; };
    auto sf = [](float* p){ return p?*p:0.f; };
    auto sd = [](double* p){ return p?*p:0.0; };
    int is = si(p00)+si(p01)+si(p02)+si(p03)+si(p04)+si(p05)+si(p06)+si(p07)+si(p08)+si(p09);
    h = mix64(h ^ (uint64_t)is);
    float fs = strict_sum(sf(f00), sf(f01), sf(f02), sf(f03), sf(f04), sf(f05), sf(f06), sf(f07), sf(f08), sf(f09));
    h = add_f(h, fs);
    h = add_d(h, strict_sum(sd(d00), sd(d01), sd(d02), sd(d03), sd(d04)));
    if (s0) { h = mix64(h ^ (uint64_t)s0->a); h = add_f(h, s0->b); }
    if (s1) { h = mix64(h ^ (uint64_t)s1->a); h = add_f(h, s1->b); }
    if (m0) { h = mix64(h ^ (uint64_t)m0->x); h = add_d(h, m0->d1); }
    if (m1) { h = mix64(h ^ (uint64_t)m1->y); h = add_d(h, m1->d2); }
    if (b0) { h = mix64(h ^ (uint64_t)b0->arr[2]); h = add_f(h, b0->farr[1]); }
    if (b1) { h = mix64(h ^ (uint64_t)b1->arr[5]); h = add_d(h, b1->d); }
    if (pk) h = mix64(h ^ pk->u32 ^ (uint64_t)pk->i64);
    if (c) h = mix64(h ^ (uint64_t)*c);
    if (sh) h = mix64(h ^ (uint64_t)*sh);
    if (lg) h = mix64(h ^ (uint64_t)*lg);
    if (ll) h = mix64(h ^ (uint64_t)*ll);
    if (u) h = mix64(h ^ *u);
    if (bl) h = mix64(h ^ (*bl ? 1 : 0));
    if (v) h = mix64(h ^ (uintptr_t)v);
    h = mix64(h ^ sz);
    return h;
}

__attribute__((noinline))
uint64_t f13(
    int i0, float f0, double d0, char c0, short s0, long l0, long long ll0, unsigned u0, bool b0,
    int i1, float f1, double d1, char c1, short s1, long l1, long long ll1, unsigned u1, bool b1,
    int i2, float f2, double d2, char c2, short s2, long l2, long long ll2, unsigned u2, bool b2,
    int i3, float f3, double d3, char c3, short s3, long l3, long long ll3, unsigned u3, bool b3,
    SmallS ss, MedS mm, BigS* bb, PackedS pp)
{
    uint64_t h = 0x3333333333333333ULL;
    h = mix64(h ^ (uint64_t)(i0+i1+i2+i3));
    h = add_f(h, strict_sum(f0, f1, f2, f3));
    h = add_d(h, strict_sum(d0, d1, d2, d3));
    h = mix64(h ^ (uint64_t)c0 ^ c1 ^ c2 ^ c3);
    h = mix64(h ^ (uint64_t)s0 ^ s1 ^ s2 ^ s3);
    h = mix64(h ^ (uint64_t)l0 ^ l1 ^ l2 ^ l3);
    h = mix64(h ^ (uint64_t)ll0 ^ ll1 ^ ll2 ^ ll3);
    h = mix64(h ^ u0 ^ u1 ^ u2 ^ u3);
    h = mix64(h ^ (b0?1:0) ^ (b1?1:0) ^ (b2?1:0) ^ (b3?1:0));
    h = mix64(h ^ (uint64_t)ss.a); h = add_f(h, ss.b);
    h = mix64(h ^ (uint64_t)(mm.x+mm.y+mm.z)); h = add_d(h, strict_sum(mm.d1, mm.d2));
    if (bb) {
        h = mix64(h ^ (uint64_t)bb->arr[0] ^ bb->arr[7]);
        h = add_f(h, strict_sum(bb->farr[0], bb->farr[3]));
        h = add_d(h, bb->d);
    }
    h = mix64(h ^ pp.u8 ^ pp.u16 ^ pp.u32 ^ (uint64_t)pp.i64);
    return h;
}

__attribute__((noinline))
uint64_t f14(
    BigS b0, BigS b1, BigS b2, BigS b3,
    MedS m0, MedS m1, MedS m2, MedS m3, MedS m4, MedS m5,
    SmallS s0, SmallS s1, SmallS s2, SmallS s3, SmallS s4, SmallS s5, SmallS s6, SmallS s7,
    PackedS p0, PackedS p1,
    int i0, int i1, float f0, float f1, double d0, double d1,
    int* pi, float* pf, double* pd, void* pv, char c, short sh, long lg, long long ll,
    unsigned u, bool bl, size_t sz, int pad0, int pad1, int pad2)
{
    uint64_t h = 0x4444444444444444ULL;
    h = mix64(h ^ (uint64_t)(b0.arr[0]+b1.arr[1]+b2.arr[2]+b3.arr[3]));
    h = add_f(h, strict_sum(b0.farr[0], b1.farr[1], b2.farr[2], b3.farr[3]));
    h = add_d(h, strict_sum(b0.d, b1.d, b2.d, b3.d));
    h = mix64(h ^ (uint64_t)(m0.x+m1.y+m2.z+m3.x+m4.y+m5.z));
    h = add_d(h, strict_sum(m0.d1, m1.d2, m2.d1, m3.d2, m4.d1, m5.d2));
    h = mix64(h ^ (uint64_t)(s0.a+s1.a+s2.a+s3.a+s4.a+s5.a+s6.a+s7.a));
    h = add_f(h, strict_sum(s0.b, s1.b, s2.b, s3.b, s4.b, s5.b, s6.b, s7.b));
    h = mix64(h ^ p0.u32 ^ p1.u32);
    h = mix64(h ^ (uint64_t)(i0+i1)); h = add_f(h, strict_sum(f0, f1)); h = add_d(h, strict_sum(d0, d1));
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    h = mix64(h ^ (uint64_t)c ^ sh ^ lg ^ ll ^ u ^ (bl?1:0) ^ sz ^ pad0 ^ pad1 ^ pad2);
    return h;
}

__attribute__((noinline))
uint64_t f15(
    int i00, int i01, int i02, int i03, int i04, int i05, int i06, int i07, int i08, int i09,
    int i10, int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19,
    float f00, float f01, float f02, float f03, float f04, float f05, float f06, float f07, float f08, float f09,
    double d00, double d01, double d02, double d03, double d04, double d05, double d06, double d07, double d08, double d09)
{
    uint64_t h = 0x5555555555555555ULL;
    int64_t isum = 0;
    isum += i00+i01+i02+i03+i04+i05+i06+i07+i08+i09;
    isum += i10+i11+i12+i13+i14+i15+i16+i17+i18+i19;
    h = mix64(h ^ (uint64_t)isum);
    float fsum = strict_sum(f00, f01, f02, f03, f04, f05, f06, f07, f08, f09);
    h = add_f(h, fsum);
    double dsum = strict_sum(d00, d01, d02, d03, d04, d05, d06, d07, d08, d09);
    h = add_d(h, dsum);
    int xorv = i00 ^ i05 ^ i10 ^ i15;
    h = mix64(h ^ (uint64_t)xorv);
    h = add_f(h, strict_sum(f00 * f05, f09));
    h = add_d(h, strict_sum(d00 * d05, d09));
    return h;
}

__attribute__((noinline))
uint64_t f16(
    PackedS p0, PackedS p1, PackedS p2, PackedS p3, PackedS p4, PackedS p5, PackedS p6, PackedS p7,
    BigS b0, BigS b1,
    MedS m0, MedS m1, MedS m2, MedS m3,
    SmallS s0, SmallS s1, SmallS s2, SmallS s3,
    int i0, int i1, int i2, int i3, float f0, float f1, float f2, float f3,
    double d0, double d1, int* pi, float* pf, double* pd, void* pv,
    char c, short sh, long lg, long long ll, unsigned u, bool bl, size_t sz, int pad)
{
    uint64_t h = 0x6666666666666666ULL;
    h = mix64(h ^ p0.u32 ^ p1.u32 ^ p2.u32 ^ p3.u32 ^ p4.u32 ^ p5.u32 ^ p6.u32 ^ p7.u32);
    h = mix64(h ^ (uint64_t)(p0.i64 + p7.i64));
    h = mix64(h ^ (uint64_t)(b0.arr[0] + b1.arr[7])); h = add_f(h, strict_sum(b0.farr[0], b1.farr[3])); h = add_d(h, strict_sum(b0.d, b1.d));
    h = mix64(h ^ (uint64_t)(m0.x + m1.y + m2.z + m3.x)); h = add_d(h, strict_sum(m0.d1, m3.d2));
    h = mix64(h ^ (uint64_t)(s0.a + s1.a + s2.a + s3.a)); h = add_f(h, strict_sum(s0.b, s3.b));
    h = mix64(h ^ (uint64_t)(i0+i1+i2+i3)); h = add_f(h, strict_sum(f0, f1, f2, f3)); h = add_d(h, strict_sum(d0, d1));
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    h = mix64(h ^ (uint64_t)c ^ sh ^ lg ^ ll ^ u ^ (bl?1:0) ^ sz ^ pad);
    return h;
}

__attribute__((noinline))
uint64_t f17(
    int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9,
    int i10, int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19,
    float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9,
    double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7, double d8, double d9)
{
    uint64_t h = 0x7777777777777777ULL;
    int64_t s = 0;
    s += i0+i1+i2+i3+i4+i5+i6+i7+i8+i9+i10+i11+i12+i13+i14+i15+i16+i17+i18+i19;
    h = mix64(h ^ (uint64_t)s);
    h = add_f(h, strict_sum(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9));
    h = add_d(h, strict_sum(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9));
    h = mix64(h ^ (uint64_t)(i0*i19 + i5*i14));
    h = add_f(h, f0*f9);
    h = add_d(h, d0*d9);
    return h;
}

__attribute__((noinline))
uint64_t f18(
    SmallS* s0, SmallS* s1, SmallS* s2, SmallS* s3, SmallS* s4, SmallS* s5, SmallS* s6, SmallS* s7,
    MedS* m0, MedS* m1, MedS* m2, MedS* m3,
    BigS* b0, BigS* b1, BigS* b2,
    PackedS* p0, PackedS* p1, PackedS* p2,
    int* i0, int* i1, float* f0, float* f1, double* d0, double* d1,
    char* c, short* sh, long* lg, long long* ll, unsigned* u, bool* bl, void* v0, void* v1,
    size_t sz0, size_t sz1, int pad0, int pad1, int pad2, int pad3)
{
    uint64_t h = 0x8888888888888888ULL;
    auto sa = [](SmallS* p){ return p ? p->a : 0; };
    auto sb = [](SmallS* p){ return p ? p->b : 0.f; };
    h = mix64(h ^ (uint64_t)(sa(s0)+sa(s1)+sa(s2)+sa(s3)+sa(s4)+sa(s5)+sa(s6)+sa(s7)));
    h = add_f(h, strict_sum(sb(s0), sb(s1), sb(s2), sb(s3), sb(s4), sb(s5), sb(s6), sb(s7)));
    if (m0) { h = mix64(h ^ (uint64_t)m0->x); h = add_d(h, m0->d1); }
    if (m1) { h = mix64(h ^ (uint64_t)m1->y); h = add_d(h, m1->d2); }
    if (m2) { h = mix64(h ^ (uint64_t)m2->z); }
    if (m3) { h = mix64(h ^ (uint64_t)m3->x); }
    if (b0) { h = mix64(h ^ (uint64_t)b0->arr[0]); h = add_f(h, b0->farr[0]); }
    if (b1) { h = mix64(h ^ (uint64_t)b1->arr[3]); h = add_d(h, b1->d); }
    if (b2) { h = mix64(h ^ (uint64_t)b2->arr[7]); }
    if (p0) h = mix64(h ^ p0->u32);
    if (p1) h = mix64(h ^ p1->u16);
    if (p2) h = mix64(h ^ (uint64_t)p2->i64);
    if (i0) h = mix64(h ^ (uint64_t)*i0);
    if (i1) h = mix64(h ^ (uint64_t)*i1);
    if (f0) h = add_f(h, *f0);
    if (f1) h = add_f(h, *f1);
    if (d0) h = add_d(h, *d0);
    if (d1) h = add_d(h, *d1);
    if (c) h = mix64(h ^ (uint64_t)*c);
    if (sh) h = mix64(h ^ (uint64_t)*sh);
    if (lg) h = mix64(h ^ (uint64_t)*lg);
    if (ll) h = mix64(h ^ (uint64_t)*ll);
    if (u) h = mix64(h ^ *u);
    if (bl) h = mix64(h ^ (*bl ? 1 : 0));
    if (v0) h = mix64(h ^ (uintptr_t)v0);
    if (v1) h = mix64(h ^ (uintptr_t)v1);
    h = mix64(h ^ sz0 ^ sz1 ^ pad0 ^ pad1 ^ pad2 ^ pad3);
    return h;
}

__attribute__((noinline))
uint64_t f19(
    int i0, float f0, double d0, int i1, float f1, double d1, int i2, float f2, double d2,
    int i3, float f3, double d3, int i4, float f4, double d4, int i5, float f5, double d5,
    int i6, float f6, double d6, int i7, float f7, double d7, int i8, float f8, double d8,
    int i9, float f9, double d9, int i10, float f10, double d10, int i11, float f11, double d11,
    SmallS s, MedS m, BigS* b, PackedS p)
{
    uint64_t h = 0x9999999999999999ULL;
    h = mix64(h ^ (uint64_t)(i0+i1+i2+i3+i4+i5+i6+i7+i8+i9+i10+i11));
    h = add_f(h, strict_sum(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11));
    h = add_d(h, strict_sum(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11));
    h = mix64(h ^ (uint64_t)s.a); h = add_f(h, s.b);
    h = mix64(h ^ (uint64_t)(m.x+m.y+m.z)); h = add_d(h, strict_sum(m.d1, m.d2));
    if (b) {
        h = mix64(h ^ (uint64_t)b->arr[4]); h = add_f(h, b->farr[2]); h = add_d(h, b->d);
    }
    h = mix64(h ^ p.u8 ^ p.u16 ^ p.u32 ^ (uint64_t)p.i64);
    return h;
}

__attribute__((noinline))
uint64_t f20(
    int i00, int i01, int i02, int i03, int i04, int i05, int i06, int i07, int i08, int i09,
    int i10, int i11, int i12, int i13, int i14, int i15, int i16, int i17, int i18, int i19,
    int i20, int i21, int i22, int i23, int i24, int i25, int i26, int i27, int i28, int i29,
    int i30, int i31, int i32, int i33, int i34, int i35, int i36, int i37, int i38, int i39)
{
    uint64_t h = 0xaaaaaaaaaaaaaaaaULL;
    int64_t sum = 0;
    sum += i00+i01+i02+i03+i04+i05+i06+i07+i08+i09;
    sum += i10+i11+i12+i13+i14+i15+i16+i17+i18+i19;
    sum += i20+i21+i22+i23+i24+i25+i26+i27+i28+i29;
    sum += i30+i31+i32+i33+i34+i35+i36+i37+i38+i39;
    h = mix64(h ^ (uint64_t)sum);
    int x = i00;
    x ^= i10; x += i20; x ^= i30;
    x ^= i39; x += i19; x ^= i29;
    h = mix64(h ^ (uint64_t)x);
    return h;
}

__attribute__((noinline))
uint64_t f21(
    float f00, float f01, float f02, float f03, float f04, float f05, float f06, float f07, float f08, float f09,
    float f10, float f11, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19,
    double d00, double d01, double d02, double d03, double d04, double d05, double d06, double d07, double d08, double d09,
    double d10, double d11, double d12, double d13, double d14, double d15, double d16, double d17, double d18, double d19)
{
    uint64_t h = 0xbbbbbbbbbbbbbbbbULL;
    float fs = 0.f;
    fs += f00+f01+f02+f03+f04+f05+f06+f07+f08+f09;
    fs += f10+f11+f12+f13+f14+f15+f16+f17+f18+f19;
    h = add_f(h, fs);
    double ds = 0.0;
    ds += d00+d01+d02+d03+d04+d05+d06+d07+d08+d09;
    ds += d10+d11+d12+d13+d14+d15+d16+d17+d18+d19;
    h = add_d(h, ds);
    h = add_f(h, f00 * f19);
    h = add_d(h, d00 * d19);
    return h;
}

__attribute__((noinline))
uint64_t f22(
    BigS b0, BigS b1, BigS b2, BigS b3, BigS b4,
    MedS m0, MedS m1, MedS m2, MedS m3, MedS m4, MedS m5, MedS m6, MedS m7,
    SmallS s0, SmallS s1, SmallS s2, SmallS s3, SmallS s4, SmallS s5, SmallS s6, SmallS s7, SmallS s8, SmallS s9,
    PackedS p0, PackedS p1, PackedS p2, PackedS p3,
    int i0, int i1, float f0, float f1, double d0, double d1, int* pi, float* pf)
{
    uint64_t h = 0xccccccccccccccccULL;
    h = mix64(h ^ (uint64_t)(b0.arr[0]+b1.arr[1]+b2.arr[2]+b3.arr[3]+b4.arr[4]));
    h = add_f(h, strict_sum(b0.farr[0], b4.farr[3]));
    h = add_d(h, strict_sum(b0.d, b4.d));
    h = mix64(h ^ (uint64_t)(m0.x+m7.z));
    h = add_d(h, strict_sum(m0.d1, m7.d2));
    h = mix64(h ^ (uint64_t)(s0.a+s9.a));
    h = add_f(h, strict_sum(s0.b, s9.b));
    h = mix64(h ^ p0.u32 ^ p3.u32);
    h = mix64(h ^ (uint64_t)(i0+i1)); h = add_f(h, strict_sum(f0, f1)); h = add_d(h, strict_sum(d0, d1));
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    return h;
}

__attribute__((noinline))
uint64_t f23(
    int* p0, int* p1, int* p2, int* p3, int* p4, int* p5, int* p6, int* p7, int* p8, int* p9,
    int* p10, int* p11, int* p12, int* p13, int* p14, int* p15, int* p16, int* p17, int* p18, int* p19,
    float* f0, float* f1, float* f2, float* f3, float* f4, float* f5, float* f6, float* f7, float* f8, float* f9,
    double* d0, double* d1, double* d2, double* d3, double* d4, double* d5, double* d6, double* d7, double* d8, double* d9)
{
    uint64_t h = 0xddddddddddddddddULL;
    auto si = [](int* p){ return p?*p:0; };
    auto sf = [](float* p){ return p?*p:0.f; };
    auto sd = [](double* p){ return p?*p:0.0; };
    int64_t is = 0;
    is += si(p0)+si(p1)+si(p2)+si(p3)+si(p4)+si(p5)+si(p6)+si(p7)+si(p8)+si(p9);
    is += si(p10)+si(p11)+si(p12)+si(p13)+si(p14)+si(p15)+si(p16)+si(p17)+si(p18)+si(p19);
    h = mix64(h ^ (uint64_t)is);
    float fs = strict_sum(sf(f0), sf(f1), sf(f2), sf(f3), sf(f4), sf(f5), sf(f6), sf(f7), sf(f8), sf(f9));
    h = add_f(h, fs);
    double ds = strict_sum(sd(d0), sd(d1), sd(d2), sd(d3), sd(d4), sd(d5), sd(d6), sd(d7), sd(d8), sd(d9));
    h = add_d(h, ds);
    return h;
}

__attribute__((noinline))
uint64_t f24(
    SmallS s0, SmallS s1, SmallS s2, SmallS s3, SmallS s4, SmallS s5, SmallS s6, SmallS s7, SmallS s8, SmallS s9,
    MedS m0, MedS m1, MedS m2, MedS m3, MedS m4, MedS m5, MedS m6, MedS m7, MedS m8, MedS m9,
    BigS b0, BigS b1, BigS b2, BigS b3,
    PackedS p0, PackedS p1, PackedS p2, PackedS p3, PackedS p4, PackedS p5,
    int i0, int i1, float f0, float f1, double d0, double d1, int* pi, float* pf, double* pd, void* pv)
{
    uint64_t h = 0xeeeeeeeeeeeeeeeeULL;
    int as = s0.a+s1.a+s2.a+s3.a+s4.a+s5.a+s6.a+s7.a+s8.a+s9.a;
    h = mix64(h ^ (uint64_t)as);
    float bs = strict_sum(s0.b, s1.b, s2.b, s3.b, s4.b, s5.b, s6.b, s7.b, s8.b, s9.b);
    h = add_f(h, bs);
    h = mix64(h ^ (uint64_t)(m0.x+m9.z));
    h = add_d(h, strict_sum(m0.d1, m9.d2));
    h = mix64(h ^ (uint64_t)(b0.arr[0]+b3.arr[7]));
    h = add_f(h, strict_sum(b0.farr[0], b3.farr[3]));
    h = add_d(h, strict_sum(b0.d, b3.d));
    h = mix64(h ^ p0.u32 ^ p5.u32);
    h = mix64(h ^ (uint64_t)(i0+i1)); h = add_f(h, strict_sum(f0, f1)); h = add_d(h, strict_sum(d0, d1));
    if (pi) h = mix64(h ^ (uint64_t)*pi);
    if (pf) h = add_f(h, *pf);
    if (pd) h = add_d(h, *pd);
    if (pv) h = mix64(h ^ (uintptr_t)pv);
    return h;
}

__attribute__((noinline))
uint64_t f25(
    int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9,
    float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9,
    double d0, double d1, double d2, double d3, double d4, double d5, double d6, double d7, double d8, double d9,
    char c0, char c1, char c2, char c3, short s0, short s1, short s2, short s3,
    long l0, long l1, long long ll0, long long ll1, unsigned u0, unsigned u1, bool b0, bool b1)
{
    uint64_t h = 0xffffffffffffffffULL;
    h = mix64(h ^ (uint64_t)(i0+i1+i2+i3+i4+i5+i6+i7+i8+i9));
    h = add_f(h, strict_sum(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9));
    h = add_d(h, strict_sum(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9));
    h = mix64(h ^ (uint64_t)c0 ^ c1 ^ c2 ^ c3);
    h = mix64(h ^ (uint64_t)s0 ^ s1 ^ s2 ^ s3);
    h = mix64(h ^ (uint64_t)l0 ^ l1);
    h = mix64(h ^ (uint64_t)ll0 ^ ll1);
    h = mix64(h ^ u0 ^ u1);
    h = mix64(h ^ (b0?1:0) ^ (b1?1:0));
    return h;
}

int main() {

    auto start = std::chrono::high_resolution_clock::now();

    int ints[40];
    float floats[40];
    double doubles[40];
    for (int i = 0; i < 40; ++i) {
        ints[i] = (i * 17 + 3) ^ (i << 3);
        floats[i] = (float)(i * 0.1f + 1.5f);
        doubles[i] = (double)(i * 0.01 + 2.5);
    }

    SmallS ss[10];
    for (int i = 0; i < 10; ++i) {
        ss[i].a = ints[i];
        ss[i].b = floats[i];
    }

    MedS ms[10];
    for (int i = 0; i < 10; ++i) {
        ms[i].x = ints[i];
        ms[i].y = ints[i+1];
        ms[i].z = ints[i+2];
        ms[i].d1 = doubles[i];
        ms[i].d2 = doubles[i+1];
        ms[i].c = (char)('A' + (i % 26));
    }

    BigS bs[5];
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 8; ++j) bs[i].arr[j] = ints[j + i];
        for (int j = 0; j < 4; ++j) bs[i].farr[j] = floats[j + i];
        bs[i].d = doubles[i];
        bs[i].p = (void*)(uintptr_t)(0x1000 + i * 16);
        bs[i].nested = ms[i];
    }

    PackedS ps[10];
    for (int i = 0; i < 10; ++i) {
        ps[i].u8 = (uint8_t)(i * 3);
        ps[i].u16 = (uint16_t)(i * 7 + 1);
        ps[i].u32 = (uint32_t)(i * 13 + 5);
        ps[i].i64 = (int64_t)(i * 19 - 2);
    }

    char chars[8] = {'x', 'y', 'z', 'w', 'a', 'b', 'c', 'd'};
    short shorts[4] = {100, 200, 300, 400};
    long longs[4] = {1000L, 2000L, 3000L, 4000L};
    long long llongs[4] = {10000LL, 20000LL, 30000LL, 40000LL};
    unsigned uints[4] = {1u, 2u, 3u, 4u};
    bool bools[4] = {true, false, true, false};
    size_t sizes[4] = {64, 128, 256, 512};
    void* voids[4] = {(void*)0xdead, (void*)0xbeef, (void*)0xcafe, (void*)0xbabe};

    uint64_t checksum = 0x5a5a5a5a5a5a5a5aULL;

    checksum ^= f01(
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7],
        ints[8], ints[9], floats[0], floats[1], floats[2], floats[3], floats[4],
        floats[5], floats[6], floats[7], doubles[0], doubles[1], doubles[2], doubles[3],
        &ints[0], &ints[1], &floats[0], &doubles[0], ss[0], ss[1],
        ms[0], ms[1], &bs[0], ps[0], chars[0], shorts[0], longs[0],
        llongs[0], uints[0], bools[0], voids[0], sizes[0]);

    checksum ^= f02(
        doubles[0], doubles[1], doubles[2], doubles[3], doubles[4], doubles[5],
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], ints[8], ints[9],
        floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7],
        &ss[0], &ss[1], ms[0], bs[0], &ps[0], &ints[0], &floats[0],
        &doubles[0], &chars[0], &shorts[0], &longs[0], &llongs[0], &uints[0],
        &bools[0], &voids[0], &sizes[0]);

    checksum ^= f03(
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7],
        ints[8], ints[9], ints[10], ints[11], ints[12], ints[13], ints[14], ints[15],
        floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7],
        floats[8], floats[9], doubles[0], doubles[1], doubles[2], doubles[3],
        ss[0], ms[0], &bs[0], ps[0], &ints[0], &floats[0], &doubles[0],
        voids[0], chars[0], bools[0]);

    checksum ^= f04(
        ps[0], ps[1], ps[2], ps[3],
        bs[0], bs[1],
        ms[0], ms[1], ms[2], ms[3],
        ss[0], ss[1], ss[2], ss[3],
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5],
        floats[0], floats[1], floats[2], floats[3],
        doubles[0], doubles[1], doubles[2], doubles[3],
        &ints[0], &floats[0], &doubles[0], voids[0],
        chars[0], chars[1], shorts[0], longs[0], llongs[0], uints[0],
        bools[0], sizes[0]);

    checksum ^= f05(
        &ints[0], &ints[1], &ints[2], &ints[3], &ints[4], &ints[5], &ints[6], &ints[7],
        &floats[0], &floats[1], &floats[2], &floats[3], &floats[4], &floats[5], &floats[6], &floats[7],
        &doubles[0], &doubles[1], &doubles[2], &doubles[3],
        &ss[0], &ss[1], &ms[0], &ms[1], &bs[0], &bs[1],
        &ps[0], &ps[1], &chars[0], &chars[1], &shorts[0], &shorts[1],
        &longs[0], &llongs[0], &uints[0], &bools[0], voids[0], voids[1],
        sizes[0], sizes[1]);

    checksum ^= f06(
        ints[0], floats[0], doubles[0], ints[1], floats[1], doubles[1],
        ints[2], floats[2], doubles[2], ints[3], floats[3], doubles[3],
        ints[4], floats[4], doubles[4], ints[5], floats[5], doubles[5],
        ints[6], floats[6], doubles[6], ints[7], floats[7], doubles[7],
        ints[8], floats[8], doubles[8], ints[9], floats[9], doubles[9],
        ss[0], ss[1], ms[0], ms[1], bs[0], ps[0],
        &ints[0], &floats[0], &doubles[0], voids[0]);

    checksum ^= f07(
        bs[0], bs[1], bs[2],
        ms[0], ms[1], ms[2], ms[3],
        ss[0], ss[1], ss[2], ss[3], ss[4], ss[5],
        ps[0], ps[1], ps[2],
        ints[0], ints[1], ints[2], ints[3], floats[0], floats[1], floats[2], floats[3],
        doubles[0], doubles[1], &ints[0], &floats[0], &doubles[0], voids[0],
        chars[0], shorts[0], longs[0], llongs[0], uints[0], bools[0], sizes[0], 42);

    checksum ^= f08(
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], ints[8], ints[9],
        ints[10], ints[11], ints[12], ints[13], ints[14], ints[15], ints[16], ints[17], ints[18], ints[19],
        floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7], floats[8], floats[9],
        doubles[0], doubles[1], doubles[2], doubles[3], doubles[4],
        ss[0], ms[0], &bs[0], ps[0], &ints[0], &floats[0], &doubles[0], voids[0], chars[0]);

    checksum ^= f09(
        doubles[0], doubles[1], doubles[2], doubles[3], doubles[4], doubles[5], doubles[6], doubles[7],
        floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7], floats[8], floats[9],
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], ints[8], ints[9],
        &ss[0], &ss[1], &ms[0], &ms[1], bs[0], bs[1],
        ps[0], &ints[0], &floats[0], &doubles[0], voids[0], chars[0], shorts[0], longs[0]);

    checksum ^= f10(
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], ints[8], ints[9],
        ints[10], ints[11], ints[12], ints[13], ints[14], ints[15], ints[16], ints[17], ints[18], ints[19],
        ints[20], ints[21], ints[22], ints[23], ints[24], ints[25], ints[26], ints[27], ints[28], ints[29],
        floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7], floats[8], floats[9]);

    checksum ^= f11(
        ss[0], ss[1], ss[2], ss[3], ss[4], ss[5], ss[6], ss[7],
        ms[0], ms[1], ms[2], ms[3],
        bs[0], bs[1],
        ps[0], ps[1], ps[2], ps[3],
        ints[0], ints[1], ints[2], ints[3], floats[0], floats[1], doubles[0], doubles[1],
        &ints[0], &floats[0], &doubles[0], voids[0], chars[0], shorts[0], longs[0], llongs[0],
        uints[0], bools[0], sizes[0], 99);

    checksum ^= f12(
        &ints[0], &ints[1], &ints[2], &ints[3], &ints[4], &ints[5], &ints[6], &ints[7], &ints[8], &ints[9],
        &floats[0], &floats[1], &floats[2], &floats[3], &floats[4], &floats[5], &floats[6], &floats[7], &floats[8], &floats[9],
        &doubles[0], &doubles[1], &doubles[2], &doubles[3], &doubles[4],
        &ss[0], &ss[1], &ms[0], &ms[1], &bs[0], &bs[1],
        &ps[0], &chars[0], &shorts[0], &longs[0], &llongs[0], &uints[0], &bools[0], voids[0], sizes[0]);

    checksum ^= f13(
        ints[0], floats[0], doubles[0], chars[0], shorts[0], longs[0], llongs[0], uints[0], bools[0],
        ints[1], floats[1], doubles[1], chars[1], shorts[1], longs[1], llongs[1], uints[1], bools[1],
        ints[2], floats[2], doubles[2], chars[2], shorts[2], longs[2], llongs[2], uints[2], bools[2],
        ints[3], floats[3], doubles[3], chars[3], shorts[3], longs[3], llongs[3], uints[3], bools[3],
        ss[0], ms[0], &bs[0], ps[0]);

    checksum ^= f14(
        bs[0], bs[1], bs[2], bs[3],
        ms[0], ms[1], ms[2], ms[3], ms[4], ms[5],
        ss[0], ss[1], ss[2], ss[3], ss[4], ss[5], ss[6], ss[7],
        ps[0], ps[1],
        ints[0], ints[1], floats[0], floats[1], doubles[0], doubles[1],
        &ints[0], &floats[0], &doubles[0], voids[0], chars[0], shorts[0], longs[0], llongs[0],
        uints[0], bools[0], sizes[0], 1, 2, 3);

    checksum ^= f15(
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], ints[8], ints[9],
        ints[10], ints[11], ints[12], ints[13], ints[14], ints[15], ints[16], ints[17], ints[18], ints[19],
        floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7], floats[8], floats[9],
        doubles[0], doubles[1], doubles[2], doubles[3], doubles[4], doubles[5], doubles[6], doubles[7], doubles[8], doubles[9]);

    checksum ^= f16(
        ps[0], ps[1], ps[2], ps[3], ps[4], ps[5], ps[6], ps[7],
        bs[0], bs[1],
        ms[0], ms[1], ms[2], ms[3],
        ss[0], ss[1], ss[2], ss[3],
        ints[0], ints[1], ints[2], ints[3], floats[0], floats[1], floats[2], floats[3],
        doubles[0], doubles[1], &ints[0], &floats[0], &doubles[0], voids[0],
        chars[0], shorts[0], longs[0], llongs[0], uints[0], bools[0], sizes[0], 7);

    checksum ^= f17(
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], ints[8], ints[9],
        ints[10], ints[11], ints[12], ints[13], ints[14], ints[15], ints[16], ints[17], ints[18], ints[19],
        floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7], floats[8], floats[9],
        doubles[0], doubles[1], doubles[2], doubles[3], doubles[4], doubles[5], doubles[6], doubles[7], doubles[8], doubles[9]);

    checksum ^= f18(
        &ss[0], &ss[1], &ss[2], &ss[3], &ss[4], &ss[5], &ss[6], &ss[7],
        &ms[0], &ms[1], &ms[2], &ms[3],
        &bs[0], &bs[1], &bs[2],
        &ps[0], &ps[1], &ps[2],
        &ints[0], &ints[1], &floats[0], &floats[1], &doubles[0], &doubles[1],
        &chars[0], &shorts[0], &longs[0], &llongs[0], &uints[0], &bools[0], voids[0], voids[1],
        sizes[0], sizes[1], 10, 20, 30, 40);

    checksum ^= f19(
        ints[0], floats[0], doubles[0], ints[1], floats[1], doubles[1], ints[2], floats[2], doubles[2],
        ints[3], floats[3], doubles[3], ints[4], floats[4], doubles[4], ints[5], floats[5], doubles[5],
        ints[6], floats[6], doubles[6], ints[7], floats[7], doubles[7], ints[8], floats[8], doubles[8],
        ints[9], floats[9], doubles[9], ints[10], floats[10], doubles[10], ints[11], floats[11], doubles[11],
        ss[0], ms[0], &bs[0], ps[0]);

    checksum ^= f20(
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], ints[8], ints[9],
        ints[10], ints[11], ints[12], ints[13], ints[14], ints[15], ints[16], ints[17], ints[18], ints[19],
        ints[20], ints[21], ints[22], ints[23], ints[24], ints[25], ints[26], ints[27], ints[28], ints[29],
        ints[30], ints[31], ints[32], ints[33], ints[34], ints[35], ints[36], ints[37], ints[38], ints[39]);

    checksum ^= f21(
        floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7], floats[8], floats[9],
        floats[10], floats[11], floats[12], floats[13], floats[14], floats[15], floats[16], floats[17], floats[18], floats[19],
        doubles[0], doubles[1], doubles[2], doubles[3], doubles[4], doubles[5], doubles[6], doubles[7], doubles[8], doubles[9],
        doubles[10], doubles[11], doubles[12], doubles[13], doubles[14], doubles[15], doubles[16], doubles[17], doubles[18], doubles[19]);

    checksum ^= f22(
        bs[0], bs[1], bs[2], bs[3], bs[4],
        ms[0], ms[1], ms[2], ms[3], ms[4], ms[5], ms[6], ms[7],
        ss[0], ss[1], ss[2], ss[3], ss[4], ss[5], ss[6], ss[7], ss[8], ss[9],
        ps[0], ps[1], ps[2], ps[3],
        ints[0], ints[1], floats[0], floats[1], doubles[0], doubles[1], &ints[0], &floats[0]);

    checksum ^= f23(
        &ints[0], &ints[1], &ints[2], &ints[3], &ints[4], &ints[5], &ints[6], &ints[7], &ints[8], &ints[9],
        &ints[10], &ints[11], &ints[12], &ints[13], &ints[14], &ints[15], &ints[16], &ints[17], &ints[18], &ints[19],
        &floats[0], &floats[1], &floats[2], &floats[3], &floats[4], &floats[5], &floats[6], &floats[7], &floats[8], &floats[9],
        &doubles[0], &doubles[1], &doubles[2], &doubles[3], &doubles[4], &doubles[5], &doubles[6], &doubles[7], &doubles[8], &doubles[9]);

    checksum ^= f24(
        ss[0], ss[1], ss[2], ss[3], ss[4], ss[5], ss[6], ss[7], ss[8], ss[9],
        ms[0], ms[1], ms[2], ms[3], ms[4], ms[5], ms[6], ms[7], ms[8], ms[9],
        bs[0], bs[1], bs[2], bs[3],
        ps[0], ps[1], ps[2], ps[3], ps[4], ps[5],
        ints[0], ints[1], floats[0], floats[1], doubles[0], doubles[1], &ints[0], &floats[0], &doubles[0], voids[0]);

    checksum ^= f25(
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], ints[8], ints[9],
        floats[0], floats[1], floats[2], floats[3], floats[4], floats[5], floats[6], floats[7], floats[8], floats[9],
        doubles[0], doubles[1], doubles[2], doubles[3], doubles[4], doubles[5], doubles[6], doubles[7], doubles[8], doubles[9],
        chars[0], chars[1], chars[2], chars[3], shorts[0], shorts[1], shorts[2], shorts[3],
        longs[0], longs[1], llongs[0], llongs[1], uints[0], uints[1], bools[0], bools[1]);

    checksum = mix64(checksum);

    std::printf("CHECKSUM: 0x%016llx\n", (unsigned long long)checksum);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << diff << std::endl;

    return 0;
}
