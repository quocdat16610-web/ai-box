// Scenarios.cpp — Thiết lập các kịch bản ví dụ.
#include "Scenarios.h"
#include "Simulation.h"

namespace wb {

const char* scenarioName(Scenario s) {
    static const char* n[] = {"Thế giới tự do", "Đàn sói tiến hoá", "Người nguyên thuỷ"};
    return n[static_cast<int>(s)];
}

Scenario scenarioFromString(const std::string& s) {
    if (s == "wolves" || s == "wolf") return Scenario::WolfPack;
    if (s == "humans" || s == "primal") return Scenario::PrimalHumans;
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
    default: break;
    }
    sim.chronicle.add(sim.tick, EventKind::Misc, std::string("Thế giới được khai sinh — kịch bản: ") + scenarioName(s));
}

} // namespace wb
