// =============================================================================
// MatterGrid.cpp — Nhiệt động học, lửa, chất lỏng, khí và chuyển pha địa hình.
// Phần phản ứng hoá học nằm ở Chemistry.cpp.
// =============================================================================
#include "MatterGrid.h"
#include "../world/World.h"
#include <cmath>
#include <cstdio>

namespace wb {

const char* reactionName(ReactionKind k) {
    static const char* n[] = {"Cháy", "Nhiệt phân (thiếu oxi)", "Nóng chảy", "Đông đặc", "Sôi / bay hơi", "Ngưng tụ",
                              "Luyện kim (khử quặng)", "Tạo hợp kim", "Nung", "Hydrat hoá", "Trung hoà axit–bazơ",
                              "Axit ăn mòn kim loại", "Oxi hoá (gỉ)", "Phân huỷ", "Kết tủa", "Tôi nguội (dung nham gặp nước)",
                              "Trộn hỗn hợp", "Nổ", "Đóng băng", "Tan băng", "Thuỷ tinh hoá"};
    return n[static_cast<int>(k)];
}

std::string ReactionEvent::describe(const MaterialRegistry& reg) const {
    std::string s;
    for (MatId m : in) {
        if (m == kNoMat) continue;
        if (!s.empty()) s += " + ";
        s += reg[m].name;
    }
    if (kind == ReactionKind::Combustion || kind == ReactionKind::Explosion) s += " + O₂";
    s += " → ";
    bool first = true;
    for (MatId m : out) {
        if (m == kNoMat) continue;
        if (!first) s += " + ";
        s += reg[m].name;
        first = false;
    }
    if (first) s += "(năng lượng)";
    char buf[48];
    std::snprintf(buf, sizeof(buf), " (%.0f°C%s)", temp, lowOxygen ? ", thiếu oxi" : "");
    return s + buf;
}

namespace {
bool grassy(Terrain t) {
    return t == Terrain::Grass || t == Terrain::Savanna || t == Terrain::Jungle || t == Terrain::Forest ||
           t == Terrain::Farmland || t == Terrain::Tundra;
}
constexpr float kHeightScale = 60.0f;  // độ cao địa hình → "mét" cho dòng chảy
constexpr float kLiquidScale = 0.02f;  // 1 đơn vị chất lỏng nâng mặt nước bao nhiêu
} // namespace

void MatterGrid::init(World& w) {
    w_ = w.width(); h_ = w.height();
    cells_.assign(w_ * h_, MatterCell{});
    temp_.resize(w_ * h_);
    temp2_.resize(w_ * h_);
    activeFlag_.assign(w_ * h_, 0);
    active_.clear(); nextActive_.clear(); burning_.clear();
    recent_.clear(); tick_.clear(); explosions_.clear();
    counts.fill(0);
    atmosCO2 = 280; humidity = 0.5f;
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) temp_[idx(x, y)] = ambient(w, x, y);
}

float MatterGrid::ambient(const World& w, int x, int y) const {
    const Tile& t = w.at(x, y);
    if (t.terrain == Terrain::Lava && (t.flags & kTileVolcanic)) return 1150;
    float base = 14 + terrainInfo(t.terrain).temperature * 24;   // tuyết ≈ -10°C, sa mạc ≈ 36°C
    base += w.weather().seasonTemp * 10;                          // mùa hè / mùa đông
    if (t.height > 0.7f) base -= (t.height - 0.7f) * 60;          // càng cao càng lạnh
    if (w.raining(x, y)) base -= 4;
    base += (atmosCO2 - 280) * 0.01f;                             // hiệu ứng nhà kính (rất nhẹ)
    return base;
}

void MatterGrid::activate(int i) {
    if (!activeFlag_[i]) { activeFlag_[i] = 1; nextActive_.push_back(i); }
}

void MatterGrid::emit(const ReactionEvent& e) {
    tick_.push_back(e);
    recent_.push_back(e);
    if (recent_.size() > 300) recent_.pop_front();
    counts[static_cast<int>(e.kind)]++;
}

