// =============================================================================
// World.cpp — Sinh thế giới và mô phỏng môi trường.
// =============================================================================
#include "World.h"
#include "Noise.h"
#include "../ai/Knowledge.h"

namespace wb {

const BuildingInfo& buildingInfo(Building b) {
    static const BuildingInfo table[] = {
        // tên               gỗ đá quặng  tech cần                    chỗ ở  chặn
        {"Không",             0,  0, 0, -1,                            0, false},
        {"Hang ổ",            0,  0, 0, ti(Tech::Shelter),             4, false},
        {"Tổ",                1,  0, 0, -1,                            2, false},
        {"Lều",               3,  0, 0, ti(Tech::Shelter),             3, false},
        {"Nhà gỗ",            6,  0, 0, ti(Tech::WoodWorking),         5, false},
        {"Nhà đá",            2,  6, 0, ti(Tech::Masonry),             8, false},
        {"Tường thành",       0,  3, 0, ti(Tech::Masonry),             0, true},
        {"Tháp canh",         2,  6, 0, ti(Tech::Masonry),             0, true},
        {"Cầu",               4,  0, 0, ti(Tech::Bridges),             0, false},
        {"Kho lương",         6,  0, 0, ti(Tech::Farming),             0, false},
        {"Xưởng",             5,  2, 0, ti(Tech::WoodWorking),         0, false},
        {"Lò rèn",            2,  6, 2, ti(Tech::Metallurgy),          0, false},
        {"Đền thờ",           4,  6, 0, ti(Tech::Religion),            0, false},
        {"Toà thị chính",     8,  6, 0, ti(Tech::Law),                 0, false},
        {"Lâu đài",           4, 16, 4, ti(Tech::Law),                10, true},
        {"Dấu lãnh thổ",      0,  0, 0, ti(Tech::ScentMarking),        0, false},
        {"Vật tổ",            2,  0, 0, ti(Tech::Religion),            0, false},
    };
    return table[static_cast<int>(b)];
}

void World::generate(int width, int height, uint32_t seed) {
    w_ = width; h_ = height;
    tiles_.assign(w_ * h_, Tile{});
    dirtyFlag_.assign(w_ * h_, 0);
    items_.clear();
    fires_.clear();
    ValueNoise hN(seed), mN(seed * 7 + 3), tN(seed * 13 + 11), rN(seed * 31 + 5);
    Rng rng(seed);
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            float nx = x / 48.0f, ny = y / 48.0f;
            float hgt = hN.fbm(nx, ny, 5);
            // Hạ thấp biên bản đồ để tạo cảm giác lục địa giữa đại dương (như WorldBox)
            float dx = (x - w_ * 0.5f) / (w_ * 0.5f), dy = (y - h_ * 0.5f) / (h_ * 0.5f);
            float edge = std::sqrt(dx * dx + dy * dy);
            hgt = hgt * 1.15f - std::max(0.0f, edge - 0.55f) * 0.9f;
            float moist = mN.fbm(nx * 1.3f, ny * 1.3f, 4);
            // nhiệt độ: lạnh ở hai cực bắc/nam + nhiễu + lạnh theo độ cao
            float lat = std::fabs(y / static_cast<float>(h_) - 0.5f) * 2.0f;
            float temp = (1.0f - lat * 1.3f) + (tN.fbm(nx, ny, 3) - 0.5f) * 0.6f - std::max(0.f, hgt - 0.6f) * 2.f;
            Tile& t = at(x, y);
            t.height = clamp01(hgt);
            classify(x, y, hgt, moist, temp);
            // tài nguyên
            float r = rN.fbm(nx * 3, ny * 3, 2);
            if (t.terrain == Terrain::Forest || t.terrain == Terrain::Jungle) t.wood = static_cast<uint8_t>(120 + rng.irange(0, 120));
            if (t.terrain == Terrain::Hill || t.terrain == Terrain::Mountain) {
                t.stone = static_cast<uint8_t>(100 + rng.irange(0, 150));
                if (r > 0.62f) t.ore = static_cast<uint8_t>(60 + rng.irange(0, 150));
            }
            if ((t.terrain == Terrain::Grass || t.terrain == Terrain::Sand) && rng.chance(0.02f)) t.stone = static_cast<uint8_t>(rng.irange(10, 60));
            float fert = terrainInfo(t.terrain).fertility;
            if (rng.chance(fert * 0.35f)) t.food = static_cast<uint8_t>(rng.irange(20, 120));
        }
    fullRedraw = true;
}

