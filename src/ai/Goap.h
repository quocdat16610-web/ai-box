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

using WorldState = uint32_t;
namespace ws {
enum : uint32_t {
    HasFood = 1u << 0, Fed = 1u << 1, Rested = 1u << 2, Safe = 1u << 3,
    NearFood = 1u << 4, KnowsFood = 1u << 5, SeesPrey = 1u << 6, SeesCarrion = 1u << 7,
    HasWood = 1u << 8, HasStone = 1u << 9, HasOre = 1u << 10, HasTool = 1u << 11,
    HasWeapon = 1u << 12, KnowsTree = 1u << 13, KnowsStone = 1u << 14, KnowsOre = 1u << 15,
    HasMaterials = 1u << 16, Built = 1u << 17, SeesEnemy = 1u << 18, EnemyDefeated = 1u << 19,
    MateNearby = 1u << 20, Reproduced = 1u << 21, AllyNearby = 1u << 22, Socialized = 1u << 23,
    Explored = 1u << 24, Stored = 1u << 25, Farmed = 1u << 26, Marked = 1u << 27,
    Worshipped = 1u << 28, AtHome = 1u << 29, GroupFood = 1u << 30, Dominant = 1u << 31,
};
}

enum class ActionId : uint8_t {
    EatCarried, EatHere, GoToFood, SearchFood, Forage, SearchPrey, Hunt, Scavenge,
    EatFromStore, GoHome, SleepAtHome, SleepHere, Flee, Fight, SeekEnemy,
    SeekMate, Mate, SeekAlly, Socialize, Explore,
    GatherWood, GatherStone, GatherOre, CraftTool, CraftWeapon,
    GatherMaterials, Build, DepositWood, DepositStone, DepositFood,
    Farm, MarkTerritory, Worship, ChallengeLeader,
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
    CraftTool, CraftWeapon, Build, Farm, MarkTerritory, Worship, War, Dominate,
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
