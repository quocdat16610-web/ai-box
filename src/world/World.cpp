// =============================================================================
// World.cpp — Sinh thế giới và mô phỏng môi trường.
// =============================================================================
#include "World.h"
#include "Noise.h"
#include "../ai/Knowledge.h"

namespace wb {

const BuildingInfo& buildingInfo(Building b) {
    // Yêu cầu xây dựng là QUY LUẬT VẬT LÝ chứ không phải ô công nghệ: muốn dựng nhà gỗ
    // phải có công cụ cắt được gỗ (độ cắt ≥ ~2), nhà đá cần công cụ cắt được đá (≥ ~4.5)
    // hoặc có gạch nung... Ai, ở đâu, bằng vật liệu gì đạt được ngưỡng đó là chuyện mở.
    static const BuildingInfo table[] = {
        // tên               gỗ đá kim  tập quán cần            chỗ ở chặn  cắt≥  cháy được
        {"Không",             0,  0, 0, -1,                       0, false, 0.0f, false},
        {"Hang ổ",            0,  0, 0, ti(Tech::Shelter),        4, false, 0.0f, false},
        {"Tổ",                1,  0, 0, -1,                       2, false, 0.0f, true},
        {"Lều",               3,  0, 0, ti(Tech::Shelter),        3, false, 0.0f, true},
        {"Nhà gỗ",            6,  0, 0, ti(Tech::Shelter),        5, false, 2.0f, true},
        {"Nhà đá",            2,  6, 0, ti(Tech::Shelter),        8, false, 4.5f, false},
        {"Tường thành",       0,  3, 0, -1,                       0, true,  4.5f, false},
        {"Tháp canh",         2,  6, 0, -1,                       0, true,  4.5f, false},
        {"Cầu",               4,  0, 0, -1,                       0, false, 2.0f, true},
        {"Kho lương",         6,  0, 0, ti(Tech::Farming),        0, false, 2.0f, true},
        {"Xưởng",             5,  2, 0, -1,                       0, false, 2.0f, true},
        {"Lò luyện",          0,  6, 0, -1,                       0, false, 0.0f, false},
        {"Đền thờ",           4,  6, 0, ti(Tech::Religion),       0, false, 2.0f, false},
        {"Toà thị chính",     8,  6, 0, ti(Tech::Law),            0, false, 2.0f, false},
        {"Lâu đài",           4, 16, 4, ti(Tech::Law),           10, true,  4.5f, false},
        {"Dấu lãnh thổ",      0,  0, 0, ti(Tech::ScentMarking),   0, false, 0.0f, false},
        {"Vật tổ",            2,  0, 0, ti(Tech::Religion),       0, false, 0.0f, true},
        {"Bếp lửa",           2,  3, 0, -1,                       0, false, 0.0f, false},
        {"Hầm mỏ",            2,  0, 0, -1,                       0, false, 0.0f, false},
    };
    return table[static_cast<int>(b)];
}

MatId groundMaterial(Terrain t) {
    switch (t) {
    case Terrain::DeepWater: case Terrain::ShallowWater: case Terrain::Ice: case Terrain::Snow: return mat::Water;
    case Terrain::Sand: case Terrain::Desert: return mat::Sand;
    case Terrain::Hill: case Terrain::Mountain: return mat::Stone;
    case Terrain::Lava: case Terrain::Basalt: return mat::Basalt;
    case Terrain::Swamp: return mat::Mud;
    case Terrain::Ash: return mat::Ash;
    case Terrain::Glass: return mat::Glass;
    default: return mat::Dirt;
    }
}

const char* worldTypeName(WorldType t) {
    static const char* n[] = {"Ngẫu nhiên", "Siêu lục địa", "Nhiều châu lục", "Quần đảo", "Hành tinh đại dương", "Hành tinh khô"};
    return n[static_cast<int>(t)];
}

namespace {
struct V3 { float x, y, z; };
inline float dot3(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
// Điểm trên mặt cầu đơn vị ứng với ô (x, y): x → kinh độ, y → vĩ độ
inline V3 spherePoint(float x, float y, int w, int h) {
    float lon = x / w * 6.2831853f;
    float lat = ((y + 0.5f) / h - 0.5f) * 3.14159265f;
    return {std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon)};
}
V3 randomUnit(Rng& rng) {
    for (;;) {
        V3 v{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)};
        float l = std::sqrt(dot3(v, v));
        if (l > 0.2f && l <= 1) return {v.x / l, v.y / l * 0.8f, v.z / l};   // ít lục địa sát cực
    }
}
} // namespace