int MatterGrid::amountOf(int x, int y, MatId m) const {
    int n = 0;
    for (const auto& s : cells_[idx(x, y)].s) if (s.mat == m) n += s.amount;
    return n;
}

bool MatterGrid::wet(int x, int y) const {
    const MatterCell& c = cells_[idx(x, y)];
    float T = temp_[idx(x, y)];
    for (const auto& s : c.s)
        if ((s.mat == mat::Water || s.mat == mat::SaltWater || s.mat == mat::Mud) && s.amount > 3 && T > 0 && T < 100) return true;
    return false;
}

float MatterGrid::pressure(int x, int y) const {
    const MatterCell& c = cells_[idx(x, y)];
    if (c.gas.empty()) return 1.0f;
    return 1.0f + c.gas.amount * (temp_[idx(x, y)] + 273.0f) / 293.0f / 100.0f; // định luật khí lý tưởng (tương đối)
}

void MatterGrid::addHeat(int x, int y, float d) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    int i = idx(x, y);
    temp_[i] += d;
    activate(i);
}

void MatterGrid::setTemp(int x, int y, float c) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    int i = idx(x, y);
    temp_[i] = c;
    activate(i);
}

int MatterGrid::addMatter(int x, int y, MatId m, int amount, float tempC) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_ || amount <= 0 || m == kNoMat) return 0;
    int i = idx(x, y);
    MatterCell& c = cells_[i];
    MatterSlot* slot = nullptr;
    int existing = 0;
    for (auto& s : c.s) existing += s.amount;
    for (auto& s : c.s) if (s.mat == m && s.amount > 0) { slot = &s; break; }
    if (!slot) for (auto& s : c.s) if (s.empty()) { slot = &s; slot->mat = m; slot->amount = 0; slot->molten = false; break; }
    if (!slot) return 0;
    int add = std::min(amount, 60000 - static_cast<int>(slot->amount));
    slot->amount = static_cast<uint16_t>(slot->amount + add);
    if (tempC > -999) temp_[i] = (temp_[i] * (existing + 20) + tempC * add) / (existing + 20 + add); // trộn nhiệt
    activate(i);
    return add;
}

int MatterGrid::takeMatter(int x, int y, MatId m, int amount) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return 0;
    MatterCell& c = cells_[idx(x, y)];
    int got = 0;
    for (auto& s : c.s)
        if (s.mat == m && s.amount > 0) {
            int t = std::min<int>(amount - got, s.amount);
            s.amount = static_cast<uint16_t>(s.amount - t);
            got += t;
            if (s.amount == 0) s = MatterSlot{};
        }
    return got;
}

void MatterGrid::addGas(int x, int y, MatId m, int amount) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_ || amount <= 0) return;
    int i = idx(x, y);
    MatterSlot& g = cells_[i].gas;
    if (g.empty() || g.mat == m) { g.mat = m; g.amount = static_cast<uint16_t>(std::min(60000, g.amount + amount)); }
    else if (amount > g.amount) { g.mat = m; g.amount = static_cast<uint16_t>(amount); } // khí mạnh hơn đẩy khí cũ đi
    activate(i);
}

void MatterGrid::setHeatSource(int x, int y, float targetC, int owner) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    MatterCell& c = cells_[idx(x, y)];
    c.heatTarget = targetC;
    c.heatOwner = static_cast<int16_t>(owner);
    activate(idx(x, y));
}

void MatterGrid::clearHeatSource(int x, int y) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    cells_[idx(x, y)].heatTarget = 0;
}

void MatterGrid::ignite(int x, int y) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    temp_[idx(x, y)] = std::max(temp_[idx(x, y)], 700.0f);
    activate(idx(x, y));
}

