// =============================================================================
// Brain.h — Vòng lặp AI cá nhân: QUAN SÁT → SUY NGHĨ → QUYẾT ĐỊNH → HÀNH ĐỘNG → HỌC.
//
//   think() — chạy SONG SONG trên thread pool. Chỉ đọc thế giới, chỉ ghi vào Mind
//             của chính agent: quan sát, cập nhật trí nhớ, tính Utility cho mọi
//             mục tiêu, chọn mục tiêu, lập kế hoạch GOAP.
//   act()   — chạy TUẦN TỰ mỗi tick: thực thi bước hiện tại của kế hoạch, thay
//             đổi thế giới (ăn, săn, xây, chiến đấu, giao phối...), và học từ
//             kết quả (kỹ năng, tỷ lệ thành công, phát minh, dạy học).
// =============================================================================
#pragma once
#include "../core/Ecs.h"
#include "Knowledge.h"

namespace wb {

class Simulation;
struct Mind;
struct Genome;

class Brain {
public:
    static void initMind(Simulation& sim, Entity e, Mind& m, const Genome& g, Entity mother, Entity father);
    static void think(Simulation& sim, Entity e);
    static void act(Simulation& sim, Entity e);

    // Gây sát thương a → v. Trả về true nếu v chết.
    static bool attack(Simulation& sim, Entity a, Entity v, float mul = 1.0f);
    // Tăng "tiếp xúc" với một kích thích và thử phát minh (chỉ gọi ở pha tuần tự).
    static void stimulate(Simulation& sim, Entity e, Stimulus s, int amount = 1);
    // Học một tech (do tự phát minh, được dạy, hoặc thừa hưởng).
    static void learnTech(Simulation& sim, Entity e, Tech t, bool invented);
    // Cập nhật trí tuệ hiệu dụng từ gen + trải nghiệm + văn hoá nhóm.
    static void updateIntellect(Simulation& sim, Entity e);
};

} // namespace wb
