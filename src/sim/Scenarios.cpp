// Scenarios.cpp — Thiết lập các kịch bản ví dụ.
#include "Scenarios.h"
#include "Simulation.h"
#include <vector>

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
// Các điểm đã thả nhóm sinh vật trong lần dựng kịch bản hiện tại
std::vector<std::pair<int, int>> gUsedSpots;

bool findHabitat(Simulation& sim, int& ox, int& oy, bool needStone, int avoidX = -1000, int avoidY = -1000, int minDist = 0) {
    World& w = sim.world;
    float best = -1e9f;
    bool found = false;
    for (int tries = 0; tries < 600; ++tries) {
        int x = sim.rng.irange(0, w.width() - 1), y = sim.rng.irange(10, w.height() - 11);
        if (!w.walkable(x, y)) continue;
        int ddx = static_cast<int>(std::fabs(WrapSpace::dx(static_cast<float>(x - avoidX))));
        if (avoidX > -1000 && ddx * ddx + (y - avoidY) * (y - avoidY) < minDist * minDist) continue;
        // Tránh mọi nơi đã thả các nhóm khác (để quái vật không "đẻ" ngay cạnh làng người)
        bool tooClose = false;
        for (auto [ux, uy] : gUsedSpots) {
            int ux2 = static_cast<int>(std::fabs(WrapSpace::dx(static_cast<float>(x - ux))));
            if (ux2 * ux2 + (y - uy) * (y - uy) < 30 * 30) { tooClose = true; break; }
        }
        if (tooClose && tries < 450) continue;
        // Trên hành tinh hình cầu: tránh vùng gần cực (băng, lãnh nguyên) — ưu tiên vĩ độ ôn hoà
        float lat = std::fabs(y / static_cast<float>(w.height()) - 0.5f) * 2.0f;
        if (lat > 0.72f) continue;
        float score = 0;
        int stones = 0, trees = 0;
        for (int dy = -7; dy <= 7; dy += 1)
            for (int dx = -7; dx <= 7; dx += 1) {
                const Tile& t = w.at(x + dx, y + dy);
                if (w.walkable(x + dx, y + dy)) score += 1;
                score += t.food / 120.0f;
                stones += t.stone > 20;
                trees += t.wood > 20;
                if (t.terrain == Terrain::Snow || t.terrain == Terrain::Desert || t.terrain == Terrain::Ice || t.terrain == Terrain::Tundra) score -= 1.5f;
            }
        score += std::min(stones, 12) * 1.5f + std::min(trees, 25) * 0.8f;
        if (needStone && (stones < 3 || trees < 5)) score *= 0.4f;
        if (score > best) { best = score; ox = x; oy = y; found = true; }
    }
    if (found) gUsedSpots.push_back({ox, oy});
    return found;
}

void spawnHerd(Simulation& sim, int sp, int n, int cx, int cy, float spread, float intelBoost = 0) {
    for (int i = 0; i < n; ++i) {
        Genome g = sim.species[sp].baseGenome;
        for (float& v : g.g) v = clamp01(v + sim.rng.gauss(0.05f));
        g[Gene::Intelligence] = clamp01(g[Gene::Intelligence] + intelBoost);
        sim.spawnCreature(sp, {cx + sim.rng.range(-spread, spread), cy + sim.rng.range(-spread, spread)}, &g);
    }
}