void MatterGrid::explode(int x, int y, float energy, long tick) {
    explosions_.push_back({x, y, energy, tick});
    int r = std::max(1, static_cast<int>(std::sqrt(energy) / 6));
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w_ || ny >= h_ || dx * dx + dy * dy > r * r) continue;
            float falloff = 1.0f - std::sqrt(static_cast<float>(dx * dx + dy * dy)) / (r + 1);
            temp_[idx(nx, ny)] += 900 * falloff;
            addGas(nx, ny, mat::CO2, static_cast<int>(60 * falloff));
            activate(idx(nx, ny));
        }
    ReactionEvent e;
    e.tick = tick; e.x = x; e.y = y; e.kind = ReactionKind::Explosion; e.temp = temp_[idx(x, y)];
    e.out[0] = mat::CO2;
    emit(e);
}

// ---------------------------------------------------------------------------
// Nhiệt: khuếch tán + kéo về khí hậu + nguồn nhiệt + bức xạ của đám cháy
// ---------------------------------------------------------------------------
void MatterGrid::thermalStep(World& w) {
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            int i = idx(x, y);
            float T = temp_[i];
            // Lò/bếp có vách che chắn: hàng xóm chỉ "cảm nhận" tối đa ~+80°C (ấm, không bén lửa)
            auto nb = [&](int j) {
                float v = temp_[j];
                return cells_[j].heatTarget > 0 ? std::min(v, 90.0f) : v;
            };
            float sum = 0; int n = 0;
            if (x > 0) { sum += nb(i - 1); n++; }
            if (x < w_ - 1) { sum += nb(i + 1); n++; }
            if (y > 0) { sum += nb(i - w_); n++; }
            if (y < h_ - 1) { sum += nb(i + w_); n++; }
            float avg = sum / n;
            const Tile& t = w.at(x, y);
            MatId ground = groundMaterial(t.terrain);
            // Hệ số khuếch tán theo độ dẫn nhiệt của nền (kim loại/đá dẫn tốt, gỗ/không khí kém)
            float k = 0.05f + 0.15f * w.materials[ground].conductivity;
            float amb = ambient(w, x, y);
            // Khối nước lớn có nhiệt dung cao → giữ nhiệt độ ổn định (hồ nước điều hoà khí hậu)
            float relax = (ground == mat::Water) ? 0.04f : 0.02f;
            float nt = T + k * (avg - T);
            nt += (amb - nt) * relax;
            const MatterCell& c = cells_[i];
            // Lò/bếp: thành lò cách nhiệt giữ nhiệt bên trong, nhiên liệu bù phần hao hụt
            // → ô được đưa nhanh tới nhiệt độ mục tiêu và giữ ở đó (phản ứng toả nhiệt có thể vượt lên)
            if (c.heatTarget > 0) nt = T < c.heatTarget ? T + (c.heatTarget - T) * 0.6f : std::max(nt, c.heatTarget);
            temp2_[i] = nt;
        }
    temp_.swap(temp2_);
    // Bức xạ nhiệt từ đám cháy (theo gió) + kiểm tra bắt lửa xung quanh
    for (int i : burning_) {
        int x = i % w_, y = i / w_;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dy) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= w_ || ny >= h_) continue;
                float windBoost = 1.0f + 3.0f * std::max(0.0f, dx * wind.x + dy * wind.y) / 0.1f * 0.1f;
                float& tn = temp_[idx(nx, ny)];
                if (tn < 700) tn += 4.0f * windBoost * (1.2f - humidity * 0.6f); // không khí ẩm hấp thụ bớt bức xạ
            }
    }
}

