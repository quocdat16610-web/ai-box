// =============================================================================
// Noise.h — Value noise + fBm (fractal Brownian motion) để sinh địa hình.
// Đơn giản, không cần thư viện ngoài, cho kết quả lục địa/đảo tự nhiên.
// =============================================================================
#pragma once
#include "../core/Math.h"

namespace wb {

class ValueNoise {
public:
    explicit ValueNoise(uint32_t seed) : seed_(seed) {}

    // Nhiễu giá trị trong [0,1] tại toạ độ thực (x, y).
    float sample(float x, float y) const {
        int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
        float tx = x - xi, ty = y - yi;
        // nội suy smoothstep giữa 4 góc
        tx = tx * tx * (3 - 2 * tx);
        ty = ty * ty * (3 - 2 * ty);
        float a = corner(xi, yi), b = corner(xi + 1, yi);
        float c = corner(xi, yi + 1), d = corner(xi + 1, yi + 1);
        return lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
    }

    // Cộng nhiều tầng (octave) nhiễu với tần số tăng dần, biên độ giảm dần.
    float fbm(float x, float y, int octaves, float persistence = 0.5f) const {
        float sum = 0, amp = 1, norm = 0, freq = 1;
        for (int i = 0; i < octaves; ++i) {
            sum += sample(x * freq + i * 17.3f, y * freq - i * 9.1f) * amp;
            norm += amp;
            amp *= persistence;
            freq *= 2.0f;
        }
        return sum / norm;
    }

    // Nhiễu 3D (8 góc) — lấy mẫu trên mặt cầu để bản đồ quấn vòng không có đường nối
    float sample3(float x, float y, float z) const {
        int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y)), zi = static_cast<int>(std::floor(z));
        float tx = x - xi, ty = y - yi, tz = z - zi;
        tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty); tz = tz * tz * (3 - 2 * tz);
        auto c = [&](int a, int b, int d) { return corner3(xi + a, yi + b, zi + d); };
        float x00 = lerp(c(0, 0, 0), c(1, 0, 0), tx), x10 = lerp(c(0, 1, 0), c(1, 1, 0), tx);
        float x01 = lerp(c(0, 0, 1), c(1, 0, 1), tx), x11 = lerp(c(0, 1, 1), c(1, 1, 1), tx);
        return lerp(lerp(x00, x10, ty), lerp(x01, x11, ty), tz);
    }
    float fbm3(float x, float y, float z, int octaves, float persistence = 0.5f) const {
        float sum = 0, amp = 1, norm = 0, freq = 1;
        for (int i = 0; i < octaves; ++i) {
            sum += sample3(x * freq + i * 17.3f, y * freq - i * 9.1f, z * freq + i * 5.7f) * amp;
            norm += amp;
            amp *= persistence;
            freq *= 2.0f;
        }
        return sum / norm;
    }

private:
    float corner3(int x, int y, int z) const {
        uint32_t h = hash32(static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u ^
                            static_cast<uint32_t>(z) * 83492791u ^ seed_);
        return (h & 0xFFFFFF) / 16777215.0f;
    }
    float corner(int x, int y) const {
        uint32_t h = hash32(static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u ^ seed_);
        return (h & 0xFFFFFF) / 16777215.0f;
    }
    uint32_t seed_;
};

} // namespace wb