void World::generate(int width, int height, uint32_t seed, WorldType type) {
    w_ = width; h_ = height;
    WrapSpace::width = static_cast<float>(w_);   // bản đồ quấn vòng đông–tây
    WrapSpace::height = static_cast<float>(h_);
    tiles_.assign(w_ * h_, Tile{});
    dirtyFlag_.assign(w_ * h_, 0);
    dirty_.clear();
    items_.clear();
    weather_ = Weather{};
    materials.initDefaults(seed);
    ValueNoise hN(seed), mN(seed * 7 + 3), tN(seed * 13 + 11), rN(seed * 31 + 5), oN(seed * 57 + 9), wN(seed * 91 + 17);
    Rng rng(seed);

    // ---- 1. Kiểu hành tinh: số mảng lục địa, kích thước, tỉ lệ đất liền ----
    if (type == WorldType::Random) type = static_cast<WorldType>(rng.irange(1, static_cast<int>(WorldType::Count) - 1));
    type_ = type;
    int plates = 0;
    float plateR = 0.6f, landFrac = 0.38f, noiseW = 0.55f, freq = 1.0f;
    switch (type) {
    case WorldType::Pangaea:     plates = 1;                  plateR = 1.10f; landFrac = 0.33f; noiseW = 0.58f; break;
    case WorldType::Continents:  plates = rng.irange(3, 7);   plateR = 0.55f; landFrac = 0.36f; noiseW = 0.62f; break;
    case WorldType::Archipelago: plates = rng.irange(28, 45); plateR = 0.16f; landFrac = 0.20f; noiseW = 0.55f; freq = 2.6f; break;
    case WorldType::OceanWorld:  plates = rng.irange(0, 3);   plateR = 0.10f; landFrac = 0.07f; noiseW = 0.85f; freq = 2.2f; break;
    case WorldType::DryWorld:    plates = 0;                  plateR = 0.0f;  landFrac = 0.80f; noiseW = 1.00f; break;
    default: break;
    }
    continents_ = plates;
    std::vector<V3> centers;
    std::vector<float> radii;
    for (int i = 0; i < plates; ++i) { centers.push_back(randomUnit(rng)); radii.push_back(plateR * rng.range(0.7f, 1.3f)); }

    // ---- 2. Độ cao thô = nhiễu fBm 3D trên mặt cầu + "mảng lục địa" ----
    // Bán kính cầu trong đơn vị nhiễu sao cho xích đạo dài W/48 đơn vị (giữ cỡ địa hình như trước)
    const float R = w_ / (48.0f * 6.2831853f);
    std::vector<float> raw(w_ * h_), ridge(w_ * h_);
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            V3 p = spherePoint(static_cast<float>(x), static_cast<float>(y), w_, h_);
            // uốn toạ độ bằng nhiễu → bờ biển lởm chởm, lục địa méo mó tự nhiên
            float wx = (wN.fbm3(p.x * R * 0.6f, p.y * R * 0.6f, p.z * R * 0.6f, 3) - 0.5f) * 0.9f;
            float wz = (wN.fbm3(p.z * R * 0.6f + 7, p.x * R * 0.6f, p.y * R * 0.6f, 3) - 0.5f) * 0.9f;
            V3 q{p.x + wx, p.y + (wx - wz) * 0.4f, p.z + wz};
            float ql = std::sqrt(dot3(q, q));
            q = {q.x / ql, q.y / ql, q.z / ql};
            float n = hN.fbm3(q.x * R * freq + 50, q.y * R * freq + 50, q.z * R * freq + 50, 6);
            // đảo nhỏ & bờ biển chi tiết: nhiễu tần số cao (quan trọng với quần đảo / hành tinh đại dương)
            float fine = hN.fbm3(q.x * R * 3 + 9, q.y * R * 3 + 9, q.z * R * 3 + 9, 3);
            float mask = 0;
            for (size_t k = 0; k < centers.size(); ++k) {
                float ang = std::acos(std::clamp(dot3(q, centers[k]), -1.0f, 1.0f));   // khoảng cách góc trên cầu
                float m = 1.0f - ang / radii[k];
                mask = std::max(mask, m > 0 ? m * m * (3 - 2 * m) : 0.0f);            // smoothstep: lõi phẳng, rìa thoải
            }
            float smallIsl = (type == WorldType::Archipelago || type == WorldType::OceanWorld) ? 0.35f : 0.12f;
            raw[y * w_ + x] = n * noiseW + mask * (1.0f - noiseW) + (fine - 0.5f) * smallIsl;
            // Dãy núi: nhiễu "gờ" (ridged) — núi chạy thành dải thay vì một khối tròn giữa lục địa
            float rg = 1.0f - std::fabs(2.0f * hN.fbm3(q.x * R * 1.6f + 3, q.y * R * 1.6f + 3, q.z * R * 1.6f + 3, 4) - 1.0f);
            ridge[y * w_ + x] = rg * rg * rg;
        }
    // ---- 3. Chuẩn hoá: đặt mực nước biển ở phân vị sao cho đúng tỉ lệ đất liền mong muốn ----
    std::vector<float> sorted = raw;
    size_t qi = static_cast<size_t>(std::clamp(1.0f - landFrac, 0.0f, 0.999f) * (sorted.size() - 1));
    std::nth_element(sorted.begin(), sorted.begin() + qi, sorted.end());
    float sea = sorted[qi];
    float lo = *std::min_element(raw.begin(), raw.end()), hi = *std::max_element(raw.begin(), raw.end());
    for (size_t i = 0; i < raw.size(); ++i) {
        float v = raw[i];
        if (v >= sea) {
            float t = (v - sea) / std::max(1e-4f, hi - sea);          // 0 = bờ biển, 1 = điểm cao nhất
            raw[i] = 0.42f + std::pow(t, 1.3f) * 0.30f + ridge[i] * std::min(1.0f, t * 4.0f) * 0.24f;
        } else {
            raw[i] = 0.42f - (sea - v) / std::max(1e-4f, sea - lo) * 0.30f;   // biển: 0.12 → 0.42
        }
    }
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            V3 p = spherePoint(static_cast<float>(x), static_cast<float>(y), w_, h_);
            float hgt = raw[y * w_ + x];
            float moist = mN.fbm3(p.x * R * 1.3f, p.y * R * 1.3f, p.z * R * 1.3f, 4);
            // nhiệt độ: lạnh ở hai cực bắc/nam + nhiễu + lạnh theo độ cao
            float lat = std::fabs(y / static_cast<float>(h_) - 0.5f) * 2.0f;
            float temp = (1.0f - lat * 1.3f) + (tN.fbm3(p.x * R, p.y * R, p.z * R, 3) - 0.5f) * 0.6f - std::max(0.f, hgt - 0.6f) * 2.f;
            if (type == WorldType::DryWorld) moist *= 0.8f;   // hành tinh khô: ít mưa
            Tile& t = at(x, y);
            t.height = clamp01(hgt);
            classify(x, y, hgt, moist, temp);
            // Chỏm băng hai cực: biển gần cực đóng băng (rìa băng lởm chởm theo nhiễu)
            float edgeN = hN.fbm3(p.x * R * 4 + 31, p.y * R * 4, p.z * R * 4 - 13, 3);
            if (lat > 0.82f + (edgeN - 0.5f) * 0.22f && (t.terrain == Terrain::DeepWater || t.terrain == Terrain::ShallowWater))
                t.terrain = Terrain::Ice;
            // ---- Tài nguyên: gỗ, đá (nhiều loại), quặng (nhiều loại) ----
            float r = rN.fbm3(p.x * R * 3, p.y * R * 3, p.z * R * 3, 2);
            float o = oN.fbm3(p.x * R * 2.2f, p.y * R * 2.2f, p.z * R * 2.2f, 2); // "mạch khoáng" quyết định loại quặng theo vùng
            if (t.terrain == Terrain::Forest || t.terrain == Terrain::Jungle) t.wood = static_cast<uint8_t>(120 + rng.irange(0, 120));
            if (t.terrain == Terrain::Hill || t.terrain == Terrain::Mountain) {
                t.stone = static_cast<uint8_t>(100 + rng.irange(0, 150));
                // Loại đá theo vùng: đá vôi (trầm tích), đá lửa (mạch silic), đá thường
                t.stoneMat = o < 0.35f ? mat::Limestone : (rng.chance(0.18f) ? mat::Flint : mat::Stone);
                if (r > 0.6f) {
                    t.ore = static_cast<uint8_t>(60 + rng.irange(0, 150));
                    if (o < 0.40f) t.oreMat = mat::CopperOre;
                    else if (o < 0.52f) t.oreMat = mat::IronOre;
                    else if (o < 0.60f) t.oreMat = mat::TinOre;
                    else if (o < 0.64f) t.oreMat = mat::Gold;
                    else t.oreMat = mat::IronOre;
                    if (r > 0.74f && !materials.exoticOres.empty())   // mỏ khoáng chất lạ — hiếm
                        t.oreMat = materials.exoticOres[static_cast<size_t>(o * 100) % materials.exoticOres.size()];
                }
            }
            if ((t.terrain == Terrain::Grass || t.terrain == Terrain::Sand) && rng.chance(0.02f)) {
                t.stone = static_cast<uint8_t>(rng.irange(10, 60));
                t.stoneMat = rng.chance(0.3f) ? mat::Flint : mat::Stone;
            }
            if (t.terrain == Terrain::Desert && rng.chance(0.015f)) { t.ore = static_cast<uint8_t>(rng.irange(30, 90)); t.oreMat = mat::Saltpeter; }
            float fert = terrainInfo(t.terrain).fertility;
            if (rng.chance(fert * 0.35f)) t.food = static_cast<uint8_t>(rng.irange(20, 120));
            if (t.terrain == Terrain::ShallowWater || t.terrain == Terrain::DeepWater) t.food = static_cast<uint8_t>(rng.irange(20, 90)); // tảo, phù du
        }
    // Đất sét ở ven sông/hồ: đất gần nước
    for (int y = 1; y < h_ - 1; ++y)
        for (int x = 1; x < w_ - 1; ++x) {
            Tile& t = at(x, y);
            if (!terrainInfo(t.terrain).walkable || t.stone > 0) continue;
            bool nearWater = isWater(x + 1, y) || isWater(x - 1, y) || isWater(x, y + 1) || isWater(x, y - 1);
            if (nearWater && t.terrain != Terrain::Snow && rng.chance(0.08f)) { t.stone = static_cast<uint8_t>(rng.irange(20, 80)); t.stoneMat = mat::Clay; }
        }
    // Núi lửa: vài hồ dung nham trên đỉnh núi, xung quanh có lưu huỳnh & đá vỏ chai
    int volcanoes = std::max(1, w_ * h_ / 15000);
    for (int v = 0; v < volcanoes; ++v) {
        for (int tries = 0; tries < 200; ++tries) {
            int x = rng.irange(4, w_ - 5), y = rng.irange(4, h_ - 5);
            if (at(x, y).terrain != Terrain::Mountain) continue;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) { at(x + dx, y + dy).terrain = Terrain::Lava; at(x + dx, y + dy).flags |= kTileVolcanic; }
            for (int dy = -4; dy <= 4; ++dy)
                for (int dx = -4; dx <= 4; ++dx) {
                    Tile& t = at(x + dx, y + dy);
                    if (t.terrain == Terrain::Lava) continue;
                    if (rng.chance(0.25f)) { t.ore = static_cast<uint8_t>(rng.irange(40, 120)); t.oreMat = mat::Sulfur; }
                    else if (rng.chance(0.2f)) { t.stone = static_cast<uint8_t>(rng.irange(30, 90)); t.stoneMat = mat::Obsidian; }
                }
            break;
        }
    }
    generateUnderground(seed, rng);
    matter.init(*this);
    fullRedraw = true;
}