// ---------------------------------------------------------------------------
// Lửa trên địa hình: nhiên liệu = cây gỗ / cỏ / công trình gỗ. Cần oxi, bị nước dập.
// ---------------------------------------------------------------------------
void MatterGrid::fireStep(World& w, Rng& rng, long tick) {
    // 1) Bắt lửa: ô nóng vượt điểm bắt lửa của nhiên liệu
    if (tick % 2 == 0) {
        auto tryIgnite = [&](int x, int y) {
            int i = idx(x, y);
            Tile& t = w.at(x, y);
            if (t.fire || w.isWater(x, y)) return;
            bool woodFuel = t.wood > 10, grassFuel = grassy(t.terrain);
            bool buildingFuel = t.building != Building::None && buildingInfo(t.building).flammable;
            if (!woodFuel && !grassFuel && !buildingFuel) return;
            float ign = woodFuel || buildingFuel ? w.materials[mat::Wood].ignitionPoint : w.materials[mat::Fiber].ignitionPoint;
            if (temp_[i] < ign || wet(x, y) || w.raining(x, y) || cells_[i].heatTarget > 0) return;
            if (!rng.chance(0.15f * (1.0f - humidity * 0.7f))) return;   // thảm thực vật ẩm khó bắt lửa
            t.fire = woodFuel ? 60 : 20;
            burning_.push_back(i);
            w.markDirty(x, y);
        };
        // Chỉ xét quanh các đám cháy + ô hoạt động (rẻ)
        size_t nb = burning_.size();
        for (size_t k = 0; k < nb; ++k) {
            int x = burning_[k] % w_, y = burning_[k] / w_;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < w_ && ny < h_) tryIgnite(nx, ny);
                }
        }
        for (int i : active_) tryIgnite(i % w_, i / w_);
    }
    // 2) Cháy: tiêu thụ nhiên liệu, toả nhiệt, sinh tro + CO₂ (hoặc than nếu thiếu oxi)
    std::vector<int> next;
    next.reserve(burning_.size());
    for (int i : burning_) {
        int x = i % w_, y = i / w_;
        Tile& t = w.at(x, y);
        if (!t.fire) continue;
        if (wet(x, y) || (w.raining(x, y) && rng.chance(0.2f))) {
            // Nước dập lửa: hấp thụ nhiệt, bốc hơi
            t.fire = 0; temp_[i] = std::min(temp_[i], 90.0f);
            addGas(x, y, mat::Water, 6);
            w.markDirty(x, y);
            continue;
        }
        // Thiếu oxi khi bị bao quanh bởi lửa dày đặc hoặc khí CO₂ đậm đặc
        int burnNb = 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx, ny = y + dy;
                if ((dx || dy) && nx >= 0 && ny >= 0 && nx < w_ && ny < h_ && w.at(nx, ny).fire) burnNb++;
            }
        const MatterCell& c = cells_[i];
        bool lowO2 = burnNb >= 7 || (c.gas.mat == mat::CO2 && c.gas.amount > 150);
        bool wood = t.wood > 0;
        // Lửa gỗ ngoài trời ~600–1100°C, lửa cỏ ~500°C (không phải lò kín nên không vượt quá)
        temp_[i] = std::clamp(temp_[i], wood ? 850.0f : 500.0f, wood ? 1050.0f : 650.0f);
        if (wood) t.wood = static_cast<uint8_t>(std::max(0, t.wood - 2));
        t.food = 0;
        if (t.building != Building::None && buildingInfo(t.building).flammable) {
            t.buildingHp = static_cast<uint8_t>(std::max(0, t.buildingHp - 3));
            if (t.buildingHp == 0) { w.setBuilding(x, y, Building::None); addMatter(x, y, mat::Ash, 5); }
        }
        if (tick % 4 == 0) addGas(x, y, mat::CO2, 3);
        atmosCO2 += 0.0003f;
        if (lowO2 && wood && rng.chance(0.06f)) {
            addMatter(x, y, mat::Charcoal, 3);
            if (rng.chance(0.15f)) {
                ReactionEvent e; e.tick = tick; e.x = x; e.y = y; e.kind = ReactionKind::Pyrolysis;
                e.in[0] = mat::Wood; e.out[0] = mat::Charcoal; e.out[1] = mat::CO2; e.temp = temp_[i]; e.lowOxygen = true;
                emit(e);
            }
        } else if (rng.chance(0.01f)) {
            ReactionEvent e; e.tick = tick; e.x = x; e.y = y; e.kind = ReactionKind::Combustion;
            e.in[0] = wood ? mat::Wood : mat::Fiber; e.out[0] = mat::Ash; e.out[1] = mat::CO2; e.temp = temp_[i];
            emit(e);
        }
        if (!wood) t.fire = static_cast<uint8_t>(t.fire - 1);
        if (t.fire == 0 || (!wood && !grassy(t.terrain) && t.building == Building::None)) {
            t.fire = 0;
            if (grassy(t.terrain)) w.setTerrain(x, y, Terrain::Ash);
            w.markDirty(x, y);
            continue;
        }
        next.push_back(i);
    }
    burning_.swap(next);
}

