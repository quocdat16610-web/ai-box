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
#include "../core/Math.h"
#include <string>

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

    // ---- Người chơi điều khiển sinh vật (góc nhìn thứ nhất) ----
    enum class PlayerAct { Attack, Eat, Gather, BuildHut };
    // Thực hiện một hành động theo hướng nhìn `facing` (vector đơn vị trên mặt phẳng bản đồ).
    // Trả về mô tả kết quả (để hiện lên màn hình). Dùng chung luật của AI: sát thương, ăn, khai thác...
    static std::string playerAction(Simulation& sim, Entity e, PlayerAct act, Vec2 facing);
    // Di chuyển sinh vật bị điều khiển một bước theo hướng `dir` (đã nhân tốc độ), tôn trọng môi trường sống.
    static bool playerMove(Simulation& sim, Entity e, Vec2 dir);
    // Kiệt sức giữa nước sâu (không có bè) → mất máu dần, có thể chết đuối. Trả về true nếu đang đuối nước.
    static bool playerDrowning(Simulation& sim, Entity e, float dt);
};

} // namespace wb
