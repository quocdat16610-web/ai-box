// =============================================================================
// Genome.h — Bộ gen của mỗi cá thể (tiến hoá sinh học).
//
// Mỗi gen là một số thực 0..1. Con cái nhận gen = trung bình có trọng số ngẫu
// nhiên của bố mẹ (lai ghép) + đột biến Gauss. Chọn lọc tự nhiên diễn ra "tự
// nhiên": cá thể khoẻ/thông minh sống lâu hơn, sinh nhiều con hơn → phân bố gen
// của quần thể dịch chuyển theo thời gian. Khi trung bình gen của một nhóm bị
// cô lập lệch đủ xa so với loài gốc → hình thành LOÀI MỚI (xem SocietySystem).
// =============================================================================
#pragma once
#include "../core/Math.h"
#include <array>

namespace wb {

enum class Gene : uint8_t {
    Intelligence, // tiềm năng trí tuệ
    Strength,     // sức mạnh thể chất
    Speed,        // tốc độ
    Sociability,  // tính bầy đàn / hướng ngoại
    Aggression,   // tính hung hăng
    Curiosity,    // tò mò → khám phá, phát minh
    Fertility,    // khả năng sinh sản
    Longevity,    // tuổi thọ
    ColdResist,   // chịu lạnh
    HeatResist,   // chịu nóng
    Dexterity,    // độ khéo của chi trước → cầm nắm, chế tạo (tiến hoá được!)
    Size,         // kích thước cơ thể → sức khoẻ, sức mạnh, lượng ăn
    Swimming,     // khả năng bơi → vượt nước nông
    Count
};
constexpr int kGeneCount = static_cast<int>(Gene::Count);

const char* geneName(Gene g);

struct Genome {
    std::array<float, kGeneCount> g{};
    int generation = 0;

    float operator[](Gene k) const { return g[static_cast<int>(k)]; }
    float& operator[](Gene k) { return g[static_cast<int>(k)]; }

    // Lai ghép hai bộ gen + đột biến. mutationRate điều khiển tốc độ tiến hoá.
    static Genome crossover(const Genome& a, const Genome& b, Rng& rng, float mutationRate) {
        Genome c;
        for (int i = 0; i < kGeneCount; ++i) {
            float t = rng.uniform();
            float v = a.g[i] * t + b.g[i] * (1 - t);
            v += rng.gauss(0.03f * mutationRate);
            if (rng.chance(0.02f * mutationRate)) v += rng.gauss(0.12f); // đột biến lớn hiếm gặp
            c.g[i] = clamp01(v);
        }
        c.generation = std::max(a.generation, b.generation) + 1;
        return c;
    }

    // Khoảng cách di truyền (Euclid) — dùng để phát hiện phân hoá loài.
    static float distance(const Genome& a, const Genome& b) {
        float s = 0;
        for (int i = 0; i < kGeneCount; ++i) { float d = a.g[i] - b.g[i]; s += d * d; }
        return std::sqrt(s);
    }
};

} // namespace wb