// Phân loại địa hình theo độ cao / độ ẩm / nhiệt độ (biome).
void World::classify(int x, int y, float h, float moist, float temp) {
    Tile& t = at(x, y);
    if (h < 0.36f) t.terrain = Terrain::DeepWater;
    else if (h < 0.42f) t.terrain = Terrain::ShallowWater;
    else if (h < 0.445f) t.terrain = temp < -0.2f ? Terrain::Snow : Terrain::Sand;
    else if (h > 0.78f) t.terrain = Terrain::Mountain;
    else if (h > 0.68f) t.terrain = temp < -0.1f ? Terrain::Snow : Terrain::Hill;
    else if (temp < -0.35f) t.terrain = Terrain::Snow;
    else if (temp < -0.05f) t.terrain = moist > 0.55f ? Terrain::Forest : Terrain::Tundra;
    else if (temp > 0.75f) t.terrain = moist < 0.45f ? Terrain::Desert : (moist > 0.6f ? Terrain::Jungle : Terrain::Savanna);
    else if (moist > 0.66f && h < 0.5f) t.terrain = Terrain::Swamp;
    else if (moist > 0.52f) t.terrain = Terrain::Forest;
    else t.terrain = Terrain::Grass;
}

bool World::isWater(int x, int y) const {
    if (!inBounds(x, y)) return false;
    Terrain t = at(x, y).terrain;
    return t == Terrain::DeepWater || t == Terrain::ShallowWater;
}

bool World::walkable(int x, int y) const {
    if (!inBounds(x, y)) return false;
    const Tile& t = at(x, y);
    if (t.building == Building::Bridge) return true;
    if (t.building != Building::None && buildingInfo(t.building).blocksMove) return false;
    return terrainInfo(t.terrain).walkable;
}

float World::moveCost(int x, int y) const {
    if (!inBounds(x, y)) return 99.f;
    const Tile& t = at(x, y);
    if (t.building == Building::Bridge) return 0.8f;
    return terrainInfo(t.terrain).moveCost;
}

float World::temperatureAt(int x, int y) const {
    if (!inBounds(x, y)) return 0;
    return terrainInfo(at(x, y).terrain).temperature + weather_.seasonTemp * 0.4f;
}

bool World::findNearestTile(int cx, int cy, int r, const std::function<bool(const Tile&, int, int)>& pred,
                            int& ox, int& oy) const {
    // Quét theo các "vòng vuông" đồng tâm: tìm được ô gần trước → dừng sớm.
    for (int ring = 0; ring <= r; ++ring) {
        int best = 1 << 30;
        for (int dy = -ring; dy <= ring; ++dy)
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring) continue;
                int x = cx + dx, y = cy + dy;
                if (!inBounds(x, y)) continue;
                if (pred(at(x, y), x, y)) {
                    int d = dx * dx + dy * dy;
                    if (d < best) { best = d; ox = x; oy = y; }
                }
            }
        if (best != (1 << 30)) return true;
    }
    return false;
}

void World::dropItem(int x, int y, ItemKind k, int amount, int techHint) {
    if (!inBounds(x, y)) return;
    if (items_.size() > 3000) items_.erase(items_.begin()); // giới hạn bộ nhớ
    items_.push_back(GroundItem{x, y, k, amount, 0, techHint});
    at(x, y).itemFlag = 1;
}

