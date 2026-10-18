// Scenarios.cpp — Thiết lập các kịch bản ví dụ.
#include "Scenarios.h"
#include "Simulation.h"

namespace wb {

const char* scenarioName(Scenario s) {
    static const char* n[] = {"Thế giới tự do", "Đàn sói tiến hoá", "Người nguyên thuỷ", "Hai bộ tộc song sinh"};
    return n[static_cast<int>(s)];
}

Scenario scenarioFromString(const std::string& s) {
    if (s == "wolves" || s == "wolf") return Scenario::WolfPack;
    if (s == "humans" || s == "primal") return Scenario::PrimalHumans;
    if (s == "twins" || s == "twin") return Scenario::TwinTribes;
    return Scenario::Sandbox;
}

namespace {
// Tìm vùng đất rộng phù hợp: nhiều ô đi được + có thức ăn/gỗ/đá xung quanh.
bool findHabitat(Simulation& sim, int& ox, int& oy, bool needStone, int avoidX = -1000, int avoidY = -1000, int minDist = 0) {
    World& w = sim.world;
    float best = -1;
    for (int tries = 0; tries < 400; ++tries) {
        int x = sim.rng.irange(10, w.width() - 11), y = sim.rng.irange(10, w.height() - 11);
        if (!w.walkable(x, y)) continue;
        if ((x - avoidX) * (x - avoidX) + (y - avoidY) * (y - avoidY) < minDist * minDist) continue;
        float score = 0;
        bool stone = false, tree = false;
        for (int dy = -7; dy <= 7; dy += 1)
            for (int dx = -7; dx <= 7; dx += 1) {
                const Tile& t = w.at(x + dx, y + dy);
                if (w.walkable(x + dx, y + dy)) score += 1;
                score += t.food / 120.0f;
                stone |= t.stone > 20;
                tree |= t.wood > 20;
                if (t.terrain == Terrain::Snow || t.terrain == Terrain::Desert) score -= 1.5f;
            }
        if (needStone && !(stone && tree)) score *= 0.5f;
        if (score > best) { best = score; ox = x; oy = y; }
    }
    return best > 0;
}

void spawnHerd(Simulation& sim, int sp, int n, int cx, int cy, float spread, float intelBoost = 0) {
    for (int i = 0; i < n; ++i) {
        Genome g = sim.species[sp].baseGenome;
        for (float& v : g.g) v = clamp01(v + sim.rng.gauss(0.05f));
        g[Gene::Intelligence] = clamp01(g[Gene::Intelligence] + intelBoost);
        sim.spawnCreature(sp, {cx + sim.rng.range(-spread, spread), cy + sim.rng.range(-spread, spread)}, &g);
    }
}

void spawnRandomly(Simulation& sim, int sp, int n) {
    for (int i = 0; i < n; ++i) {
        int x = sim.rng.irange(5, sim.world.width() - 6), y = sim.rng.irange(5, sim.world.height() - 6);
        if (sim.findSpawnTile(x, y, 12)) sim.spawnCreature(sp, {x + 0.5f, y + 0.5f});
    }
}
} // namespace

void setupScenario(Simulation& sim, Scenario s) {
    int x = sim.world.width() / 2, y = sim.world.height() / 2;
    switch (s) {
    case Scenario::Sandbox: {
        int px = -1000, py = -1000;
        for (int sp : {sp::Human, sp::Elf, sp::Orc, sp::Dwarf}) {
            findHabitat(sim, x, y, true, px, py, 40);
            spawnHerd(sim, sp, 12, x, y, 3);
            px = x; py = y;
        }
        for (int k = 0; k < 3; ++k) { findHabitat(sim, x, y, false); spawnHerd(sim, sp::Wolf, 6, x, y, 2); }
        spawnRandomly(sim, sp::Deer, 90);
        spawnRandomly(sim, sp::Rabbit, 80);
        spawnRandomly(sim, sp::Bear, 6);
        spawnRandomly(sim, sp::Troll, 2);
        findHabitat(sim, x, y, false); spawnHerd(sim, sp::Goblin, 8, x, y, 2);
        break;
    }
    case Scenario::WolfPack: {
        // Tiến hoá & học nhanh hơn để quan sát được trong vài phút
        sim.cfg.mutationRate = 2.5f;
        sim.cfg.learningRate = 2.0f;
        findHabitat(sim, x, y, false);
        spawnHerd(sim, sp::Wolf, 10, x, y, 2, 0.06f);
        spawnHerd(sim, sp::Deer, 50, x, y, 25);
        spawnHerd(sim, sp::Rabbit, 50, x, y, 25);
        spawnRandomly(sim, sp::Deer, 40);
        // Một bầy sói đối thủ ở xa → tranh chấp lãnh thổ
        int rx = x, ry = y;
        findHabitat(sim, rx, ry, false, x, y, 45);
        spawnHerd(sim, sp::Wolf, 6, rx, ry, 2);
        spawnHerd(sim, sp::Deer, 30, rx, ry, 20);
        break;
    }
    case Scenario::PrimalHumans: {
        sim.cfg.learningRate = 2.0f;
        sim.cfg.mutationRate = 1.5f;
        findHabitat(sim, x, y, true);
        spawnHerd(sim, sp::Human, 14, x, y, 3);
        spawnHerd(sim, sp::Deer, 40, x, y, 25);
        spawnHerd(sim, sp::Rabbit, 40, x, y, 25);
        spawnRandomly(sim, sp::Wolf, 4);
        break;
    }
    case Scenario::TwinTribes: {
        sim.cfg.learningRate = 2.0f;
        sim.cfg.mutationRate = 1.5f;
        World& w = sim.world;
        // (A) Bờ núi lửa: gần hồ dung nham — đá vỏ chai, lưu huỳnh, bazan, hơi nước khi dung nham gặp nước
        int ax = -1, ay = -1, bestA = 1 << 30;
        for (int y = 6; y < w.height() - 6; ++y)
            for (int x = 6; x < w.width() - 6; ++x) {
                if (!(w.at(x, y).flags & kTileVolcanic)) continue;
                for (int tries = 0; tries < 30; ++tries) {
                    int px = x + sim.rng.irange(-9, 9), py = y + sim.rng.irange(-9, 9);
                    if (!w.inBounds(px, py) || !w.walkable(px, py)) continue;
                    int d = (px - x) * (px - x) + (py - y) * (py - y);
                    if (d >= 25 && d < bestA) { bestA = d; ax = px; ay = py; }
                }
            }
        if (ax < 0) findHabitat(sim, ax, ay, true);
        // (B) Rừng lạnh ven sông: ô có nhiệt độ thấp, nhiều cây, cách xa (A)
        int bx = -1, by = -1;
        float bestB = -1e9f;
        for (int tries = 0; tries < 3000; ++tries) {
            int x = sim.rng.irange(8, w.width() - 9), y = sim.rng.irange(8, w.height() - 9);
            if (!w.walkable(x, y) || (x - ax) * (x - ax) + (y - ay) * (y - ay) < 50 * 50) continue;
            float cold = -w.matter.ambient(w, x, y);
            int trees = 0, water = 0;
            for (int dy = -5; dy <= 5; ++dy)
                for (int dx = -5; dx <= 5; ++dx) { trees += w.at(x + dx, y + dy).wood > 20; water += w.isWater(x + dx, y + dy); }
            float score = cold * 2 + trees * 0.6f + std::min(water, 10) * 1.0f;
            if (score > bestB) { bestB = score; bx = x; by = y; }
        }
        if (bx < 0) findHabitat(sim, bx, by, false, ax, ay, 40);
        // Cùng một nhóm sáng lập (bộ gen nhân bản) cho cả hai nơi
        std::vector<Genome> founders;
        for (int i = 0; i < 12; ++i) {
            Genome g = sim.species[sp::Human].baseGenome;
            for (float& v : g.g) v = clamp01(v + sim.rng.gauss(0.05f));
            founders.push_back(g);
        }
        for (auto [cx, cy] : {std::pair<int, int>{ax, ay}, std::pair<int, int>{bx, by}}) {
            for (const Genome& g : founders) sim.spawnCreature(sp::Human, {cx + sim.rng.range(-2.f, 2.f), cy + sim.rng.range(-2.f, 2.f)}, &g);
            spawnHerd(sim, sp::Deer, 30, cx, cy, 18);
            spawnHerd(sim, sp::Rabbit, 30, cx, cy, 18);
        }
        sim.chronicle.add(sim.tick, EventKind::Misc,
                          "Hai nhóm song sinh được thả xuống: (A) bờ núi lửa (" + std::to_string(ax) + "," + std::to_string(ay) +
                              "), (B) rừng lạnh ven sông (" + std::to_string(bx) + "," + std::to_string(by) + ")",
                          ax, ay);
        break;
    }
    default: break;
    }
    sim.chronicle.add(sim.tick, EventKind::Misc, std::string("Thế giới được khai sinh — kịch bản: ") + scenarioName(s));
}

} // namespace wb