// Sinh lòng đất bằng nhiễu 3D trên mặt cầu (liền mạch qua kinh tuyến 180°):
//   * hang động  = mặt đẳng trị của nhiễu "gờ" → mạng đường hầm ngoằn ngoèo + vài hang lớn
//   * hồ ngầm    = hang ở vùng thấp / dưới biển nông
//   * túi magma  = hang gần núi lửa
//   * mạch quặng = vùng nhiễu cao, dày đặc dưới đồi núi; loại quặng theo "tỉnh khoáng sản"
void World::generateUnderground(uint32_t seed, Rng& rng) {
    under_.assign(w_ * h_, UnderTile{});
    ValueNoise cN(seed * 101 + 7), chN(seed * 211 + 3), vN(seed * 307 + 1), pN(seed * 401 + 9);
    const float R = w_ / (48.0f * 6.2831853f);
    caveTiles = veinTiles = 0;
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            V3 p = spherePoint(static_cast<float>(x), static_cast<float>(y), w_, h_);
            const Tile& t = at(x, y);
            UnderTile& u = under_[y * w_ + x];
            float tunnel = std::fabs(cN.fbm3(p.x * R * 3.2f + 11, p.y * R * 3.2f, p.z * R * 3.2f - 5, 3) - 0.5f);
            float chamber = chN.fbm3(p.x * R * 2.0f, p.y * R * 2.0f + 21, p.z * R * 2.0f, 3);
            bool open = tunnel < 0.022f || chamber > 0.70f;
            if (t.height < 0.30f) open = false;            // dưới biển sâu: đá đặc
            if (open) {
                if (t.flags & kTileVolcanic || t.terrain == Terrain::Lava) u.kind = Under::Magma;
                else if (t.height < 0.42f || (chamber > 0.76f && t.height < 0.5f)) u.kind = Under::Water;
                else { u.kind = Under::Cave; caveTiles++; u.fungus = static_cast<uint8_t>(rng.irange(0, 3) == 0 ? rng.irange(20, 120) : 0); }
            }
            // Mạch quặng ngầm: dày dưới đồi núi, thưa dưới đồng bằng
            float v = vN.fbm3(p.x * R * 5 + 3, p.y * R * 5, p.z * R * 5 + 8, 3);
            bool highland = t.terrain == Terrain::Hill || t.terrain == Terrain::Mountain || t.terrain == Terrain::Snow;
            if (u.kind != Under::Water && u.kind != Under::Magma && v > (highland ? 0.60f : 0.70f)) {
                float prov = pN.fbm3(p.x * R * 1.5f, p.y * R * 1.5f, p.z * R * 1.5f, 2);
                MatId ore = prov < 0.40f ? mat::CopperOre : prov < 0.50f ? mat::TinOre : prov < 0.62f ? mat::IronOre
                          : prov < 0.66f ? mat::Gold : prov < 0.72f ? mat::Sulfur : mat::IronOre;
                if (v > 0.75f && !materials.exoticOres.empty()) ore = materials.exoticOres[static_cast<size_t>(prov * 97) % materials.exoticOres.size()];
                u.oreMat = static_cast<uint8_t>(ore);
                u.oreAmt = static_cast<uint8_t>(std::min(250, 60 + static_cast<int>((v - 0.6f) * 900) + rng.irange(0, 40)));
                veinTiles++;
            }
        }
    // Cửa hang: nơi hang động chạm lên mặt đất đồi núi / rừng
    for (int y = 1; y < h_ - 1; ++y)
        for (int x = 0; x < w_; ++x) {
            Tile& t = at(x, y);
            if (under(x, y).kind != Under::Cave || !terrainInfo(t.terrain).walkable) continue;
            float chance = (t.terrain == Terrain::Hill || t.terrain == Terrain::Mountain) ? 0.06f : 0.012f;
            if (rng.chance(chance)) t.flags |= kTileCaveEntrance;
        }
    underVersion++;
}

