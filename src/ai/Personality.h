// =============================================================================
// Personality.h — Tính cách cá nhân (mô hình Big Five rút gọn + tham vọng).
//
// Tính cách được suy ra một phần từ gen và một phần từ ngẫu nhiên lúc sinh, sau
// đó thay đổi nhẹ theo trải nghiệm (bị thương nhiều → nhút nhát hơn, thắng trận
// → tự tin/hung hăng hơn). Tính cách là TRỌNG SỐ trong Utility AI: cùng một tình
// huống, cá thể tò mò sẽ chọn "khám phá", cá thể tham vọng chọn "giành quyền lực".
// =============================================================================
#pragma once
#include "Genome.h"

namespace wb {

struct Personality {
    float openness = 0.5f;      // cởi mở / sáng tạo
    float diligence = 0.5f;     // chăm chỉ (thích lao động, tích trữ)
    float extraversion = 0.5f;  // thích giao tiếp
    float agreeableness = 0.5f; // hoà đồng (ngược với hung hăng)
    float fearfulness = 0.5f;   // nhút nhát
    float ambition = 0.5f;      // tham vọng quyền lực / thống trị

    static Personality fromGenome(const Genome& gn, Rng& rng) {
        Personality p;
        auto noisy = [&](float base) { return clamp01(base + rng.gauss(0.15f)); };
        p.openness = noisy(gn[Gene::Curiosity]);
        p.diligence = noisy(0.5f);
        p.extraversion = noisy(gn[Gene::Sociability]);
        p.agreeableness = noisy(1.0f - gn[Gene::Aggression]);
        p.fearfulness = noisy(1.0f - gn[Gene::Strength] * 0.6f - gn[Gene::Aggression] * 0.4f);
        p.ambition = noisy(gn[Gene::Aggression] * 0.5f + gn[Gene::Intelligence] * 0.3f + 0.1f);
        return p;
    }

    // Mô tả ngắn để hiển thị debug.
    const char* archetype() const {
        if (ambition > 0.7f && agreeableness < 0.4f) return "Kẻ chinh phục";
        if (openness > 0.7f) return "Nhà phát minh";
        if (extraversion > 0.7f && agreeableness > 0.6f) return "Người kết nối";
        if (diligence > 0.7f) return "Người cần mẫn";
        if (fearfulness > 0.7f) return "Kẻ nhút nhát";
        if (agreeableness < 0.3f) return "Kẻ hung bạo";
        return "Bình thường";
    }
};

} // namespace wb
