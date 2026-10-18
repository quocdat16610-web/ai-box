// =============================================================================
// Memory.h — Hệ thống trí nhớ của từng cá thể.
//
//  * ShortTermMemory (trí nhớ ngắn hạn / "blackboard"): những gì vừa nhìn thấy
//    trong lần quan sát gần nhất — kẻ thù, con mồi, đồng loại, vật phẩm... Bị
//    ghi đè mỗi lần quan sát mới.
//  * LongTermMemory (trí nhớ dài hạn / "đồ thị tri thức" đơn giản): các "sự kiện"
//    dạng (loại, vị trí|đối tượng, độ tin cậy, lần cuối xác nhận). Ví dụ:
//        (Nguồn thức ăn, (40,52), 0.8)   (Kẻ thù, entity#123, 0.9)
//    Sức chứa tỷ lệ với trí tuệ; sự kiện không được củng cố sẽ phai dần (quên).
//  * Experience: tỷ lệ thành công của từng hành động → AI học được hành động
//    nào hiệu quả và điều chỉnh chi phí trong bộ lập kế hoạch GOAP.
// =============================================================================
#pragma once
#include "../core/Ecs.h"
#include "../core/Math.h"
#include <array>
#include <vector>

namespace wb {

// ---------------- Trí nhớ ngắn hạn ----------------
enum class PerceptKind : uint8_t { Ally, Kin, Stranger, Predator, Prey, Enemy, Item, Fire };

struct Percept {
    PerceptKind kind;
    Entity entity = NullEntity;
    Vec2 pos;
    float strength = 0; // mức đe doạ hoặc giá trị
};

struct ShortTermMemory {
    static constexpr int kCap = 20;
    std::array<Percept, kCap> items;
    int count = 0;
    int allies = 0, enemies = 0, predators = 0, prey = 0;
    float threat = 0;           // tổng mức đe doạ cảm nhận
    Entity nearestPrey = NullEntity, nearestThreat = NullEntity, nearestMate = NullEntity;
    Entity nearestStranger = NullEntity, nearestAlly = NullEntity, nearestEnemy = NullEntity;
    Vec2 threatPos;

    void clear() {
        count = allies = enemies = predators = prey = 0; threat = 0;
        nearestPrey = nearestThreat = nearestMate = nearestStranger = nearestAlly = nearestEnemy = NullEntity;
    }
    void add(const Percept& p) { if (count < kCap) items[count++] = p; }
};

// ---------------- Trí nhớ dài hạn ----------------
enum class FactType : uint8_t {
    FoodSource, WaterSource, WoodSource, StoneSource, OreSource, Danger,
    HuntingGround, Home, FireSeen, OldTool, EnemyGroup,
    DeepOre,   // mạch khoáng dưới lòng đất đã thăm dò được (subject = loại khoáng)
    Count
};

struct Fact {
    FactType type;
    int x = 0, y = 0;
    int subject = -1;     // id đối tượng (entity hoặc nhóm) nếu có
    float confidence = 1; // độ tin cậy 0..1
    long lastTick = 0;
    int reinforce = 1;    // số lần được củng cố
};

class LongTermMemory {
public:
    int capacity = 16;

    // Ghi nhớ: nếu đã có sự kiện cùng loại gần đó thì củng cố, ngược lại thêm mới
    // (đẩy sự kiện yếu nhất ra nếu đầy).
    void remember(FactType t, int x, int y, long tick, int subject = -1, float conf = 1.0f) {
        for (auto& f : facts_) {
            if (f.type == t && (subject >= 0 ? f.subject == subject : (std::abs(f.x - x) + std::abs(f.y - y) <= 4))) {
                f.confidence = std::min(1.0f, f.confidence + 0.2f);
                f.lastTick = tick; f.reinforce++;
                if (subject >= 0) { f.x = x; f.y = y; }
                return;
            }
        }
        Fact nf{t, x, y, subject, conf, tick, 1};
        if (static_cast<int>(facts_.size()) < capacity) { facts_.push_back(nf); return; }
        size_t weakest = 0;
        for (size_t i = 1; i < facts_.size(); ++i)
            if (score(facts_[i]) < score(facts_[weakest])) weakest = i;
        if (score(facts_[weakest]) < score(nf)) facts_[weakest] = nf;
    }

    // Phủ nhận: đến nơi mà không còn tài nguyên → giảm độ tin cậy.
    void contradict(FactType t, int x, int y) {
        for (auto& f : facts_)
            if (f.type == t && std::abs(f.x - x) + std::abs(f.y - y) <= 4) f.confidence -= 0.5f;
        prune();
    }

    // Quên dần theo thời gian (đường cong lãng quên). Trí tuệ cao quên chậm hơn.
    void decay(float rate) {
        for (auto& f : facts_) f.confidence -= rate / (1.0f + f.reinforce * 0.3f);
        prune();
    }

    // Lấy sự kiện tốt nhất (gần + tin cậy cao) thuộc loại t.
    const Fact* best(FactType t, Vec2 from) const {
        const Fact* bestF = nullptr; float bestS = -1e9f;
        for (auto& f : facts_) {
            if (f.type != t) continue;
            float s = f.confidence * 50.0f - dist(from, {float(f.x), float(f.y)});
            if (s > bestS) { bestS = s; bestF = &f; }
        }
        return bestF;
    }
    bool knows(FactType t) const {
        for (auto& f : facts_) if (f.type == t) return true;
        return false;
    }
    const std::vector<Fact>& facts() const { return facts_; }
    void clear() { facts_.clear(); }

private:
    static float score(const Fact& f) { return f.confidence + f.reinforce * 0.05f; }
    void prune() {
        facts_.erase(std::remove_if(facts_.begin(), facts_.end(), [](const Fact& f) { return f.confidence <= 0; }), facts_.end());
    }
    std::vector<Fact> facts_;
};

} // namespace wb
