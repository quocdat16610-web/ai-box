// =============================================================================
// Goap.h — Goal-Oriented Action Planning (GOAP).
//
// Trạng thái thế giới (theo góc nhìn của MỘT cá thể) được mã hoá thành 32 bit
// "sự thật" (WorldState). Mỗi hành động có:
//      tiền điều kiện (pre)  : các bit phải đúng
//      hiệu ứng (effect)     : các bit sẽ trở thành đúng sau khi làm xong
//      chi phí (cost)        : cơ sở × hệ số cá nhân (kỹ năng, kinh nghiệm, vai trò)
//      yêu cầu               : tech cần biết, cần tay khéo, chế độ ăn...
// Bộ lập kế hoạch A* tìm chuỗi hành động rẻ nhất biến trạng thái hiện tại thành
// trạng thái mục tiêu (do Utility AI chọn). Ví dụ một con người đói, không thấy
// thức ăn nhưng nhớ có cây ăn quả:
//      [GoToFood] → [Forage] → [EatCarried]
// Một con sói đói:  [SearchPrey] → [Hunt] → [EatCarried]
// =============================================================================
#pragma once
#include "Knowledge.h"
#include <array>
#include <cstdint>

namespace wb {

using WorldState = uint64_t;
namespace ws {
enum : uint64_t {
    HasFood = 1ull << 0, Fed = 1ull << 1, Rested = 1ull << 2, Safe = 1ull << 3,
    NearFood = 1ull << 4, KnowsFood = 1ull << 5, SeesPrey = 1ull << 6, SeesCarrion = 1ull << 7,
    HasWood = 1ull << 8, HasStone = 1ull << 9, HasOre = 1ull << 10, HasTool = 1ull << 11,
    HasWeapon = 1ull << 12, KnowsTree = 1ull << 13, KnowsStone = 1ull << 14, KnowsOre = 1ull << 15,
    HasMaterials = 1ull << 16, Built = 1ull << 17, SeesEnemy = 1ull << 18, EnemyDefeated = 1ull << 19,
    MateNearby = 1ull << 20, Reproduced = 1ull << 21, AllyNearby = 1ull << 22, Socialized = 1ull << 23,
    Explored = 1ull << 24, Stored = 1ull << 25, Farmed = 1ull << 26, Marked = 1ull << 27,
    Worshipped = 1ull << 28, AtHome = 1ull << 29, GroupFood = 1ull << 30, Dominant = 1ull << 31,
    Invented = 1ull << 32, Experimented = 1ull << 33, Produced = 1ull << 34, Studied = 1ull << 35,
};
}

enum class ActionId : uint8_t {
    EatCarried, EatHere, GoToFood, SearchFood, Forage, SearchPrey, Hunt, Scavenge,
    EatFromStore, GoHome, SleepAtHome, SleepHere, Flee, Fight, SeekEnemy,
    SeekMate, Mate, SeekAlly, Socialize, Explore,
    GatherWood, GatherStone, GatherOre, Craft, Invent,
    GatherMaterials, Build, DepositWood, DepositStone, DepositFood,
    Farm, MarkTerritory, Worship, ChallengeLeader,
    Experiment, Produce, DepositOre,
    Study,     // nghiên cứu khoa học có chủ đích (quan sát, giải phẫu, thăm dò, thả vật thử nghiệm)
    Count
};
constexpr int kActionCount = static_cast<int>(ActionId::Count);

// Kỹ năng cá nhân (tăng dần khi luyện tập hành động tương ứng).
enum class Skill : uint8_t { Foraging, Hunting, Crafting, Building, Combat, Social, Farming, Leadership, Count };
constexpr int kSkillCount = static_cast<int>(Skill::Count);
const char* skillName(Skill s);

enum class DietReq : uint8_t { Any, Plants, Meat };

struct ActionDef {
    const char* name;      // tên tiếng Việt (hiển thị trong debug "suy nghĩ")
    WorldState pre;
    WorldState effect;
    float cost;
    int tech;              // -1 = bản năng
    bool needsHands;       // cần tay khéo
    bool needsGroup;       // cần thuộc một nhóm
    DietReq diet;
    int skill;             // kỹ năng được rèn luyện khi làm (-1 = không)
};

const ActionDef& actionDef(ActionId a);

// Mục tiêu (do Utility AI chọn) → trạng thái mong muốn cho GOAP.
enum class GoalId : uint8_t {
    Idle, Eat, Rest, Flee, Defend, Reproduce, Socialize, Explore, Work,
    Craft, Invent, Build, Farm, MarkTerritory, Worship, War, Dominate,
    Experiment, Produce, Study,
    Count
};
constexpr int kGoalCount = static_cast<int>(GoalId::Count);
const char* goalName(GoalId g);
WorldState goalState(GoalId g);

struct Plan {
    static constexpr int kMax = 8;
    std::array<ActionId, kMax> steps{};
    uint8_t len = 0, idx = 0;
    bool done() const { return idx >= len; }
    ActionId current() const { return steps[idx]; }
    void clear() { len = idx = 0; }
};

// Bộ lọc + hàm chi phí phụ thuộc cá thể (truyền từ Brain vào Planner).
struct PlannerAgentView {
    bool available[kActionCount];
    float costMul[kActionCount];
};

// A*: trả về true nếu tìm được kế hoạch.
bool planActions(WorldState start, WorldState goal, const PlannerAgentView& view, Plan& out);

} // namespace wb