// ---------------------------------------------------------------------------
// Chất lỏng chảy theo độ cao (trọng lực trong góc nhìn top-down), bột trượt dốc
// ---------------------------------------------------------------------------
void MatterGrid::flowLiquids(World& w, int i) {
    MatterCell& c = cells_[i];
    if (c.heatTarget > 0) return;   // trong lò có nồi nung: kim loại nóng chảy không tràn ra ngoài
    int x = i % w_, y = i / w_;
    float T = temp_[i];
    float liquid = 0;
    for (auto& s : c.s) if (!s.empty() && w.materials[s.mat].phaseAt(T) == Phase::Liquid) liquid += s.amount;
    if (liquid <= 0) return;
    float surf = w.at(x, y).height * kHeightScale + liquid * kLiquidScale;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (auto& s : c.s) {
        if (s.empty()) continue;
        const MaterialDef& md = w.materials[s.mat];
        Phase ph = md.phaseAt(T);
        if (ph == Phase::Solid && md.powder && s.amount > 30) {
            // Bột trượt khi dốc vượt góc nghỉ
            for (int k = 0; k < 4; ++k) {
                int nx = x + dx[k], ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= w_ || ny >= h_ || w.isWater(nx, ny)) continue;
                float drop = (w.at(x, y).height - w.at(nx, ny).height) * kHeightScale + s.amount * kLiquidScale;
                if (drop > 1.2f) { int mv = s.amount / 6; s.amount = static_cast<uint16_t>(s.amount - mv); addMatter(nx, ny, s.mat, mv); }
            }
            continue;
        }
        if (ph != Phase::Liquid) continue;
        float fluidity = 1.0f - md.viscosity;
        for (int k = 0; k < 4 && s.amount > 0; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || ny < 0 || nx >= w_ || ny >= h_) continue;
            const Tile& nt = w.at(nx, ny);
            if (nt.building != Building::None && buildingInfo(nt.building).blocksMove) continue; // tường chặn nước/dung nham
            int ni = idx(nx, ny);
            if (w.isWater(nx, ny)) {
                // Chảy ra biển/hồ: nước hoà vào; chất khác (dung nham) gặp nước → tôi nguội tại chỗ
                if (s.mat == mat::Water || s.mat == mat::SaltWater || s.mat == mat::Blood || s.mat == mat::Lye) {
                    int mv = std::max(1, static_cast<int>(s.amount * 0.25f * fluidity));
                    s.amount = static_cast<uint16_t>(std::max(0, s.amount - mv));
                } else if (md.metal == false && (s.mat == mat::Stone || s.mat == mat::Basalt)) {
                    temp_[i] -= 300; // nước rút nhiệt cực mạnh
                    addGas(nx, ny, mat::Water, 10);
                }
                continue;
            }
            float nl = 0;
            for (auto& ns : cells_[ni].s) if (!ns.empty() && w.materials[ns.mat].phaseAt(temp_[ni]) == Phase::Liquid) nl += ns.amount;
            float nsurf = nt.height * kHeightScale + nl * kLiquidScale;
            float diff = surf - nsurf;
            if (diff <= 0.05f) continue;
            int mv = static_cast<int>(std::min<float>(s.amount, diff / kLiquidScale * 0.2f * fluidity));
            if (mv <= 0) continue;
            int moved = addMatter(nx, ny, s.mat, mv, T);
            s.amount = static_cast<uint16_t>(s.amount - moved);
            surf -= moved * kLiquidScale;
        }
        if (s.amount == 0) s = MatterSlot{};
    }
}

