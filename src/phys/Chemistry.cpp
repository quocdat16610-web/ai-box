// =============================================================================
// Chemistry.cpp — Các QUY LUẬT phản ứng tổng quát (không liệt kê từng công thức).
//
// Mỗi quy luật dựa trên THUỘC TÍNH vật liệu, nên áp dụng được cho cả vật liệu mới
// sinh ra lúc chạy (hợp kim, khoáng chất lạ):
//   1. Chuyển pha       : nóng chảy / đông đặc / sôi / ngưng tụ (theo điểm nóng chảy, điểm sôi)
//   2. Cháy             : chất có điểm bắt lửa + oxi → nhiệt + cặn + khí; thiếu oxi → nhiệt phân
//   3. Nung             : nhiệt ≥ fireTemp → biến đổi (đất sét → gạch, đá vôi → vôi sống + CO₂)
//   4. Khử quặng        : quặng + chất khử (than) ≥ smeltTemp → kim loại + CO₂
//   5. Hợp kim          : hai kim loại cùng nóng chảy → vật liệu MỚI
//   6. Hydrat hoá       : chất hút nước + nước → chất mới + nhiệt (vôi sống, tro → kiềm, SO₂ → axit)
//   7. Axit – bazơ      : trung hoà → nước muối + nhiệt
//   8. Axit – kim loại  : ăn mòn → muối + khí hiđrô (cháy được!)
//   9. Oxi hoá chậm     : kim loại hoạt động + ẩm → gỉ
//  10. Tôi nguội        : dung nham gặp nước → đá vỏ chai + hơi nước
//  11. Trộn hỗn hợp     : than + lưu huỳnh + diêm tiêu → thuốc súng (chất nổ)
//  12. Phân huỷ         : chất hữu cơ → đất mùn + mê-tan (làm đất màu mỡ)
//  13. Bay hơi/kết tủa  : nước muối cạn → muối kết tinh
// =============================================================================
#include "MatterGrid.h"
#include "../world/World.h"