int World::digMine(int x, int y, Rng& rng, MatId& oreOut) {
    if (!inBounds(x, y)) return 0;
    oreOut = kNoMat;
    UnderTile& u = under(x, y);
    bool changed = false;
    if (u.kind == Under::Rock) { u.kind = Under::Tunnel; changed = true; }
    // Đào theo mạch: tìm ô ngầm có quặng gần nhất (bán kính 4) nối được với đường hầm hiện tại
    int bx = 0, by = 0, bd = 1 << 30;
    for (int dy = -4; dy <= 4; ++dy)
        for (int dx = -4; dx <= 4; ++dx) {
            if (!inBounds(x + dx, y + dy)) continue;
            const UnderTile& v = under(x + dx, y + dy);
            if (v.oreAmt == 0 || v.oreMat == 0xFF || v.kind == Under::Water || v.kind == Under::Magma) continue;
            int d = dx * dx + dy * dy;
            if (d < bd) { bd = d; bx = dx; by = dy; }
        }
    int got = 0;
    if (bd < (1 << 30)) {
        // tiến một ô về phía mạch quặng (hầm mỏ mọc dần theo mạch)
        int sx = bx == 0 ? 0 : (bx > 0 ? 1 : -1), sy = by == 0 ? 0 : (by > 0 ? 1 : -1);
        int cx = x, cy = y;
        // đi theo các ô hầm đã có cho tới mặt đào
        for (int step = 0; step < 6; ++step) {
            int nx = cx + sx, ny = cy + sy;
            if (!inBounds(nx, ny) || (nx - x) * (nx - x) + (ny - y) * (ny - y) > bd) break;
            cx = nx; cy = ny;
            UnderTile& n = under(cx, cy);
            if (n.kind == Under::Rock) { n.kind = Under::Tunnel; changed = true; break; }
        }
        UnderTile& vein = under(x + bx, y + by);
        if ((cx == x + bx && cy == y + by) || bd <= 2) {
            got = std::min<int>(vein.oreAmt, 3 + rng.irange(0, 3));
            vein.oreAmt = static_cast<uint8_t>(vein.oreAmt - got);
            oreOut = vein.oreMat;
            if (vein.kind == Under::Rock) vein.kind = Under::Tunnel;
            changed = true;
        }
    }
    if (u.oreAmt > 0 && u.oreMat != 0xFF && got == 0) {
        got = std::min<int>(u.oreAmt, 3);
        u.oreAmt = static_cast<uint8_t>(u.oreAmt - got);
        oreOut = u.oreMat;
        changed = true;
    }
    if (changed) underVersion++;
    return got;
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
    if (!inBounds(x, y)) return 10;
    return matter.temp(x, y);
}