// Khí: khuếch tán theo chênh lệch áp suất, trôi theo gió, thoát dần vào khí quyển.
void MatterGrid::flowGas(World& w, int i, Rng& rng) {
    MatterSlot& g = cells_[i].gas;
    if (g.empty()) return;
    int x = i % w_, y = i / w_;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; ++k) {
        int nx = x + dx[k], ny = y + dy[k];
        if (nx < 0 || ny < 0 || nx >= w_ || ny >= h_) continue;
        MatterSlot& ng = cells_[idx(nx, ny)].gas;
        if (!ng.empty() && ng.mat != g.mat) continue;
        float bias = 1.0f + (dx[k] * wind.x + dy[k] * wind.y) * 8.0f; // gió thổi khí đi
        int mv = static_cast<int>((g.amount - ng.amount) * 0.12f * std::max(0.2f, bias));
        if (mv <= 0) continue;
        ng.mat = g.mat; ng.amount = static_cast<uint16_t>(ng.amount + mv);
        g.amount = static_cast<uint16_t>(g.amount - mv);
        activate(idx(nx, ny));
    }
    // Thoát lên cao → góp vào khí quyển toàn cầu (chu trình carbon & nước)
    int escape = std::max(1, g.amount / 25);
    if (rng.chance(0.5f)) {
        g.amount = static_cast<uint16_t>(std::max(0, g.amount - escape));
        if (g.mat == mat::CO2) atmosCO2 += escape * 0.002f;
        if (g.mat == mat::Water) humidity = std::min(1.0f, humidity + escape * 0.0004f);
    }
    // Ngưng tụ: hơi nước gặp lạnh → đọng thành nước lỏng
    const MaterialDef& gm = w.materials[g.mat];
    if (gm.boilPoint > -50 && temp_[i] < gm.boilPoint - 5 && g.amount > 4) {
        int c = g.amount / 3;
        g.amount = static_cast<uint16_t>(g.amount - c);
        addMatter(x, y, g.mat, c / 2 + 1);
        if (rng.chance(0.05f)) {
            ReactionEvent e; e.tick = curTick_; e.x = x; e.y = y; e.kind = ReactionKind::Condensing;
            e.in[0] = g.mat; e.out[0] = g.mat; e.temp = temp_[i];
            emit(e);
        }
    }
    if (g.amount == 0) g = MatterSlot{};
}

// ---------------------------------------------------------------------------
// Chuyển pha của chính địa hình: hồ đóng băng, tuyết tan, dung nham nguội, cát → thuỷ tinh
// ---------------------------------------------------------------------------
void MatterGrid::terrainPhaseStep(World& w, long tick) {
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            int i = idx(x, y);
            Tile& t = w.at(x, y);
            float T = temp_[i];
            auto ev = [&](ReactionKind k, MatId a, MatId b) {
                if ((x * 7 + y * 13 + tick) % 11 != 0) return; // giảm số sự kiện hiển thị
                ReactionEvent e; e.tick = tick; e.x = x; e.y = y; e.kind = k; e.in[0] = a; e.out[0] = b; e.temp = T;
                emit(e);
            };
            switch (t.terrain) {
            case Terrain::ShallowWater:
            case Terrain::DeepWater:
                if (T < -3) { w.setTerrain(x, y, Terrain::Ice); ev(ReactionKind::Freezing, mat::Water, mat::Water); }
                break;
            case Terrain::Ice:
                if (T > 1) { w.setTerrain(x, y, t.height < 0.36f ? Terrain::DeepWater : Terrain::ShallowWater); ev(ReactionKind::Thawing, mat::Water, mat::Water); }
                break;
            case Terrain::Snow:
                if (T > 6) { w.setTerrain(x, y, Terrain::Tundra); addMatter(x, y, mat::Water, 15, 2); ev(ReactionKind::Thawing, mat::Water, mat::Water); }
                break;
            case Terrain::Lava:
                if (!(t.flags & kTileVolcanic) && T < 900) { w.setTerrain(x, y, Terrain::Basalt); ev(ReactionKind::Solidifying, mat::Stone, mat::Basalt); }
                break;
            case Terrain::Sand:
            case Terrain::Desert:
                if (T > 1700) { w.setTerrain(x, y, Terrain::Glass); ev(ReactionKind::Vitrification, mat::Sand, mat::Glass); }
                break;
            default:
                break;
            }
            // Mùa đông lạnh + mưa → tuyết phủ
            if ((t.terrain == Terrain::Grass || t.terrain == Terrain::Tundra) && T < -6 && w.raining(x, y) && t.building == Building::None)
                w.setTerrain(x, y, Terrain::Snow);
            // Đá nóng chảy (thiên thạch, dung nham tràn) → hồ dung nham tạm thời
            if (T > 1250 && t.terrain != Terrain::Lava && t.terrain != Terrain::Glass && !w.isWater(x, y) &&
                t.terrain != Terrain::Sand && t.terrain != Terrain::Desert) {
                w.setTerrain(x, y, Terrain::Lava);
                ev(ReactionKind::Melting, groundMaterial(t.terrain), mat::Stone);
            }
            // Vật chất rắn tích tụ dày → thành địa hình (dung nham nguội tạo đất mới!)
            MatterCell& c = cells_[i];
            for (auto& s : c.s)
                if (s.mat == mat::Basalt && s.amount > 150 && w.materials[s.mat].phaseAt(T) == Phase::Solid) {
                    if (w.isWater(x, y)) { t.height = std::max(t.height, 0.45f); }
                    w.setTerrain(x, y, Terrain::Basalt);
                    s = MatterSlot{};
                }
        }
}

