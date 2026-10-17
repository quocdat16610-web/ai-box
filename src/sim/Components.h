// =============================================================================
// Components.h — Các component ECS của sinh vật.
//
//   Transform  : vị trí / hướng di chuyển
//   Body       : cơ thể sinh học (máu, tuổi, đói, mệt, mang thai)
//   Inventory  : đồ mang theo (thức ăn, gỗ, đá, quặng, cấp công cụ/vũ khí)
//   Social     : quan hệ xã hội (nhóm, vai trò, bố mẹ, tên)
//   GenomeC    : bộ gen
//   Mind       : BỘ NÃO — AI cá nhân (xem ai/Mind.h)
// =============================================================================
#pragma once
#include "../core/Math.h"
#include "../core/Ecs.h"
#include "../ai/Genome.h"
#include <string>

namespace wb {

struct Transform {
    Vec2 pos;
    Vec2 target;       // điểm đang đi tới
    bool moving = false;
    int stuck = 0;     // số tick bị kẹt liên tiếp
};

struct Body {
    int species = 0;
    float hp = 50, maxHp = 50;
    float damage = 5;
    float speed = 0.1f;
    long ageTicks = 0;
    float hunger = 0.2f;   // 0 = no, 1 = chết đói
    float fatigue = 0.0f;  // 0 = tỉnh táo, 1 = kiệt sức
    float cold = 0.0f;     // mức lạnh (không chịu được khí hậu)
    bool female = false;
    int pregnancyTicks = 0;      // >0: đang mang thai
    Entity pregnantBy = NullEntity;
    int reproCooldown = 0;
    int kills = 0;
    Entity lastAttacker = NullEntity;
    bool sleeping = false;
    bool dead = false;
    int deathCause = 0;          // 0 già, 1 đói, 2 bị giết, 3 thiên tai, 4 lạnh
};

struct Inventory {
    int food = 0, wood = 0, stone = 0, ore = 0;
    uint8_t toolTier = 0;    // 0 tay không, 1 công cụ đá, 2 kim loại
    uint8_t weaponTier = 0;  // 0 không, 1 giáo đá, 2 kiếm kim loại
    int total() const { return food + wood + stone + ore; }
};

enum class Role : uint8_t { None, Leader, Elder, Hunter, Gatherer, Builder, Farmer, Soldier, Crafter, Priest, Child, Count };
const char* roleName(Role r);

struct Social {
    int group = -1;            // id nhóm (bầy / bộ tộc / quốc gia)
    Role role = Role::None;
    Entity mother = NullEntity, father = NullEntity;
    Entity mate = NullEntity;
    std::string name;
    float status = 0;          // địa vị / uy quyền trong nhóm
    int children = 0;
    long joinedTick = 0;
};

struct GenomeC { Genome genome; };

} // namespace wb
