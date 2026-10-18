// =============================================================================
// Material.cpp — Bảng vật liệu tự nhiên (số liệu gần với thực tế, đã làm tròn)
// và cơ chế sinh hợp kim / khoáng chất mới khi đang chạy.
// =============================================================================
#include "Material.h"
#include "../core/Math.h"
#include <cmath>

namespace wb {

namespace {
MaterialDef base(const char* name, const char* formula, uint8_t r, uint8_t g, uint8_t b) {
    MaterialDef d;
    d.name = name; d.formula = formula; d.r = r; d.g = g; d.b = b;
    return d;
}
} // namespace

void MaterialRegistry::initDefaults(uint32_t worldSeed) {
    seed_ = worldSeed;
    defs_.clear();
    alloyCache_.clear();
    exoticOres.clear();
    auto& D = defs_;
    D.resize(mat::DefaultCount);

    // ---------------- Khí / chất lỏng cơ bản ----------------
    D[mat::Air] = base("Không khí", "N₂O₂", 0, 0, 0);
    D[mat::Air].density = 1.2f; D[mat::Air].meltPoint = -210; D[mat::Air].boilPoint = -190; D[mat::Air].oxidizer = true;

    auto& W = D[mat::Water] = base("Nước", "H₂O", 60, 110, 200);
    W.density = 1000; W.meltPoint = 0; W.boilPoint = 100; W.heatCapacity = 4.2f; W.conductivity = 0.6f;
    W.hardness = 1.5f; W.viscosity = 0.1f;   // băng có độ cứng ~1.5

    auto& SW = D[mat::SaltWater] = base("Nước muối", "NaCl(aq)", 70, 120, 190);
    SW.density = 1030; SW.meltPoint = -2; SW.boilPoint = 101; SW.heatCapacity = 4.0f; SW.conductivity = 0.6f;
    SW.viscosity = 0.12f; SW.dissolvedSalt = mat::Salt; SW.acidity = 7;

    auto& Sa = D[mat::Salt] = base("Muối", "NaCl", 240, 240, 235);
    Sa.density = 2160; Sa.meltPoint = 801; Sa.boilPoint = 1465; Sa.hardness = 2.5f; Sa.powder = true; Sa.toughness = 0.1f;

    // ---------------- Khoáng & đất ----------------
    auto& Sd = D[mat::Sand] = base("Cát", "SiO₂", 222, 206, 140);
    Sd.density = 1600; Sd.meltPoint = 1700; Sd.boilPoint = 2230; Sd.hardness = 7; Sd.toughness = 0.05f; Sd.powder = true;
    Sd.porous = true; Sd.solidifiesTo = mat::Glass; Sd.viscosity = 0.95f;

    auto& Gl = D[mat::Glass] = base("Thuỷ tinh", "SiO₂", 190, 230, 235);
    Gl.density = 2500; Gl.meltPoint = 1400; Gl.boilPoint = 2230; Gl.hardness = 5.5f; Gl.toughness = 0.05f; Gl.conductivity = 0.1f;

    auto& Di = D[mat::Dirt] = base("Đất", "—", 120, 90, 60);
    Di.density = 1300; Di.meltPoint = 1300; Di.hardness = 1.5f; Di.toughness = 0.2f; Di.powder = true; Di.porous = true;

    auto& Mu = D[mat::Mud] = base("Bùn", "—", 90, 70, 50);
    Mu.density = 1700; Mu.meltPoint = -1; Mu.boilPoint = 105; Mu.viscosity = 0.85f; Mu.hardness = 0.5f;

    auto& Cl = D[mat::Clay] = base("Đất sét", "Al₂Si₂O₅", 175, 120, 90);
    Cl.density = 1700; Cl.meltPoint = 1600; Cl.hardness = 1.5f; Cl.toughness = 0.3f; Cl.conductivity = 0.15f;
    Cl.firesTo = mat::Brick; Cl.fireTemp = 850;

    auto& Br = D[mat::Brick] = base("Gạch nung", "—", 170, 70, 50);
    Br.density = 1900; Br.meltPoint = 1650; Br.hardness = 5; Br.toughness = 0.3f; Br.conductivity = 0.08f;

    auto& St = D[mat::Stone] = base("Đá", "CaMgSiO", 140, 140, 135);
    St.density = 2700; St.meltPoint = 1200; St.boilPoint = 2800; St.hardness = 6; St.toughness = 0.3f; St.conductivity = 0.25f;
    St.solidifiesTo = mat::Basalt; St.viscosity = 0.9f;

    auto& Fl = D[mat::Flint] = base("Đá lửa", "SiO₂", 90, 85, 95);
    Fl.density = 2600; Fl.meltPoint = 1700; Fl.hardness = 7; Fl.toughness = 0.15f; // giòn → ghè thành mảnh sắc

    auto& Li = D[mat::Limestone] = base("Đá vôi", "CaCO₃", 210, 205, 190);
    Li.density = 2700; Li.meltPoint = 1330; Li.hardness = 3; Li.toughness = 0.3f;
    Li.firesTo = mat::Quicklime; Li.fireTemp = 850; Li.fireGas = mat::CO2; Li.acidity = 8.5f;

    auto& QL = D[mat::Quicklime] = base("Vôi sống", "CaO", 245, 245, 240);
    QL.density = 3300; QL.meltPoint = 2600; QL.hardness = 3.5f; QL.powder = true; QL.acidity = 12.5f; QL.toxicity = 0.2f;
    QL.hydratesTo = mat::SlakedLime; QL.hydrationHeat = 160; // + nước toả nhiệt rất mạnh!

    auto& SL = D[mat::SlakedLime] = base("Vôi tôi", "Ca(OH)₂", 235, 235, 230);
    SL.density = 2200; SL.meltPoint = 580; SL.hardness = 2; SL.powder = true; SL.acidity = 12;
    SL.firesTo = mat::Quicklime; SL.fireTemp = 580;

    auto& Ba = D[mat::Basalt] = base("Đá bazan", "—", 70, 70, 75);
    Ba.density = 3000; Ba.meltPoint = 1200; Ba.hardness = 6; Ba.toughness = 0.4f; Ba.conductivity = 0.2f;

    auto& Ob = D[mat::Obsidian] = base("Đá vỏ chai", "SiO₂", 35, 25, 45);
    Ob.density = 2400; Ob.meltPoint = 1000; Ob.hardness = 5.5f; Ob.toughness = 0.05f; // cực sắc nhưng rất giòn

    // ---------------- Hữu cơ ----------------
    auto& Wo = D[mat::Wood] = base("Gỗ", "C₆H₁₀O₅", 140, 100, 55);
    Wo.density = 600; Wo.meltPoint = 1e6f; Wo.hardness = 2.5f; Wo.toughness = 0.7f; Wo.elasticity = 0.5f; Wo.conductivity = 0.1f;
    Wo.ignitionPoint = 300; Wo.combustionHeat = 16; Wo.organic = true; Wo.porous = false;
    Wo.burnResidue = mat::Ash; Wo.burnGas = mat::CO2; Wo.pyrolysisTo = mat::Charcoal;
    Wo.decaysTo = mat::Dirt;

    auto& Ch = D[mat::Charcoal] = base("Than củi", "C", 40, 40, 40);
    Ch.density = 300; Ch.hardness = 1; Ch.toughness = 0.1f; Ch.ignitionPoint = 350; Ch.combustionHeat = 30; // cháy nóng hơn gỗ
    Ch.burnResidue = mat::Ash; Ch.burnGas = mat::CO2; Ch.powder = true;

    auto& As = D[mat::Ash] = base("Tro", "K₂CO₃", 150, 145, 140);
    As.density = 500; As.hardness = 0.5f; As.powder = true; As.porous = true; As.acidity = 10;
    As.hydratesTo = mat::Lye; // tro + nước → nước tro (kiềm)

    auto& Fi = D[mat::Fiber] = base("Sợi thực vật", "C₆H₁₀O₅", 180, 170, 110);
    Fi.density = 500; Fi.hardness = 0.5f; Fi.toughness = 0.8f; Fi.elasticity = 0.3f; Fi.ignitionPoint = 250; Fi.combustionHeat = 15;
    Fi.organic = true; Fi.porous = true; Fi.conductivity = 0.05f; Fi.burnResidue = mat::Ash; Fi.burnGas = mat::CO2; Fi.decaysTo = mat::Dirt;

    auto& Hi = D[mat::Hide] = base("Da thú", "—", 160, 110, 70);
    Hi.density = 900; Hi.hardness = 1; Hi.toughness = 0.9f; Hi.elasticity = 0.4f; Hi.conductivity = 0.04f; Hi.organic = true;
    Hi.ignitionPoint = 350; Hi.combustionHeat = 12; Hi.burnResidue = mat::Ash; Hi.burnGas = mat::CO2; Hi.decaysTo = mat::Dirt;

    auto& Fs = D[mat::Flesh] = base("Thịt", "—", 190, 80, 80);
    Fs.density = 1050; Fs.hardness = 0.2f; Fs.toughness = 0.6f; Fs.nutrition = 1; Fs.organic = true; Fs.meltPoint = 1e6f;
    Fs.ignitionPoint = 400; Fs.combustionHeat = 8; Fs.burnResidue = mat::Ash; Fs.burnGas = mat::CO2;
    Fs.decaysTo = mat::Dirt; Fs.decayGas = mat::Methane;

    auto& Bo = D[mat::Bone] = base("Xương", "Ca₅(PO₄)₃", 230, 225, 205);
    Bo.density = 1900; Bo.hardness = 3; Bo.toughness = 0.5f; Bo.organic = true;

    auto& Bl = D[mat::Blood] = base("Máu", "—", 150, 20, 20);
    Bl.density = 1060; Bl.meltPoint = -1; Bl.boilPoint = 100; Bl.viscosity = 0.3f; Bl.organic = true; Bl.decaysTo = mat::Dirt;

    auto& Fa = D[mat::Fat] = base("Mỡ", "C₅₅H₉₈O₆", 240, 220, 150);
    Fa.density = 900; Fa.meltPoint = 40; Fa.boilPoint = 300; Fa.ignitionPoint = 280; Fa.combustionHeat = 37; Fa.organic = true;
    Fa.nutrition = 0.8f; Fa.viscosity = 0.5f; Fa.burnResidue = kNoMat; Fa.burnGas = mat::CO2;

    // ---------------- Quặng & kim loại ----------------
    auto& CO = D[mat::CopperOre] = base("Quặng đồng", "Cu₂(OH)₂CO₃", 70, 150, 110);
    CO.density = 4000; CO.meltPoint = 1100; CO.hardness = 3.5f; CO.toughness = 0.2f; CO.smeltsTo = mat::Copper; CO.smeltTemp = 1000;
    auto& Cu = D[mat::Copper] = base("Đồng", "Cu", 200, 120, 70);
    Cu.density = 8960; Cu.meltPoint = 1085; Cu.boilPoint = 2560; Cu.hardness = 3; Cu.toughness = 0.85f; Cu.elasticity = 0.2f;
    Cu.conductivity = 0.95f; Cu.metal = true; Cu.reactivity = 0.15f; Cu.oxide = mat::CopperOre; Cu.viscosity = 0.15f;

    auto& TO = D[mat::TinOre] = base("Quặng thiếc", "SnO₂", 120, 110, 100);
    TO.density = 6900; TO.meltPoint = 1630; TO.hardness = 6.5f; TO.toughness = 0.2f; TO.smeltsTo = mat::Tin; TO.smeltTemp = 900;
    auto& Sn = D[mat::Tin] = base("Thiếc", "Sn", 200, 200, 205);
    Sn.density = 7300; Sn.meltPoint = 232; Sn.boilPoint = 2600; Sn.hardness = 1.5f; Sn.toughness = 0.6f; Sn.conductivity = 0.6f;
    Sn.metal = true; Sn.reactivity = 0.05f; Sn.viscosity = 0.1f;

    auto& IO = D[mat::IronOre] = base("Quặng sắt", "Fe₂O₃", 140, 70, 50);
    IO.density = 5200; IO.meltPoint = 1565; IO.hardness = 5.5f; IO.toughness = 0.2f; IO.smeltsTo = mat::Iron; IO.smeltTemp = 1250;
    auto& Fe = D[mat::Iron] = base("Sắt", "Fe", 150, 150, 160);
    Fe.density = 7870; Fe.meltPoint = 1538; Fe.boilPoint = 2860; Fe.hardness = 4.5f; Fe.toughness = 0.8f; Fe.elasticity = 0.4f;
    Fe.conductivity = 0.8f; Fe.metal = true; Fe.reactivity = 0.45f; Fe.oxide = mat::Rust; Fe.viscosity = 0.2f;

    auto& Ru = D[mat::Rust] = base("Gỉ sắt", "Fe₂O₃·H₂O", 160, 80, 40);
    Ru.density = 5200; Ru.meltPoint = 1565; Ru.hardness = 2; Ru.toughness = 0.05f; Ru.powder = true;
    Ru.smeltsTo = mat::Iron; Ru.smeltTemp = 1250;

    auto& Au = D[mat::Gold] = base("Vàng", "Au", 240, 200, 60);
    Au.density = 19300; Au.meltPoint = 1064; Au.boilPoint = 2970; Au.hardness = 2.5f; Au.toughness = 0.9f; Au.conductivity = 0.9f;
    Au.metal = true; Au.reactivity = 0.0f; Au.viscosity = 0.12f;

    // ---------------- Hoá chất ----------------
    auto& Su = D[mat::Sulfur] = base("Lưu huỳnh", "S", 230, 220, 60);
    Su.density = 2000; Su.meltPoint = 115; Su.boilPoint = 445; Su.hardness = 2; Su.powder = true;
    Su.ignitionPoint = 230; Su.combustionHeat = 9; Su.burnGas = mat::SO2; Su.toxicity = 0.1f;

    auto& Sp = D[mat::Saltpeter] = base("Diêm tiêu", "KNO₃", 235, 235, 225);
    Sp.density = 2100; Sp.meltPoint = 334; Sp.hardness = 2; Sp.powder = true; Sp.oxidizer = true;

    auto& Gp = D[mat::Gunpowder] = base("Thuốc súng", "KNO₃+C+S", 60, 60, 65);
    Gp.density = 1700; Gp.hardness = 1; Gp.powder = true; Gp.ignitionPoint = 300; Gp.combustionHeat = 3; Gp.explosive = true;
    Gp.burnGas = mat::CO2; Gp.burnResidue = mat::Ash;

    auto& Ac = D[mat::Acid] = base("Axit", "H₂SO₄", 200, 230, 90);
    Ac.density = 1830; Ac.meltPoint = 10; Ac.boilPoint = 337; Ac.acidity = 1; Ac.toxicity = 0.7f; Ac.viscosity = 0.3f;

    auto& Ly = D[mat::Lye] = base("Nước tro (kiềm)", "KOH(aq)", 200, 200, 170);
    Ly.density = 1100; Ly.meltPoint = -2; Ly.boilPoint = 102; Ly.acidity = 13; Ly.toxicity = 0.3f; Ly.viscosity = 0.12f;

    auto& Oi = D[mat::Oil] = base("Dầu", "CₙH₂ₙ", 50, 40, 30);
    Oi.density = 850; Oi.meltPoint = -30; Oi.boilPoint = 350; Oi.ignitionPoint = 210; Oi.combustionHeat = 45; Oi.viscosity = 0.4f;
    Oi.burnGas = mat::CO2;

    // ---------------- Khí ----------------
    auto& C2 = D[mat::CO2] = base("Khí CO₂", "CO₂", 90, 90, 90);
    C2.density = 1.9f; C2.meltPoint = -78; C2.boilPoint = -78; C2.acidity = 6;
    auto& Me = D[mat::Methane] = base("Khí mê-tan", "CH₄", 140, 160, 120);
    Me.density = 0.7f; Me.meltPoint = -182; Me.boilPoint = -161; Me.ignitionPoint = 580; Me.combustionHeat = 55; Me.explosive = true;
    Me.burnGas = mat::CO2;
    auto& H2 = D[mat::Hydrogen] = base("Khí hiđrô", "H₂", 220, 220, 255);
    H2.density = 0.09f; H2.meltPoint = -259; H2.boilPoint = -253; H2.ignitionPoint = 500; H2.combustionHeat = 120; H2.explosive = true;
    H2.burnGas = kNoMat;
    auto& S2 = D[mat::SO2] = base("Khí SO₂", "SO₂", 200, 200, 120);
    S2.density = 2.6f; S2.meltPoint = -72; S2.boilPoint = -10; S2.toxicity = 0.6f; S2.acidity = 3;
    S2.hydratesTo = mat::Acid; // SO₂ + nước → axit (mưa axit!)

    // ---------------- Khoáng chất lạ (sinh theo seed) ----------------
    // Mỗi thế giới có 2 khoáng chất chưa từng có, thuộc tính ngẫu nhiên → các nền văn
    // minh ở thế giới khác nhau sẽ có vật liệu "thần kỳ" khác nhau.
    Rng r(worldSeed * 2654435761u + 17);
    static const char* syl[] = {"ka", "ri", "zor", "mi", "thal", "vex", "ur", "ny", "dra", "sil", "quo", "bel"};
    for (int k = 0; k < 2; ++k) {
        std::string nm;
        for (int s = 0; s < 2; ++s) nm += syl[r.irange(0, 11)];
        nm[0] = static_cast<char>(nm[0] - 'a' + 'A');
        MaterialDef metal = base(("Kim loại " + nm).c_str(), "X", static_cast<uint8_t>(r.irange(80, 255)),
                                 static_cast<uint8_t>(r.irange(80, 255)), static_cast<uint8_t>(r.irange(80, 255)));
        metal.metal = true;
        metal.density = r.range(2000, 15000);
        metal.meltPoint = r.range(400, 2400);
        metal.boilPoint = metal.meltPoint + r.range(800, 2000);
        metal.hardness = r.range(2, 8.5f);
        metal.toughness = r.range(0.3f, 0.95f);
        metal.elasticity = r.range(0.1f, 0.8f);
        metal.conductivity = r.range(0.1f, 1.0f);
        metal.reactivity = r.range(0, 0.6f);
        metal.natural = true;
        MatId metalId = add(metal);
        MaterialDef ore = base(("Quặng " + nm).c_str(), "X-ore", static_cast<uint8_t>(defs_[metalId].r * 0.6f),
                               static_cast<uint8_t>(defs_[metalId].g * 0.6f), static_cast<uint8_t>(defs_[metalId].b * 0.6f));
        ore.density = metal.density * 0.6f; ore.hardness = std::min(9.0f, metal.hardness + 1); ore.toughness = 0.2f;
        ore.meltPoint = metal.meltPoint + 200; ore.smeltsTo = metalId; ore.smeltTemp = metal.meltPoint * r.range(0.7f, 1.1f);
        exoticOres.push_back(add(ore));
    }
}

MatId MaterialRegistry::add(const MaterialDef& d) {
    defs_.push_back(d);
    return static_cast<MatId>(defs_.size() - 1);
}

MatId MaterialRegistry::alloy(MatId a, MatId b, const std::string& suggestedName) {
    if (a == b) return a;
    if (a > b) std::swap(a, b);
    uint32_t key = (static_cast<uint32_t>(a) << 16) | b;
    auto it = alloyCache_.find(key);
    if (it != alloyCache_.end()) return it->second;
    if (defs_.size() >= 60000) return a; // giới hạn kỹ thuật của MatId 16 bit
    const MaterialDef& A = defs_[a];
    const MaterialDef& B = defs_[b];
    // Cộng hưởng: mỗi cặp kim loại có một "định mệnh" riêng theo seed thế giới.
    // Có cặp cứng vượt trội (như đồng + thiếc → đồng điếu), có cặp lại tệ hơn cả hai.
    Rng r(hash32(key ^ (seed_ * 0x9E3779B9u)));
    float synergy = r.range(0.75f, 1.6f);
    MaterialDef c;
    c.metal = true;
    c.natural = false;
    c.parentA = a; c.parentB = b;
    c.generation = std::max(A.generation, B.generation) + 1;
    c.density = (A.density + B.density) * 0.5f;
    c.meltPoint = std::min(A.meltPoint, B.meltPoint) * r.range(0.85f, 1.05f); // hợp kim thường chảy ở nhiệt thấp hơn
    c.boilPoint = std::max(A.boilPoint, B.boilPoint);
    c.hardness = std::min(10.0f, std::max(A.hardness, B.hardness) * synergy);
    c.toughness = clamp01((A.toughness + B.toughness) * 0.5f * r.range(0.8f, 1.25f));
    c.elasticity = clamp01((A.elasticity + B.elasticity) * 0.5f * r.range(0.7f, 1.5f));
    c.conductivity = clamp01((A.conductivity + B.conductivity) * 0.5f * r.range(0.5f, 1.0f));
    c.reactivity = clamp01(std::min(A.reactivity, B.reactivity) * r.range(0.3f, 1.2f));
    c.viscosity = (A.viscosity + B.viscosity) * 0.5f;
    c.oxide = A.oxide != kNoMat ? A.oxide : B.oxide;
    c.r = static_cast<uint8_t>((A.r + B.r) / 2); c.g = static_cast<uint8_t>((A.g + B.g) / 2); c.b = static_cast<uint8_t>((A.b + B.b) / 2);
    c.name = suggestedName.empty() ? ("Hợp kim " + A.name + "–" + B.name) : suggestedName;
    c.formula = A.formula + "·" + B.formula;
    MatId id = add(c);
    alloyCache_[key] = id;
    return id;
}

std::string MaterialRegistry::displayName(MatId id, float tempC) const {
    const MaterialDef& d = defs_[id];
    Phase p = d.phaseAt(tempC);
    if (id == mat::Water) return p == Phase::Solid ? "Băng" : (p == Phase::Gas ? "Hơi nước" : "Nước");
    if (id == mat::Stone || id == mat::Basalt) return p == Phase::Liquid ? "Dung nham" : d.name;
    if (p == Phase::Liquid && d.meltPoint > 100) return d.name + " nóng chảy";
    if (p == Phase::Gas && d.boilPoint > 0) return "Hơi " + d.name;
    return d.name;
}

float MaterialRegistry::advancement(MatId id) const {
    const MaterialDef& d = defs_[id];
    // Kim loại cứng, khó luyện, nhiều thế hệ hợp kim → "tiến bộ" hơn. Không có trần.
    float a = d.hardness * (0.5f + d.toughness);
    if (d.metal) a += 3 + d.meltPoint / 400.0f;
    a += d.generation * 2.0f;
    return a;
}

} // namespace wb