bool World::raining(int x, int y) const {
    return weather_.rainTicks > 0 &&
           dist2({float(x), float(y)}, {weather_.rainX, weather_.rainY}) < weather_.rainRadius * weather_.rainRadius;
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
    x = wrapX(x);
    items_.push_back(GroundItem{x, y, k, amount, 0, techHint});
    at(x, y).itemFlag = 1;
}

void World::markDirty(int x, int y) {
    if (y < 0 || y >= h_) return;
    int i = y * w_ + wrapX(x);
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
    matter.ignite(x, y); // lửa giờ là phản ứng cháy thật: cần nhiên liệu + oxi + nhiệt
}

void World::update(long tick, Rng& rng) {
    // ---- Mùa: chu kỳ hình sin theo năm ----
    weather_.seasonTemp = std::sin(tick * 6.2831853f / kTicksPerYear);

    // ---- Mưa: một vùng mưa trôi qua bản đồ ----
    if (weather_.rainTicks > 0) {
        weather_.rainTicks--;
        weather_.rainX += weather_.rainVX;
        weather_.rainY += weather_.rainVY;
    } else if (weather_.droughtTicks <= 0 && rng.chance(0.001f + matter.humidity * 0.006f)) {
        // Chu trình nước: độ ẩm khí quyển (do bốc hơi) càng cao càng dễ mưa
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
        if (raining(x, y)) fert *= 2.0f;
        // Chu trình carbon: cây quang hợp hấp thụ CO₂ khí quyển; CO₂ cao → cây mọc nhanh hơn chút
        fert *= 0.8f + std::min(0.6f, matter.atmosCO2 / 1400.0f);
        if (t.fire) continue;
        if (t.food < 200 && rng.chance(fert * 0.6f)) t.food = static_cast<uint8_t>(std::min(255, t.food + 12));
        if (t.terrain == Terrain::Farmland && t.food < 250) t.food = static_cast<uint8_t>(std::min(255, t.food + 20));
        if ((t.terrain == Terrain::Forest || t.terrain == Terrain::Jungle) && t.wood < 230 && rng.chance(0.5f)) {
            t.wood += 10;
            matter.atmosCO2 = std::max(150.0f, matter.atmosCO2 - 0.002f);
        }
        // Rừng lan sang đồng cỏ bên cạnh rất chậm; tro tàn hồi phục thành cỏ
        if (t.terrain == Terrain::Ash && rng.chance(0.05f)) setTerrain(x, y, Terrain::Grass);
        if (t.terrain == Terrain::Grass && t.building == Building::None && t.owner < 0 && rng.chance(0.004f * fert)) {
            int nx = x + rng.irange(-1, 1), ny = y + rng.irange(-1, 1);
            if (inBounds(nx, ny) && at(nx, ny).terrain == Terrain::Forest) setTerrain(x, y, Terrain::Forest);
        }
        // Rừng bị chặt trụi trở thành đồng cỏ
        if (t.terrain == Terrain::Forest && t.wood < 5) setTerrain(x, y, Terrain::Grass);
    }

    // ---- Hệ sinh thái: tảo/sinh vật phù du dưới nước (thức ăn của cá), nấm & côn trùng trong hang ----
    for (int i = 0; i < samples / 3; ++i) {
        int x = rng.irange(0, w_ - 1), y = rng.irange(0, h_ - 1);
        Tile& t = at(x, y);
        if ((t.terrain == Terrain::ShallowWater || t.terrain == Terrain::DeepWater) && t.food < 140 && rng.chance(t.terrain == Terrain::ShallowWater ? 0.5f : 0.25f))
            t.food = static_cast<uint8_t>(std::min(255, t.food + 10));
        UnderTile& u = under(x, y);
        if (u.kind == Under::Cave && u.fungus < 160 && rng.chance(0.4f)) u.fungus = static_cast<uint8_t>(std::min(255, u.fungus + 12));
    }

    // ---- Sét: nhiệt độ cục bộ ~3000°C → cháy rừng, nung chảy cát thành thuỷ tinh ----
    if (weather_.rainTicks > 0 && rng.chance(0.01f)) {
        int x = static_cast<int>(weather_.rainX + rng.range(-weather_.rainRadius, weather_.rainRadius));
        int y = static_cast<int>(weather_.rainY + rng.range(-weather_.rainRadius, weather_.rainRadius));
        if (inBounds(x, y)) matter.setTemp(x, y, 3000);
    }
    // Hạn hán + nóng → cháy rừng tự phát
    if (weather_.droughtTicks > 0 && rng.chance(0.02f)) ignite(rng.irange(0, w_ - 1), rng.irange(0, h_ - 1));

    // ---- Vật lý & hoá học: nhiệt, lửa, chất lỏng, khí, phản ứng, chu trình vật chất ----
    matter.update(*this, tick, rng);

    // ---- Vật phẩm mục rữa ----
    if (tick % 10 == 0) {
        for (auto& it : items_) {
            it.age += 10;
            if (it.kind == ItemKind::Corpse && it.age > 400) {
                // Phân huỷ: thịt → đất mùn (đất màu mỡ hơn) + khí mê-tan — chu trình dinh dưỡng
                it.kind = ItemKind::Bones; it.amount = 1;
                Tile& t = at(it.x, it.y);
                t.food = static_cast<uint8_t>(std::min(255, t.food + 40));
                matter.addGas(it.x, it.y, mat::Methane, 4);
                ReactionEvent ev;
                ev.tick = tick; ev.x = it.x; ev.y = it.y; ev.kind = ReactionKind::Decay;
                ev.in[0] = mat::Flesh; ev.out[0] = mat::Dirt; ev.out[1] = mat::Methane; ev.temp = matter.temp(it.x, it.y);
                matter.emit(ev);
            }
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