namespace wb {

namespace {
struct Ctx {
    World& w;
    MatterGrid& g;
    MatterCell& c;
    float& T;
    int x, y;
    Rng& rng;
    long tick;
    MaterialRegistry& R;
};

// Hạn chế số sự kiện mỗi ô để log dễ đọc (phản ứng vẫn diễn ra, chỉ không ghi hết)
bool shouldLog(Ctx& k, float p = 0.08f) { return k.rng.chance(p); }

void log(Ctx& k, ReactionKind kind, MatId a, MatId b, MatId o1, MatId o2, MatId c = kNoMat, bool lowO2 = false) {
    ReactionEvent e;
    e.tick = k.tick; e.x = k.x; e.y = k.y; e.kind = kind;
    e.in[0] = a; e.in[1] = b; e.in[2] = c; e.out[0] = o1; e.out[1] = o2; e.temp = k.T; e.lowOxygen = lowO2;
    k.g.emit(e);
}

MatterSlot* find(Ctx& k, MatId m) {
    for (auto& s : k.c.s) if (s.mat == m && s.amount > 0) return &s;
    return nullptr;
}

bool hasLiquidWater(Ctx& k) {
    for (auto& s : k.c.s)
        if ((s.mat == mat::Water || s.mat == mat::SaltWater) && s.amount > 0 && k.T > 0 && k.T < 100) return true;
    return false;
}

bool waterNearby(Ctx& k) {
    if (hasLiquidWater(k) || k.w.raining(k.x, k.y)) return true;
    // Nước vừa đổ vào chất nóng (đang hoá hơi) vẫn tiếp xúc trực tiếp → tính là có nước
    for (auto& s : k.c.s) if ((s.mat == mat::Water || s.mat == mat::SaltWater) && s.amount > 0) return true;
    if (k.c.gas.mat == mat::Water && k.c.gas.amount > 5) return true;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if ((dx || dy) && k.w.isWater(k.x + dx, k.y + dy)) return true;
    return false;
}

void consume(MatterSlot& s, int n) {
    s.amount = static_cast<uint16_t>(std::max(0, s.amount - n));
    if (s.amount == 0) s = MatterSlot{};
}

// Thêm sản phẩm vào ô (nếu đầy ngăn thì bỏ ra ô bên cạnh)
void produce(Ctx& k, MatId m, int n) {
    if (m == kNoMat || n <= 0) return;
    if (k.g.addMatter(k.x, k.y, m, n) == 0) {
        for (int d = 0; d < 4; ++d) {
            static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            if (k.g.addMatter(k.x + dx[d], k.y + dy[d], m, n) > 0) return;
        }
    }
}
} // namespace

void reactCell(World& w, MatterGrid& g, int x, int y, Rng& rng, long tick) {
    MatterCell& cell = g.cell(x, y);
    float& T = g.temps()[g.idx(x, y)];
    Ctx k{w, g, cell, T, x, y, rng, tick, w.materials};
    MaterialRegistry& R = w.materials;
    bool submerged = false;
    for (auto& s : cell.s) if ((s.mat == mat::Water || s.mat == mat::SaltWater) && s.amount > 40 && T < 100 && T > 0) submerged = true;
    // Lò kín (ô có nguồn nhiệt) = môi trường khử: than không cháy hết mà khử quặng, gỗ hoá than
    bool lowO2 = submerged || cell.heatTarget > 0 || (cell.gas.mat == mat::CO2 && cell.gas.amount > 120) ||
                 (cell.gas.mat == mat::Water && cell.gas.amount > 80);

    // ---------------- 1. Chuyển pha ----------------
    for (auto& s : cell.s) {
        if (s.empty()) continue;
        const MaterialDef& d = R[s.mat];
        Phase ph = d.phaseAt(T);
        if (ph == Phase::Liquid && !s.molten && d.meltPoint > 50) {
            s.molten = true;
            if (shouldLog(k, 0.5f)) log(k, ReactionKind::Melting, s.mat, kNoMat, s.mat, kNoMat);
        } else if (ph == Phase::Solid && s.molten) {
            s.molten = false;
            MatId from = s.mat;
            MatId to = d.solidifiesTo;
            // Dung nham (đá nóng chảy) nguội nhanh trong nước → đá vỏ chai; nguội chậm → bazan
            if ((s.mat == mat::Stone || s.mat == mat::Basalt) && waterNearby(k)) to = mat::Obsidian;
            if (to != kNoMat) {
                s.mat = to;
                log(k, to == mat::Glass ? ReactionKind::Vitrification : (to == mat::Obsidian ? ReactionKind::Quench : ReactionKind::Solidifying),
                    from, kNoMat, to, kNoMat);
            } else if (shouldLog(k, 0.3f)) {
                log(k, ReactionKind::Solidifying, from, kNoMat, from, kNoMat);
            }
        } else if (ph == Phase::Gas) {
            // Sôi: chuyển dần sang ngăn khí, hấp thụ nhiệt hoá hơi
            int n = std::min<int>(s.amount, 6);
            MatId from = s.mat;
            if (d.dissolvedSalt != kNoMat) {
                // Nước muối sôi cạn → muối KẾT TỦA ở lại
                produce(k, d.dissolvedSalt, std::max(1, n / 4));
                if (shouldLog(k, 0.3f)) log(k, ReactionKind::Precipitation, from, kNoMat, d.dissolvedSalt, mat::Water);
                g.addGas(x, y, mat::Water, n);
            } else {
                g.addGas(x, y, s.mat, n);
                if (shouldLog(k, 0.15f)) log(k, ReactionKind::Boiling, from, kNoMat, from, kNoMat);
            }
            consume(s, n);
            T -= n * 2.0f;
        } else if ((s.mat == mat::Water || s.mat == mat::SaltWater || s.mat == mat::Blood) && ph == Phase::Liquid && T > 8 &&
                   rng.chance(0.04f + std::max(0.0f, T - 20) * 0.004f)) {
            // Bay hơi tự nhiên (vũng nước khô dần) → độ ẩm khí quyển; nước muối cạn → muối
            consume(s, 1);
            g.humidity = std::min(1.0f, g.humidity + 0.0003f);
            if (d.dissolvedSalt != kNoMat && rng.chance(0.3f)) {
                produce(k, d.dissolvedSalt, 1);
                if (shouldLog(k, 0.05f)) log(k, ReactionKind::Precipitation, mat::SaltWater, kNoMat, mat::Salt, mat::Water);
            }
        }
    }

    // ---------------- 2. Cháy / nhiệt phân / nổ ----------------
    for (auto& s : cell.s) {
        if (s.empty()) continue;
        const MaterialDef& d = R[s.mat];
        if (d.ignitionPoint <= 0 || T < d.ignitionPoint || submerged) continue;
        MatId fuel = s.mat;
        if (d.explosive) {
            // Nổ: toàn bộ năng lượng giải phóng tức thì → áp suất cực lớn
            float energy = d.combustionHeat * s.amount * 30.0f + 200;
            consume(s, s.amount);
            g.explode(x, y, energy, tick);
            log(k, ReactionKind::Explosion, fuel, kNoMat, d.burnGas, d.burnResidue);
            continue;
        }
        int burn = std::min<int>(s.amount, 3);
        if (lowO2 && d.pyrolysisTo != kNoMat) {
            consume(s, burn);
            produce(k, d.pyrolysisTo, std::max(1, burn / 2));
            g.addGas(x, y, mat::CO2, 1);
            if (shouldLog(k, 0.2f)) log(k, ReactionKind::Pyrolysis, fuel, kNoMat, d.pyrolysisTo, mat::CO2, kNoMat, true);
            continue;
        }
        if (lowO2) continue; // không có oxi thì không cháy
        consume(s, burn);
        T += d.combustionHeat * burn * 4.0f;                // toả nhiệt
        T = std::min(T, 900 + d.combustionHeat * 25.0f);    // than cháy nóng hơn gỗ
        produce(k, d.burnResidue, std::max(burn / 3, d.burnResidue != kNoMat ? 1 : 0));
        if (d.burnGas != kNoMat) g.addGas(x, y, d.burnGas, burn * 2);
        g.atmosCO2 += burn * 0.004f;
        if (shouldLog(k, 0.1f)) log(k, ReactionKind::Combustion, fuel, kNoMat, d.burnResidue, d.burnGas);
    }
    // Khí cháy được (mê-tan, hiđrô) gặp nhiệt độ cao → nổ
    if (!cell.gas.empty()) {
        const MaterialDef& gd = R[cell.gas.mat];
        if (gd.ignitionPoint > 0 && T >= gd.ignitionPoint && cell.gas.amount > 10) {
            float energy = gd.combustionHeat * cell.gas.amount * 0.6f;
            MatId gm = cell.gas.mat;
            cell.gas = MatterSlot{};
            if (gd.burnGas != kNoMat) g.addGas(x, y, gd.burnGas, 10);
            if (gm == mat::Hydrogen) g.addGas(x, y, mat::Water, 10); // H₂ + O₂ → H₂O
            g.explode(x, y, energy, tick);
        }
    }

    // ---------------- 3. Nung (firing / calcination / decomposition) ----------------
    for (auto& s : cell.s) {
        if (s.empty()) continue;
        const MaterialDef& d = R[s.mat];
        if (d.firesTo == kNoMat || T < d.fireTemp) continue;
        int n = std::min<int>(s.amount, 4);
        MatId from = s.mat, to = d.firesTo;
        consume(s, n);
        produce(k, to, n);
        if (d.fireGas != kNoMat) g.addGas(x, y, d.fireGas, n * 2);
        if (shouldLog(k, 0.25f)) log(k, ReactionKind::Firing, from, kNoMat, to, d.fireGas);
    }

    // ---------------- 4. Khử quặng (luyện kim) ----------------
    MatterSlot* reducer = find(k, mat::Charcoal);
    for (auto& s : cell.s) {
        if (s.empty()) continue;
        const MaterialDef& d = R[s.mat];
        if (d.smeltsTo == kNoMat || T < d.smeltTemp) continue;
        reducer = find(k, mat::Charcoal);
        if (!reducer) break;
        int n = std::min<int>({static_cast<int>(s.amount), 4, reducer->amount * 2});
        if (n <= 0) continue;
        MatId ore = s.mat, metal = d.smeltsTo;
        consume(s, n);
        consume(*reducer, std::max(1, n / 2));
        produce(k, metal, std::max(1, n * 6 / 10));
        g.addGas(x, y, mat::CO2, n);
        log(k, ReactionKind::Smelting, ore, mat::Charcoal, metal, mat::CO2);
    }

    // ---------------- 5. Hợp kim: hai kim loại lỏng trộn lẫn → vật liệu mới ----------------
    for (int a = 0; a < 3; ++a)
        for (int b = a + 1; b < 3; ++b) {
            MatterSlot& A = cell.s[a];
            MatterSlot& B = cell.s[b];
            if (A.empty() || B.empty() || A.mat == B.mat) continue;
            if (!R[A.mat].metal || !R[B.mat].metal) continue;
            if (R[A.mat].phaseAt(T) != Phase::Liquid || R[B.mat].phaseAt(T) != Phase::Liquid) continue;
            MatId ma = A.mat, mb = B.mat;
            MatId al = R.alloy(ma, mb);
            int total = A.amount + B.amount;
            A = MatterSlot{al, static_cast<uint16_t>(std::min(60000, total)), true};
            B = MatterSlot{};
            log(k, ReactionKind::Alloying, ma, mb, al, kNoMat);
        }

    // ---------------- 6. Hydrat hoá (cần nước) ----------------
    bool water = waterNearby(k);
    if (water) {
        for (auto& s : cell.s) {
            if (s.empty()) continue;
            const MaterialDef& d = R[s.mat];
            if (d.hydratesTo == kNoMat || d.phaseAt(T) != Phase::Solid) continue;
            int n = std::min<int>(s.amount, 4);
            MatId from = s.mat, to = d.hydratesTo;
            consume(s, n);
            produce(k, to, n);
            if (MatterSlot* ws = find(k, mat::Water)) consume(*ws, n);
            T += d.hydrationHeat * n * 0.5f;    // vôi sống + nước nóng tới mức làm cháy gỗ!
            if (shouldLog(k, 0.25f)) log(k, ReactionKind::Hydration, from, mat::Water, to, kNoMat);
        }
        // Khí SO₂ hoà tan trong nước → axit (mưa axit)
        if (cell.gas.mat == mat::SO2 && cell.gas.amount > 5) {
            int n = cell.gas.amount / 3;
            cell.gas.amount = static_cast<uint16_t>(cell.gas.amount - n);
            produce(k, mat::Acid, std::max(1, n / 3));
            if (shouldLog(k, 0.3f)) log(k, ReactionKind::Hydration, mat::SO2, mat::Water, mat::Acid, kNoMat);
        }
    }

    // ---------------- 7–8. Axit ----------------
    for (auto& a : cell.s) {
        if (a.empty() || R[a.mat].acidity >= 4 || R[a.mat].phaseAt(T) != Phase::Liquid) continue;
        for (auto& b : cell.s) {
            if (&a == &b || b.empty() || a.empty()) continue;
            const MaterialDef& bd = R[b.mat];
            MatId am = a.mat, bm = b.mat;
            if (bd.acidity > 9.5f) {
                int n = std::min<int>({static_cast<int>(a.amount), static_cast<int>(b.amount), 4});
                consume(a, n); consume(b, n);
                produce(k, mat::SaltWater, n * 2);
                T += n * 6;
                log(k, ReactionKind::Neutralization, am, bm, mat::SaltWater, kNoMat);
            } else if (bd.metal && bd.reactivity > 0.05f) {
                int n = std::min<int>({static_cast<int>(a.amount), static_cast<int>(b.amount), 2});
                consume(a, n); consume(b, n);
                produce(k, mat::SaltWater, n);
                g.addGas(x, y, mat::Hydrogen, n * 4);
                T += n * 4;
                if (shouldLog(k, 0.4f)) log(k, ReactionKind::AcidMetal, am, bm, mat::SaltWater, mat::Hydrogen);
            } else if (b.mat == mat::Limestone) {
                int n = std::min<int>({static_cast<int>(a.amount), static_cast<int>(b.amount), 3});
                consume(a, n); consume(b, n);
                produce(k, mat::SaltWater, n);
                g.addGas(x, y, mat::CO2, n * 3);
                if (shouldLog(k, 0.4f)) log(k, ReactionKind::Neutralization, am, bm, mat::SaltWater, mat::CO2);
            }
        }
    }

    // ---------------- 9. Oxi hoá chậm (gỉ sét) ----------------
    if (water)
        for (auto& s : cell.s) {
            if (s.empty()) continue;
            const MaterialDef& d = R[s.mat];
            if (!d.metal || d.oxide == kNoMat || d.phaseAt(T) != Phase::Solid) continue;
            if (!rng.chance(d.reactivity * 0.02f)) continue;
            MatId from = s.mat;
            consume(s, 1);
            produce(k, d.oxide, 1);
            if (shouldLog(k, 0.1f)) log(k, ReactionKind::Oxidation, from, mat::Water, d.oxide, kNoMat);
        }

    // ---------------- 10. Dung nham gặp nước ----------------
    for (auto& s : cell.s) {
        if (s.empty() || (s.mat != mat::Stone && s.mat != mat::Basalt) || R[s.mat].phaseAt(T) != Phase::Liquid) continue;
        if (MatterSlot* ws = find(k, mat::Water)) {
            int n = std::min<int>({static_cast<int>(s.amount), static_cast<int>(ws->amount), 8});
            consume(*ws, n);
            g.addGas(x, y, mat::Water, n * 2);
            T -= n * 40;
            if (T < 1000) { s.mat = mat::Obsidian; s.molten = false; log(k, ReactionKind::Quench, mat::Stone, mat::Water, mat::Obsidian, mat::Water); }
        }
    }

    // ---------------- 11. Trộn bột → thuốc súng ----------------
    MatterSlot* c = find(k, mat::Charcoal);
    MatterSlot* su = find(k, mat::Sulfur);
    MatterSlot* sp = find(k, mat::Saltpeter);
    if (c && su && sp && T < 250) {
        int n = std::min<int>({static_cast<int>(c->amount), static_cast<int>(su->amount), static_cast<int>(sp->amount)});
        consume(*c, n); consume(*su, n); consume(*sp, n);
        produce(k, mat::Gunpowder, n * 3);
        log(k, ReactionKind::Mixing, mat::Charcoal, mat::Sulfur, mat::Gunpowder, kNoMat, mat::Saltpeter);
    }

    // ---------------- 12. Phân huỷ chất hữu cơ ----------------
    for (auto& s : cell.s) {
        if (s.empty()) continue;
        const MaterialDef& d = R[s.mat];
        if (d.decaysTo == kNoMat || T < 4 || T > 60 || !rng.chance(0.01f)) continue;
        MatId from = s.mat;
        consume(s, 1);
        Tile& t = w.at(x, y);
        t.food = static_cast<uint8_t>(std::min(255, t.food + 4)); // mùn làm đất màu mỡ → chu trình dinh dưỡng
        if (d.decayGas != kNoMat) g.addGas(x, y, d.decayGas, 2);
        if (shouldLog(k, 0.05f)) log(k, ReactionKind::Decay, from, kNoMat, d.decaysTo, d.decayGas);
    }

    // Gộp ngăn trùng vật liệu
    for (int a = 0; a < 3; ++a)
        for (int b = a + 1; b < 3; ++b)
            if (!cell.s[a].empty() && cell.s[a].mat == cell.s[b].mat) {
                cell.s[a].amount = static_cast<uint16_t>(std::min(60000, cell.s[a].amount + cell.s[b].amount));
                cell.s[b] = MatterSlot{};
            }
}

} // namespace wb
