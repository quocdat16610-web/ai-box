// =============================================================================
// Math.h — Các tiện ích toán học cơ bản dùng chung toàn bộ engine.
//   * Vec2  : vector 2D số thực (vị trí, vận tốc của sinh vật)
//   * Rng   : bộ sinh số ngẫu nhiên nhanh (xoshiro128**), mỗi sinh vật có một
//             bộ Rng riêng để có thể chạy AI song song mà không tranh chấp dữ liệu.
// Không phụ thuộc raylib → lõi mô phỏng có thể build "headless" (test, server).
// =============================================================================
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

// Kiểm tra chuỗi định dạng printf lúc biên dịch (GCC/Clang); MSVC bỏ qua.
#if defined(__GNUC__) || defined(__clang__)
#define WB_PRINTF(fmtIdx, argIdx) __attribute__((format(printf, fmtIdx, argIdx)))
#else
#define WB_PRINTF(fmtIdx, argIdx)
#endif

namespace wb {

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    float len2() const { return x * x + y * y; }
    float len() const { return std::sqrt(len2()); }
    Vec2 normalized() const { float l = len(); return l > 1e-6f ? Vec2{x / l, y / l} : Vec2{0, 0}; }
};

inline float dist2(Vec2 a, Vec2 b) { return (a - b).len2(); }
inline float dist(Vec2 a, Vec2 b) { return (a - b).len(); }
inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Bộ sinh số ngẫu nhiên xoshiro128** — nhanh, chất lượng tốt, trạng thái nhỏ (16 byte).
struct Rng {
    uint32_t s[4];
    Rng() { reseed(0x9E3779B97F4A7C15ull); }
    explicit Rng(uint64_t seed) { reseed(seed); }
    void reseed(uint64_t seed) {
        // SplitMix64 để khởi tạo trạng thái từ một seed duy nhất
        for (int i = 0; i < 4; ++i) {
            seed += 0x9E3779B97F4A7C15ull;
            uint64_t z = seed;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            s[i] = static_cast<uint32_t>(z ^ (z >> 31));
        }
    }
    static uint32_t rotl(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }
    uint32_t next() {
        uint32_t result = rotl(s[1] * 5, 7) * 9;
        uint32_t t = s[1] << 9;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t; s[3] = rotl(s[3], 11);
        return result;
    }
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }            // [0,1)
    float range(float a, float b) { return a + (b - a) * uniform(); }
    int irange(int a, int b) { return a + static_cast<int>(next() % static_cast<uint32_t>(b - a + 1)); } // [a,b]
    bool chance(float p) { return uniform() < p; }
    // Phân phối chuẩn xấp xỉ (tổng 4 biến đều) — đủ dùng cho đột biến gen
    float gauss(float sigma) {
        float sum = uniform() + uniform() + uniform() + uniform() - 2.0f;
        return sum * sigma * 1.732f;
    }
};

// Hàm băm số nguyên — dùng cho noise và tạo màu ổn định theo id.
inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}

} // namespace wb
