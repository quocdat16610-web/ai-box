// =============================================================================
// Simulation.h — Trái tim của game: sở hữu thế giới, ECS, các loài, các nhóm xã
// hội và chạy vòng lặp mô phỏng theo tick.
//
// Thứ tự một tick (xem Simulation::step):
//   1. World::update          — môi trường (cây mọc, lửa, mưa, mùa)
//   2. rebuildGrid            — cập nhật lưới không gian
//   3. Brain::think (SONG SONG) — chỉ các agent đến lượt "suy nghĩ" (LOD + batch)
//   4. Brain::act   (tuần tự)   — mọi agent thực hiện hành động hiện tại
//   5. metabolism             — đói, mệt, lạnh, già, mang thai, sinh nở
//   6. SocietySystem::update  — hình thành nhóm, thăng cấp văn minh, ngoại giao,
//                               chiến tranh, tách nhóm, hình thành loài mới
//   7. cleanupDead            — xoá entity đã chết, để lại xác (vật phẩm)
// =============================================================================
#pragma once
#include "../core/Ecs.h"
#include "../core/SpatialGrid.h"
#include "../core/ThreadPool.h"
#include "../world/World.h"
#include "../ai/Mind.h"
#include "../civ/Group.h"
#include "Species.h"
#include "Components.h"
#include "Chronicle.h"
#include <memory>
#include <deque>

namespace wb {

struct SimConfig {
    int width = 192, height = 192;
    uint32_t seed = 12345;
    float mutationRate = 1.0f;   // hệ số tốc độ đột biến gen (tiến hoá nhanh/chậm)
    float learningRate = 1.0f;   // hệ số tốc độ học / phát minh
    int maxPopulation = 5000;    // trần dân số để bảo vệ hiệu năng (nhất là trên điện thoại)
    unsigned threads = 0;        // 0 = tự động
};

struct SimStats {
    int population = 0, groups = 0, nations = 0, species = 0;
    int births = 0, deaths = 0;
    float avgIntellect = 0;
    int thinksLastTick = 0;
    int sapientDeaths[5] = {0, 0, 0, 0, 0}; // tử vong của loài có tay khéo theo nguyên nhân (già/đói/bị giết/thiên tai/lạnh)
};

class SocietySystem;

class Simulation {
public:
    explicit Simulation(const SimConfig& cfg);
    ~Simulation();
    void reset(const SimConfig& cfg);
    void step();

    // ---- Sinh / tử ----
    Entity spawnCreature(int species, Vec2 pos, const Genome* genome = nullptr,
                         Entity mother = NullEntity, Entity father = NullEntity);
    void kill(Entity e, int cause);
    // Tìm ô đi được gần (x,y) để thả sinh vật.
    bool findSpawnTile(int& x, int& y, int radius = 6);

    // ---- Nhóm xã hội ----
    int createGroup(Entity founder);
    void joinGroup(Entity e, int gid);
    void leaveGroup(Entity e);
    Group* group(int gid) { return (gid >= 0 && gid < static_cast<int>(groups.size()) && groups[gid].alive) ? &groups[gid] : nullptr; }
    int aliveGroupCount() const;
    Group* groupOf(Entity e);

    // ---- LOD: vùng camera đang nhìn (agent ngoài màn hình suy nghĩ thưa hơn) ----
    void setFocusRect(float x0, float y0, float x1, float y1) { focus_[0] = x0; focus_[1] = y0; focus_[2] = x1; focus_[3] = y1; }
    bool inFocus(Vec2 p) const { return p.x >= focus_[0] && p.y >= focus_[1] && p.x <= focus_[2] && p.y <= focus_[3]; }

    std::string dateString() const;
    float yearsOf(long ticks) const { return ticks / static_cast<float>(kTicksPerYear); }
    const SimStats& stats() const { return stats_; }

    // ---- Dữ liệu công khai (các hệ thống & renderer cùng đọc) ----
    SimConfig cfg;
    World world;
    Registry reg;
    SpeciesRegistry species;
    std::deque<Group> groups;   // deque: tham chiếu ổn định khi thêm nhóm mới
    SpatialGrid grid;
    Chronicle chronicle;
    Rng rng;
    long tick = 0;
    std::vector<Entity> agents; // danh sách entity có Mind (để duyệt song song)
    TechSet worldFirsts;
    std::vector<std::pair<int, int>> killEvents; // (nhóm kẻ giết, nhóm nạn nhân) — SocietySystem xử lý ngoại giao        // tech đã được phát minh lần đầu ở đâu đó trên thế giới

private:
    void rebuildGrid();
    void metabolism();
    void cleanupDead();
    std::unique_ptr<ThreadPool> pool_;
    std::unique_ptr<SocietySystem> society_;
    std::vector<std::pair<Entity, Vec2>> gridItems_;
    std::vector<Entity> dying_;
    std::vector<Entity> thinkList_;
    std::vector<Language> speciesLang_; // ngôn ngữ "mặc định" để đặt tên cá thể chưa thuộc nhóm nào
    float focus_[4] = {0, 0, 1e9f, 1e9f};
    SimStats stats_;
};

} // namespace wb