void MatterGrid::cellStep(World& w, int i, Rng& rng, long tick) {
    reactCell(w, *this, i % w_, i / w_, rng, tick);
    flowLiquids(w, i);
    flowGas(w, i, rng);
}

void MatterGrid::update(World& w, long tick, Rng& rng) {
    curTick_ = tick;
    tick_.clear();
    if (tick % 2 == 0) thermalStep(w);
    fireStep(w, rng, tick);
    // Ô hoạt động: những ô vừa được kích hoạt ở tick trước
    active_.swap(nextActive_);
    nextActive_.clear();
    for (int i : active_) activeFlag_[i] = 0;
    for (int i : active_) {
        cellStep(w, i, rng, tick);
        const MatterCell& c = cells_[i];
        bool hasMatter = !c.gas.empty() || c.heatTarget > 0;
        for (const auto& s : c.s) hasMatter |= !s.empty();
        int x = i % w_, y = i / w_;
        float dev = std::fabs(temp_[i] - ambient(w, x, y));
        // Giữ hoạt động nếu còn khí/lò/nhiệt độ lệch nhiều; vật chất ổn định thì "ngủ"
        if (!c.gas.empty() || c.heatTarget > 0 || dev > 40) activate(i);
        else if (hasMatter && rng.chance(0.25f)) activate(i);
    }
    // Quét chậm: đánh thức mọi ô có vật chất (phản ứng chậm như gỉ sét, phân huỷ, bay hơi)
    if (tick % 20 == 0)
        for (int i = 0; i < w_ * h_; ++i) {
            const MatterCell& c = cells_[i];
            if (!c.s[0].empty() || !c.s[1].empty() || !c.s[2].empty() || !c.gas.empty() || c.heatTarget > 0) activate(i);
        }
    if (tick % 30 == 0) terrainPhaseStep(w, tick);
    // Hiển thị lửa trên địa hình
    // Khí quyển trôi dần về cân bằng (đại dương hấp thụ CO₂, mưa rút độ ẩm)
    atmosCO2 += (280 - atmosCO2) * 0.0002f;                  // đại dương & đá hấp thụ CO₂
    if (w.weather().rainTicks > 0) humidity = std::max(0.0f, humidity - 0.0008f);
    humidity += (0.6f - humidity) * 0.002f;                   // biển bốc hơi giữ độ ẩm quanh mức cân bằng
    // Gió đổi hướng chậm
    if (tick % 300 == 0) { wind.x = rng.range(-0.1f, 0.1f); wind.y = rng.range(-0.1f, 0.1f); }
}

} // namespace wb
