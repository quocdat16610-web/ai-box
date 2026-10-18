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
#include "../ai/Invention.h"
#include <array>
#include <vector>
#include <string>

namespace wb {

struct Transform {
    Vec2 pos;
    Vec2 target;       // điểm đang đi tới
    bool moving = false;
    int stuck = 0;     // số tick bị kẹt liên tiếp
    Vec2 push;         // vận tốc do ngoại lực (nổ, va chạm, bị đánh bật) — tắt dần do ma sát
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
    int food = 0, wood = 0, stone = 0;          // tài nguyên khối (gỗ = mat::Wood, đá = mat::Stone)
    std::vector<std::pair<MatId, int>> mats;    // vật liệu khác: quặng, kim loại, đất sét, than, muối…
    std::array<int16_t, kPurposeCount> item;    // vật đang mang theo cho từng mục đích (id thiết kế, -1 = không)
    std::array<uint8_t, kPurposeCount> wear{};  // độ mòn còn lại (0 = gãy)
    Inventory() { item.fill(-1); }

    int count(MatId m) const {
        if (m == mat::Wood) return wood;
        if (m == mat::Stone) return stone;
        for (auto& [k, v] : mats) if (k == m) return v;
        return 0;
    }
    void add(MatId m, int n) {
        if (n <= 0 || m == kNoMat) return;
        if (m == mat::Wood) { wood += n; return; }
        if (m == mat::Stone) { stone += n; return; }
        for (auto& [k, v] : mats) if (k == m) { v += n; return; }
        mats.push_back({m, n});
    }
    int take(MatId m, int n) {
        if (m == mat::Wood) { int t = std::min(n, wood); wood -= t; return t; }
        if (m == mat::Stone) { int t = std::min(n, stone); stone -= t; return t; }
        for (auto it = mats.begin(); it != mats.end(); ++it)
            if (it->first == m) {
                int t = std::min(n, it->second);
                it->second -= t;
                if (it->second == 0) mats.erase(it);
                return t;
            }
        return 0;
    }
    int matTotal() const { int t = 0; for (auto& [k, v] : mats) t += v; return t; }
    int total() const { return food + wood + stone + matTotal(); }
};

enum class Role : uint8_t { None, Leader, Elder, Hunter, Gatherer, Builder, Farmer, Soldier, Crafter, Priest, Child, Scholar, Miner, Count };
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