void World::markDirty(int x, int y) {
    int i = y * w_ + x;
    if (!dirtyFlag_[i]) { dirtyFlag_[i] = 1; dirty_.push_back(i); }
}

void World::clearDirty() {
    for (int i : dirty_) dirtyFlag_[i] = 0;
    dirty_.clear();
}

void World::setTerrain(int x, int y, Terrain t) {
    if (!inBounds(x, y)) return;
    Tile& tile = at(x, y);
    if (tile.terrain == t) return;
    tile.terrain = t;
    tile.wood = (t == Terrain::Forest || t == Terrain::Jungle) ? 180 : 0;
    if (t == Terrain::Mountain || t == Terrain::Hill) tile.stone = 160;
    if (!terrainInfo(t).walkable && tile.building != Building::Bridge) tile.building = Building::None;
    if (t == Terrain::DeepWater || t == Terrain::ShallowWater) { tile.fire = 0; tile.food = 0; }
    markDirty(x, y);
}

void World::setBuilding(int x, int y, Building b) {
    if (!inBounds(x, y)) return;
    Tile& t = at(x, y);
    t.building = b;
    t.buildingHp = b == Building::None ? 0 : 100;
    markDirty(x, y);
}

void World::ignite(int x, int y) {
    if (!inBounds(x, y) || isWater(x, y)) return;
    Tile& t = at(x, y);
    if (t.fire) return;
    t.fire = 40;
    fires_.push_back(y * w_ + x);
    markDirty(x, y);
}