// Thả n cá thể ở các ô ngẫu nhiên SỐNG ĐƯỢC với loài (nước cho cá, hang cho dơi...),
// ưu tiên sinh cảnh phù hợp (voi ma mút: xứ lạnh; cá sấu: đầm lầy; khỉ: rừng rậm...).
void spawnRandomly(Simulation& sim, int sp, int n) {
    World& w = sim.world;
    auto prefers = [&](int x, int y) {
        Terrain t = w.at(x, y).terrain;
        switch (sp) {
        case sp::Mammoth: return t == Terrain::Tundra || t == Terrain::Snow || t == Terrain::Forest;
        case sp::Crocodile: return t == Terrain::Swamp || (t == Terrain::ShallowWater && !w.isWater(x + 1, y + 1));
        case sp::Monkey: return t == Terrain::Jungle || t == Terrain::Forest;
        case sp::Boar: return t == Terrain::Forest || t == Terrain::Jungle;
        case sp::Fish: return t == Terrain::ShallowWater || t == Terrain::DeepWater;
        case sp::Shark: return t == Terrain::DeepWater;
        default: return true;
        }
    };
    for (int i = 0, placed = 0; placed < n && i < n * 60; ++i) {
        int x = sim.rng.irange(0, w.width() - 1), y = sim.rng.irange(5, w.height() - 6);
        if (!sim.habitable(sp, x, y)) continue;
        if (i < n * 40 && !prefers(x, y)) continue;   // cuối cùng mới chấp nhận sinh cảnh "tạm được"
        if (sim.spawnCreature(sp, {x + 0.5f, y + 0.5f}) != NullEntity) placed++;
    }
}

// Muông thú trên cạn, dưới nước, trên trời, trong hang — dùng cho mọi kịch bản
void spawnBiodiversity(Simulation& sim, float k) {
    auto n = [&](int v) { return std::max(1, static_cast<int>(v * k)); };
    spawnRandomly(sim, sp::Fish, n(70));
    spawnRandomly(sim, sp::Shark, n(6));
    spawnRandomly(sim, sp::Sparrow, n(40));
    spawnRandomly(sim, sp::Eagle, n(6));
    spawnRandomly(sim, sp::Boar, n(16));
    spawnRandomly(sim, sp::Mammoth, n(10));
    spawnRandomly(sim, sp::Fox, n(8));
    spawnRandomly(sim, sp::Monkey, n(14));
    spawnRandomly(sim, sp::Crocodile, n(5));
    spawnRandomly(sim, sp::Bat, n(30));
    spawnRandomly(sim, sp::CaveSpider, n(10));
}
} // namespace

bool reseedSapients(Simulation& sim, int minSapients) {
    int sapients = 0;
    for (Entity e : sim.agents) {
        const Body& b = sim.reg.get<Body>(e);
        if (!b.dead && sim.species[b.species].dexterous && !sim.species[b.species].monster) ++sapients;
    }
    if (sapients >= minSapients) return false;
    static const int kinds[] = {sp::Human, sp::Elf, sp::Dwarf, sp::Orc};
    int spc = kinds[sim.rng.irange(0, 3)];
    int x, y;
    gUsedSpots.clear();
    if (!findHabitat(sim, x, y, true) && !findHabitat(sim, x, y, false)) return false;
    spawnHerd(sim, spc, 8, x, y, 2.5f);
    sim.chronicle.add(sim.tick, EventKind::Evolution,
                      "Thần linh gieo mầm sự sống: một bộ lạc " + sim.species[spc].name + " mới xuất hiện (các loài có trí khôn đã gần tuyệt chủng)", x, y);
    return true;
}

void setupScenario(Simulation& sim, Scenario s) {
    gUsedSpots.clear();
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
        spawnBiodiversity(sim, 1.0f);
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
        spawnBiodiversity(sim, 0.4f);
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
        spawnBiodiversity(sim, 0.4f);
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
            int x = sim.rng.irange(0, w.width() - 1), y = sim.rng.irange(8, w.height() - 9);
            int ddx = static_cast<int>(std::fabs(WrapSpace::dx(static_cast<float>(x - ax))));
            if (!w.walkable(x, y) || ddx * ddx + (y - ay) * (y - ay) < 50 * 50) continue;
            if (std::fabs(y / static_cast<float>(w.height()) - 0.5f) * 2.0f > 0.75f) continue;   // lạnh nhưng không phải chỏm băng
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
        spawnBiodiversity(sim, 0.4f);
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
