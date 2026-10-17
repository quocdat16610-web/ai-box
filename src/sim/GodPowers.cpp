// GodPowers.cpp — Cài đặt các quyền năng của Thần.
#include "GodPowers.h"
#include "Simulation.h"
#include "../ai/Brain.h"

namespace wb {

const char* powerName(Power p) {
    static const char* n[] = {"Quan sát", "Nâng đất", "Hạ đất", "Nước", "Cỏ", "Rừng", "Cát", "Núi", "Tuyết", "Dung nham",
                              "Lửa", "Thiên thạch", "Sét", "Mưa", "Hạn hán", "Động đất", "Dịch bệnh", "Ban phước",
                              "Trừng phạt", "Thả sinh vật"};
    return n[static_cast<int>(p)];
}

bool powerIsBrush(Power p) {
    switch (p) {
    case Power::RaiseLand: case Power::LowerLand: case Power::PaintWater: case Power::PaintGrass:
    case Power::PaintForest: case Power::PaintSand: case Power::PaintMountain: case Power::PaintSnow:
    case Power::PaintLava: case Power::Fire:
        return true;
    default:
        return false;
    }
}

namespace {
// Phân loại lại địa hình theo độ cao sau khi nâng/hạ.
Terrain terrainForHeight(float h, Terrain old) {
    if (h < 0.36f) return Terrain::DeepWater;
    if (h < 0.42f) return Terrain::ShallowWater;
    if (h < 0.445f) return Terrain::Sand;
    if (h > 0.78f) return Terrain::Mountain;
    if (h > 0.68f) return Terrain::Hill;
    if (old == Terrain::DeepWater || old == Terrain::ShallowWater || old == Terrain::Sand || old == Terrain::Hill || old == Terrain::Mountain)
        return Terrain::Grass;
    return old;
}

template <class F> void forDisc(Simulation& sim, int cx, int cy, int r, F&& f) {
    for (int y = cy - r; y <= cy + r; ++y)
        for (int x = cx - r; x <= cx + r; ++x)
            if (sim.world.inBounds(x, y) && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) f(x, y);
}

// Chứng kiến thiên tai → kích thích tín ngưỡng (sinh vật bắt đầu "tin" có thần linh)
void witness(Simulation& sim, int x, int y, int r) {
    sim.grid.query({x + 0.5f, y + 0.5f}, static_cast<float>(r + 12), [&](Entity e, Vec2) {
        if (sim.reg.has<Mind>(e)) Brain::stimulate(sim, e, Stimulus::SeeDeath, 2);
    });
}
} // namespace

void applyPower(Simulation& sim, Power p, int x, int y, int r, int speciesId) {
    World& w = sim.world;
    switch (p) {
    case Power::RaiseLand:
    case Power::LowerLand:
        forDisc(sim, x, y, r, [&](int tx, int ty) {
            Tile& t = w.at(tx, ty);
            float d = std::sqrt(static_cast<float>((tx - x) * (tx - x) + (ty - y) * (ty - y))) / (r + 1);
            t.height = clamp01(t.height + (p == Power::RaiseLand ? 0.03f : -0.03f) * (1 - d));
            w.setTerrain(tx, ty, terrainForHeight(t.height, t.terrain));
        });
        break;
    case Power::PaintWater: forDisc(sim, x, y, r, [&](int tx, int ty) { w.at(tx, ty).height = 0.38f; w.setTerrain(tx, ty, Terrain::ShallowWater); }); break;
    case Power::PaintGrass: forDisc(sim, x, y, r, [&](int tx, int ty) { w.at(tx, ty).height = 0.5f; w.setTerrain(tx, ty, Terrain::Grass); w.at(tx, ty).food = 60; }); break;
    case Power::PaintForest: forDisc(sim, x, y, r, [&](int tx, int ty) { w.at(tx, ty).height = 0.55f; w.setTerrain(tx, ty, Terrain::Forest); w.at(tx, ty).food = 80; }); break;
    case Power::PaintSand: forDisc(sim, x, y, r, [&](int tx, int ty) { w.at(tx, ty).height = 0.44f; w.setTerrain(tx, ty, Terrain::Sand); }); break;
    case Power::PaintMountain:
        forDisc(sim, x, y, r, [&](int tx, int ty) {
            w.at(tx, ty).height = 0.85f; w.setTerrain(tx, ty, Terrain::Mountain);
            if (sim.rng.chance(0.3f)) w.at(tx, ty).ore = static_cast<uint8_t>(sim.rng.irange(60, 200));
        });
        break;
    case Power::PaintSnow: forDisc(sim, x, y, r, [&](int tx, int ty) { w.setTerrain(tx, ty, Terrain::Snow); }); break;
    case Power::PaintLava: forDisc(sim, x, y, r, [&](int tx, int ty) { w.setTerrain(tx, ty, Terrain::Lava); w.ignite(tx + 1, ty); }); break;
    case Power::Fire: forDisc(sim, x, y, r, [&](int tx, int ty) { if (sim.rng.chance(0.5f)) w.ignite(tx, ty); }); break;
    case Power::Meteor: {
        int R = r + 3;
        forDisc(sim, x, y, R, [&](int tx, int ty) {
            w.setBuilding(tx, ty, Building::None);
            Tile& t = w.at(tx, ty);
            t.height = clamp01(t.height - 0.1f);
            w.setTerrain(tx, ty, (tx - x) * (tx - x) + (ty - y) * (ty - y) < (R / 2) * (R / 2) ? Terrain::Lava : Terrain::Ash);
            if (sim.rng.chance(0.3f)) t.ore = static_cast<uint8_t>(sim.rng.irange(80, 255)); // thiên thạch mang kim loại!
            if (sim.rng.chance(0.2f)) w.ignite(tx, ty);
        });
        sim.grid.query({x + 0.5f, y + 0.5f}, static_cast<float>(R), [&](Entity e, Vec2) { sim.kill(e, 3); });
        witness(sim, x, y, R);
        sim.chronicle.add(sim.tick, EventKind::Disaster, "Một thiên thạch rơi xuống thế giới!", x, y);
        break;
    }
    case Power::Lightning:
        w.ignite(x, y);
        sim.grid.query({x + 0.5f, y + 0.5f}, 1.5f, [&](Entity e, Vec2) { sim.kill(e, 3); });
        witness(sim, x, y, 2);
        break;
    case Power::Rain: {
        Weather& wt = w.weather();
        wt.rainX = static_cast<float>(x); wt.rainY = static_cast<float>(y);
        wt.rainRadius = static_cast<float>(r * 3 + 10); wt.rainTicks = 400; wt.rainVX = wt.rainVY = 0; wt.droughtTicks = 0;
        break;
    }
    case Power::Drought:
        w.weather().droughtTicks = 1500; w.weather().rainTicks = 0;
        sim.chronicle.add(sim.tick, EventKind::Disaster, "Hạn hán lan khắp thế giới", x, y);
        break;
    case Power::Earthquake: {
        int R = r + 8;
        forDisc(sim, x, y, R, [&](int tx, int ty) {
            Tile& t = w.at(tx, ty);
            if (t.building != Building::None && sim.rng.chance(0.4f)) w.setBuilding(tx, ty, Building::None);
            if (sim.rng.chance(0.08f)) { t.height = clamp01(t.height + sim.rng.range(-0.08f, 0.08f)); w.setTerrain(tx, ty, terrainForHeight(t.height, t.terrain)); }
        });
        sim.grid.query({x + 0.5f, y + 0.5f}, static_cast<float>(R), [&](Entity e, Vec2) {
            if (Body* b = sim.reg.tryGet<Body>(e)) { b->hp -= 15; if (b->hp <= 0) sim.kill(e, 3); }
        });
        witness(sim, x, y, R);
        sim.chronicle.add(sim.tick, EventKind::Disaster, "Động đất rung chuyển mặt đất", x, y);
        break;
    }
    case Power::Plague:
        sim.grid.query({x + 0.5f, y + 0.5f}, static_cast<float>(r + 10), [&](Entity e, Vec2) {
            if (Body* b = sim.reg.tryGet<Body>(e)) { b->hp *= 0.4f; b->hunger = std::min(1.0f, b->hunger + 0.3f); if (sim.rng.chance(0.25f)) sim.kill(e, 3); }
        });
        witness(sim, x, y, r + 10);
        sim.chronicle.add(sim.tick, EventKind::Disaster, "Dịch bệnh bùng phát", x, y);
        break;
    case Power::Blessing:
        sim.grid.query({x + 0.5f, y + 0.5f}, static_cast<float>(r + 4), [&](Entity e, Vec2) {
            if (Mind* m = sim.reg.tryGet<Mind>(e)) {
                Genome& g = sim.reg.get<GenomeC>(e).genome;
                g[Gene::Intelligence] = clamp01(g[Gene::Intelligence] + 0.08f); // di truyền được cho con cháu
                m->learned = std::min(0.25f, m->learned + 0.05f);
                Body& b = sim.reg.get<Body>(e);
                b.hp = b.maxHp; b.hunger = 0;
                Brain::updateIntellect(sim, e);
                m->think("Cảm nhận được ân sủng của thần linh!");
            }
        });
        break;
    case Power::Smite:
        sim.grid.query({x + 0.5f, y + 0.5f}, static_cast<float>(r + 1), [&](Entity e, Vec2) { sim.kill(e, 3); });
        break;
    case Power::Spawn: {
        int n = std::max(1, r);
        for (int i = 0; i < n; ++i)
            sim.spawnCreature(speciesId, {x + 0.5f + sim.rng.range(-2.f, 2.f), y + 0.5f + sim.rng.range(-2.f, 2.f)});
        break;
    }
    default: break;
    }
}

} // namespace wb