void World::update(long tick, Rng& rng) {
    // ---- Mùa: chu kỳ hình sin theo năm ----
    weather_.seasonTemp = std::sin(tick * 6.2831853f / kTicksPerYear);

    // ---- Mưa: một vùng mưa trôi qua bản đồ ----
    if (weather_.rainTicks > 0) {
        weather_.rainTicks--;
        weather_.rainX += weather_.rainVX;
        weather_.rainY += weather_.rainVY;
    } else if (weather_.droughtTicks <= 0 && rng.chance(0.004f)) {
        weather_.rainTicks = rng.irange(150, 400);
        weather_.rainX = rng.range(0, w_); weather_.rainY = rng.range(0, h_);
        weather_.rainRadius = rng.range(15, 35);
        weather_.rainVX = rng.range(-0.15f, 0.15f); weather_.rainVY = rng.range(-0.15f, 0.15f);
    }
    if (weather_.droughtTicks > 0) weather_.droughtTicks--;

    // ---- Cây cối mọc lại: lấy mẫu ngẫu nhiên một số ô mỗi tick (rẻ, đều) ----
    int samples = (w_ * h_) / 120;
    float growth = weather_.droughtTicks > 0 ? 0.2f : 1.0f;
    for (int i = 0; i < samples; ++i) {
        int x = rng.irange(0, w_ - 1), y = rng.irange(0, h_ - 1);
        Tile& t = at(x, y);
        float fert = terrainInfo(t.terrain).fertility * growth;
        bool raining = weather_.rainTicks > 0 && dist2({float(x), float(y)}, {weather_.rainX, weather_.rainY}) < weather_.rainRadius * weather_.rainRadius;
        if (raining) fert *= 2.0f;
        if (t.fire) continue;
        if (t.food < 200 && rng.chance(fert * 0.6f)) t.food = static_cast<uint8_t>(std::min(255, t.food + 12));
        if (t.terrain == Terrain::Farmland && t.food < 250) t.food = static_cast<uint8_t>(std::min(255, t.food + 20));
        if ((t.terrain == Terrain::Forest || t.terrain == Terrain::Jungle) && t.wood < 230 && rng.chance(0.5f)) t.wood += 10;
        // Rừng lan sang đồng cỏ bên cạnh rất chậm; tro tàn hồi phục thành cỏ
        if (t.terrain == Terrain::Ash && rng.chance(0.05f)) setTerrain(x, y, Terrain::Grass);
        if (t.terrain == Terrain::Grass && t.building == Building::None && t.owner < 0 && rng.chance(0.004f * fert)) {
            int nx = x + rng.irange(-1, 1), ny = y + rng.irange(-1, 1);
            if (inBounds(nx, ny) && at(nx, ny).terrain == Terrain::Forest) setTerrain(x, y, Terrain::Forest);
        }
        // Rừng bị chặt trụi trở thành đồng cỏ
        if (t.terrain == Terrain::Forest && t.wood < 5) setTerrain(x, y, Terrain::Grass);
    }

    // ---- Lửa: cháy, lan sang ô dễ cháy, bị mưa dập ----
    std::vector<int> next;
    next.reserve(fires_.size());
    for (int idx : fires_) {
        int x = idx % w_, y = idx / w_;
        Tile& t = tiles_[idx];
        if (!t.fire) continue;
        bool raining = weather_.rainTicks > 0 && dist2({float(x), float(y)}, {weather_.rainX, weather_.rainY}) < weather_.rainRadius * weather_.rainRadius;
        t.fire = raining ? 0 : static_cast<uint8_t>(t.fire - 1);
        if (t.wood > 0) t.wood = static_cast<uint8_t>(std::max(0, t.wood - 6));
        t.food = 0;
        if (t.fire == 0) {
            if (t.terrain == Terrain::Forest || t.terrain == Terrain::Jungle || t.terrain == Terrain::Grass || t.terrain == Terrain::Savanna)
                setTerrain(x, y, Terrain::Ash);
            if (t.building != Building::None && t.building != Building::Wall && t.building != Building::StoneHouse && t.building != Building::Castle)
                setBuilding(x, y, Building::None);
            markDirty(x, y);
            continue;
        }
        next.push_back(idx);
        if (rng.chance(0.08f)) {
            int nx = x + rng.irange(-1, 1), ny = y + rng.irange(-1, 1);
            if (inBounds(nx, ny)) {
                Tile& n = at(nx, ny);
                bool flammable = n.wood > 20 || n.terrain == Terrain::Grass || n.terrain == Terrain::Savanna ||
                                 (n.building != Building::None && n.building != Building::Wall && n.building != Building::StoneHouse);
                if (flammable && !n.fire && !isWater(nx, ny)) { n.fire = 40; next.push_back(ny * w_ + nx); markDirty(nx, ny); }
            }
        }
    }
    fires_.swap(next);
    burning_ = static_cast<int>(fires_.size());
    // Sét đánh ngẫu nhiên khi trời mưa → nguồn lửa tự nhiên (giúp sinh vật "phát hiện" lửa)
    if (weather_.rainTicks > 0 && rng.chance(0.01f)) {
        int x = static_cast<int>(weather_.rainX + rng.range(-weather_.rainRadius, weather_.rainRadius));
        int y = static_cast<int>(weather_.rainY + rng.range(-weather_.rainRadius, weather_.rainRadius));
        ignite(x, y);
    }
    // Hạn hán + nóng → cháy rừng tự phát
    if (weather_.droughtTicks > 0 && rng.chance(0.02f)) ignite(rng.irange(0, w_ - 1), rng.irange(0, h_ - 1));

    // ---- Vật phẩm mục rữa ----
    if (tick % 10 == 0) {
        for (auto& it : items_) {
            it.age += 10;
            if (it.kind == ItemKind::Corpse && it.age > 400) { it.kind = ItemKind::Bones; it.amount = 1; }
        }
        items_.erase(std::remove_if(items_.begin(), items_.end(), [](const GroundItem& it) {
                         return it.amount <= 0 || (it.kind == ItemKind::Bones && it.age > 3000) ||
                                (it.kind == ItemKind::Meat && it.age > 300) || it.age > 20000;
                     }), items_.end());
        // Tính lại cờ vật phẩm theo ô
        for (auto& t : tiles_) t.itemFlag = 0;
        for (auto& it : items_) at(it.x, it.y).itemFlag = 1;
    }
}

} // namespace wb
