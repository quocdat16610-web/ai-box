// GodPowers.cpp — Cài đặt các quyền năng của Thần.
#include "GodPowers.h"
#include "Simulation.h"
#include "../ai/Brain.h"

namespace wb {

const char* powerName(Power p) {
    static const char* n[] = {"Quan sát", "Nâng đất", "Hạ đất", "Nước", "Cỏ", "Rừng", "Cát", "Núi", "Tuyết", "Dung nham",
                              "Lửa", "Thiên thạch", "Sét", "Mưa", "Hạn hán", "Động đất", "Dịch bệnh", "Ban phước",
                              "Trừng phạt", "Thả sinh vật", "Đổ nước", "Đổ dung nham", "Đổ axit", "Đổ dầu",
                              "Thuốc súng", "Đốt nóng", "Làm lạnh", "Ném đá tảng", "Mạch quặng"};
    return n[static_cast<int>(p)];
}

bool powerIsBrush(Power p) {
    switch (p) {
    case Power::RaiseLand: case Power::LowerLand: case Power::PaintWater: case Power::PaintGrass:
    case Power::PaintForest: case Power::PaintSand: case Power::PaintMountain: case Power::PaintSnow:
    case Power::PaintLava: case Power::Fire: case Power::PourWater: case Power::PourLava: case Power::PourAcid:
    case Power::PourOil: case Power::Gunpowder: case Power::HeatUp: case Power::CoolDown:
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
    case Power::PaintLava:
        // Hồ dung nham "vĩnh cửu" như núi lửa (nguồn nhiệt 1150°C — đốt cháy rừng xung quanh)
        forDisc(sim, x, y, r, [&](int tx, int ty) {
            w.setTerrain(tx, ty, Terrain::Lava);
            w.at(tx, ty).flags |= kTileVolcanic;
            w.matter.setTemp(tx, ty, 1150);
        });
        break;
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
        // Va chạm siêu tốc: nhiệt độ cực cao (đá nóng chảy), sóng xung kích, mảnh vỡ nóng văng ra
        forDisc(sim, x, y, R / 2 + 1, [&](int tx, int ty) { w.matter.setTemp(tx, ty, 2200); });
        sim.physics.blast(sim, {x + 0.5f, y + 0.5f}, 4000.0f * (1 + r));
        for (int i = 0; i < 12; ++i) {
            PhysBody deb;
            deb.kind = BodyKind::Debris; deb.mat = sim.rng.chance(0.4f) ? mat::IronOre : mat::Basalt;
            deb.mass = 5; deb.radius = 0.3f; deb.temp = 1300; deb.pos = {x + 0.5f, y + 0.5f};
            float a = sim.rng.range(0, 6.2831853f), v = sim.rng.range(8, 22);
            deb.vel = {std::cos(a) * v, std::sin(a) * v}; deb.vz = sim.rng.range(6, 18); deb.life = 300;
            sim.physics.spawn(deb);
        }
        witness(sim, x, y, R);
        sim.chronicle.add(sim.tick, EventKind::Disaster, "Một thiên thạch (mang quặng sắt) rơi xuống thế giới!", x, y);
        break;
    }
    case Power::Lightning:
        w.matter.setTemp(x, y, 3000);   // ~3000°C: đốt cháy, nung chảy cát thành thuỷ tinh
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
    // ---------------- Vật chất ----------------
    case Power::PourWater:
    case Power::PourLava:
    case Power::PourAcid:
    case Power::PourOil:
    case Power::Gunpowder: {
        MatId m = p == Power::PourWater ? mat::Water : p == Power::PourLava ? mat::Basalt : p == Power::PourAcid ? mat::Acid
                : p == Power::PourOil ? mat::Oil : mat::Gunpowder;
        float t = p == Power::PourLava ? 1250.0f : 20.0f;   // dung nham = đá bazan nóng chảy
        forDisc(sim, x, y, std::max(0, r - 1), [&](int tx, int ty) {
            if (w.isWater(tx, ty) && p != Power::PourLava) return;
            w.matter.addMatter(tx, ty, m, p == Power::Gunpowder ? 6 : 25, t);
            if (p == Power::PourLava) w.matter.setTemp(tx, ty, 1250);
        });
        break;
    }
    case Power::HeatUp:
        forDisc(sim, x, y, r, [&](int tx, int ty) { w.matter.addHeat(tx, ty, 120); });
        break;
    case Power::CoolDown:
        forDisc(sim, x, y, r, [&](int tx, int ty) { w.matter.addHeat(tx, ty, -60); });
        break;
    case Power::Boulder: {
        // Tảng đá rơi từ trên trời: trọng lực, nảy, lăn xuống dốc, phá nhà, đè sinh vật
        PhysBody b;
        b.kind = BodyKind::Boulder;
        b.mat = mat::Stone;
        b.mass = 80.0f * (1 + r * 0.3f);
        b.radius = 0.6f;
        b.pos = {x + 0.5f, y + 0.5f};
        b.z = 25;
        b.vel = {sim.rng.range(-3.f, 3.f), sim.rng.range(-3.f, 3.f)};
        b.vz = -5;
        b.sharpness = 0.6f;
        b.life = 600;
        sim.physics.spawn(b);
        break;
    }
    case Power::OreVein: {
        static const MatId ores[] = {mat::CopperOre, mat::TinOre, mat::IronOre, mat::Gold, mat::Sulfur, mat::Saltpeter};
        MatId o = ores[sim.rng.irange(0, 5)];
        if (!w.materials.exoticOres.empty() && sim.rng.chance(0.25f)) o = w.materials.exoticOres[sim.rng.irange(0, static_cast<int>(w.materials.exoticOres.size()) - 1)];
        forDisc(sim, x, y, r, [&](int tx, int ty) {
            Tile& t = w.at(tx, ty);
            if (!w.walkable(tx, ty)) return;
            t.ore = 150; t.oreMat = o; w.markDirty(tx, ty);
        });
        sim.chronicle.add(sim.tick, EventKind::Misc, "Thần tạo mạch " + w.materials[o].name, x, y);
        break;
    }
    default: break;
    }
}

} // namespace wb
