// =============================================================================
// Mind.h — "Bộ não" của MỘT sinh vật (Individual Agent AI).
//
// Mỗi sinh vật (người, sói, gấu, quỷ...) đều có một Mind riêng biệt, chứa:
//   * Tính cách               (Personality)
//   * Trí nhớ ngắn & dài hạn  (ShortTermMemory / LongTermMemory)
//   * Tri thức (công nghệ đã biết) + mức tiếp xúc với từng loại kích thích
//   * Kỹ năng + kinh nghiệm hành động (học tăng cường đơn giản)
//   * Trí tuệ hiện tại (intellect) — tăng theo trải nghiệm, văn hoá, tuổi
//   * Mục tiêu hiện tại + kế hoạch GOAP + "dòng suy nghĩ" để debug
//   * Bộ sinh số ngẫu nhiên riêng (cho phép suy nghĩ song song an toàn)
//
// Vòng đời một nhịp suy nghĩ (xem Brain.cpp):
//   QUAN SÁT → SUY NGHĨ (đánh giá nhu cầu, Utility) → QUYẾT ĐỊNH (GOAP)
//   → HÀNH ĐỘNG (mỗi tick) → HỌC (kỹ năng, phát minh, ghi nhớ) → TIẾN HOÁ (gen + văn hoá)
// =============================================================================
#pragma once
#include "Personality.h"
#include "Memory.h"
#include "Knowledge.h"
#include "Goap.h"
#include "Invention.h"
#include "../phys/Physics.h"
#include "../world/Terrain.h"
#include <cstdio>
#include <cstdarg>

namespace wb {

// Giai đoạn nhận thức (hiển thị debug).
enum class Sapience : uint8_t { Instinct, Learning, Toolmaker, Sapient, Enlightened };
inline const char* sapienceName(Sapience s) {
    static const char* n[] = {"Bản năng", "Biết học hỏi", "Biết chế tác", "Có trí khôn", "Khai sáng"};
    return n[static_cast<int>(s)];
}
inline Sapience sapienceOf(float intellect) {
    if (intellect < 0.25f) return Sapience::Instinct;
    if (intellect < 0.38f) return Sapience::Learning;
    if (intellect < 0.50f) return Sapience::Toolmaker;
    if (intellect < 0.65f) return Sapience::Sapient;
    return Sapience::Enlightened;
}

struct ActionStat {
    uint16_t success = 0, fail = 0;
    // Tỷ lệ thất bại đã học (làm mượt kiểu Laplace) → nhân vào chi phí GOAP.
    float failRate() const { return (fail + 0.5f) / (success + fail + 1.0f); }
};

// Một ô trên bản đồ mà agent vừa nhìn thấy (x<0 = không có).
struct Spot {
    int x = -1, y = -1;
    bool valid() const { return x >= 0; }
    void reset() { x = y = -1; }
};

struct Mind {
    Personality pers;
    ShortTermMemory stm;
    LongTermMemory ltm;
    TechSet known;
    std::array<uint16_t, kStimulusCount> exposure{};
    std::array<float, kSkillCount> skill{};
    std::array<ActionStat, kActionCount> stats{};

    float intellect = 0.1f;     // trí tuệ hiệu dụng 0..1
    float learned = 0.0f;       // phần trí tuệ có được nhờ trải nghiệm (0..0.25)
    float experience = 0.0f;    // tổng điểm kinh nghiệm

    // Ra quyết định
    GoalId goal = GoalId::Idle;
    float goalScore = 0;
    std::array<float, kGoalCount> scores{}; // điểm Utility lần gần nhất (debug)
    WorldState state = 0;                   // trạng thái thế giới theo góc nhìn bản thân
    Plan plan;
    int actionTicks = 0;
    Entity target = NullEntity;
    int tx = -1, ty = -1;                   // ô mục tiêu của hành động
    Building buildType = Building::None;

    // Nhịp suy nghĩ (LOD): chỉ "suy nghĩ lại" mỗi thinkInterval tick.
    Rng rng;
    long nextThink = 0;
    int thinkInterval = 6;
    bool needReplan = true;

    // Kết quả quan sát tài nguyên gần nhất (pha think ghi, pha act dùng)
    Spot seenFood, seenTree, seenStone, seenOre, seenCorpse, seenTool, seenFire, blockedWater, seenRuin;
    // Kích thích phát hiện trong pha think (song song) → xử lý phát minh ở pha act (tuần tự)
    std::array<uint8_t, kStimulusCount> pendingStim{};

    // ---- Tri thức vật chất mở (khái niệm, vật liệu, thiết kế, quy trình) ----
    Knowhow know;
    Purpose craftPurpose = Purpose::Cut;      // đang muốn chế tạo/thiết kế cho mục đích nào
    // Ý tưởng sáng tạo đang theo đuổi (nghĩ ra trước, rồi mới đi kiếm vật liệu làm thử)
    std::vector<Part> idea;
    Purpose ideaPurpose = Purpose::Cut;
    int ideaParent = -1;
    bool hasIdea = false;
    std::array<long, kPurposeCount> craftBlockedUntil{};   // tạm gác chế tạo khi không kiếm nổi vật liệu
    int processChoice = -1;                   // quy trình định chạy ở lò
    // Sự kiện vật lý/hoá học quan sát được ở pha think → học ở pha act
    std::array<ReactionEvent, 3> seenReactions{};
    int seenReactionCount = 0;
    std::array<std::pair<uint8_t, MatId>, 3> seenPhys{};
    int seenPhysCount = 0;
    long lastShot = 0;

    long lastSocialTick = 0, lastMarkTick = 0, lastWorshipTick = 0, lastExploreTick = 0;
    int lastDiscovery = -1;
    long lastDiscoveryTick = -100000;
    char thought[96] = "";                  // câu "suy nghĩ" hiển thị cho người chơi
    bool possessed = false;                 // người chơi đang điều khiển (góc nhìn thứ nhất) → AI tạm nghỉ

    Sapience sapience() const { return sapienceOf(intellect); }
    void think(const char* fmt, ...) WB_PRINTF(2, 3);
    float skillOf(Skill s) const { return skill[static_cast<int>(s)]; }
};

inline void Mind::think(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(thought, sizeof(thought), fmt, args);
    va_end(args);
}

} // namespace wb
