// =============================================================================
// Invention.cpp — Vật lý của thiết kế (tính hiệu năng từ thuộc tính vật liệu),
// ràng buộc chế tạo, và các registry thiết kế / quy trình.
// =============================================================================
#include "Invention.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace wb {

const char* conceptName(Concept c) {
    static const char* n[] = {"Độ sắc bén", "Đòn bẩy", "Sức nổi", "Tính đàn hồi", "Vật chứa", "Sự cháy", "Cách nhiệt",
                              "Nóng chảy", "Khử quặng", "Hợp kim", "Chuyển động quay", "Áp suất", "Ma sát sinh nhiệt"};
    return n[static_cast<int>(c)];
}
const char* shapeName(Shape s) {
    static const char* n[] = {"cục", "mảnh sắc", "cán", "mũi nhọn", "lưỡi", "đầu nặng", "dây", "tấm", "vật chứa",
                              "ván", "bánh xe", "thân đàn hồi", "ống", "tường"};
    return n[static_cast<int>(s)];
}
const char* purposeName(Purpose p) {
    static const char* n[] = {"Cắt", "Đào", "Đâm", "Đập", "Bắn xa", "Công thành", "Chuyên chở", "Nổi trên nước",
                              "Nhiệt độ lò", "Tạo lửa", "Giữ ấm"};
    return n[static_cast<int>(p)];
}
const char* purposeNoun(Purpose p) {
    static const char* n[] = {"Rìu", "Cuốc", "Giáo", "Chuỳ", "Cung", "Máy bắn đá", "Xe", "Bè", "Lò", "Mồi lửa", "Áo"};
    return n[static_cast<int>(p)];
}

namespace {
// Thể tích vật liệu (lít) và thể tích chiếm chỗ trong nước (lít) của mỗi hình dạng ở size = 1
struct ShapeGeo { float matVol, dispVol; };
const ShapeGeo kGeo[kShapeCount] = {
    {1.0f, 1.0f},   // Lump
    {0.05f, 0.05f}, // Flake
    {0.4f, 0.4f},   // Shaft
    {0.05f, 0.05f}, // Point
    {0.15f, 0.15f}, // Blade
    {0.6f, 0.6f},   // Head
    {0.05f, 0.05f}, // Cord
    {0.5f, 0.0f},   // Sheet (chỉ chiếm chỗ khi căng trên khung)
    {2.0f, 60.0f},  // Bowl (rỗng → chiếm nhiều nước — nguyên lý thuyền)
    {20.0f, 20.0f}, // Plank
    {3.0f, 3.0f},   // Wheel
    {0.5f, 0.5f},   // Limb
    {0.6f, 2.0f},   // Pipe
    {30.0f, 30.0f}, // Wall
};
} // namespace

float partMass(const MaterialRegistry& reg, const Part& p) {
    return reg[p.mat].density / 1000.0f * kGeo[static_cast<int>(p.shape)].matVol * p.size;
}

int partCost(const MaterialRegistry& reg, const Part& p) {
    return std::max(1, static_cast<int>(std::lround(partMass(reg, p) / 3.0f)));
}

bool knappable(const MaterialDef& d) { return !d.metal && d.hardness >= 4.5f && d.toughness < 0.35f; }

DesignProps evaluateDesign(const MaterialRegistry& reg, const std::vector<Part>& parts) {
    DesignProps P;
    auto V = [&](Purpose p) -> float& { return P.v[static_cast<int>(p)]; };
    const Part* shaft = nullptr; const Part* cord = nullptr; const Part* limb = nullptr; const Part* pipe = nullptr;
    float edge = 0, point = 0, headMass = 0, headHard = 0, dispVol = 0, warm = 0, sheetTough = 0;
    float wallCond = 0, wallMelt = 1e9f, wallSize = 0, carry = 0;
    int walls = 0, wheels = 0, lumps = 0, sheets = 0;
    const Part* hardLump = nullptr; const Part* sparkMetal = nullptr;
    P.durability = 1e9f;
    for (const Part& p : parts) {
        const MaterialDef& d = reg[p.mat];
        float m = partMass(reg, p);
        P.mass += m;
        P.durability = std::min(P.durability, d.hardness * d.toughness * 10 + 5);
        float sz = std::min(1.5f, p.size);
        switch (p.shape) {
        case Shape::Shaft: if (!shaft || p.size > shaft->size) shaft = &p; break;
        case Shape::Cord: cord = &p; break;
        case Shape::Limb: if (!limb || d.elasticity * p.size > reg[limb->mat].elasticity * limb->size) limb = &p; break;
        case Shape::Pipe: pipe = &p; break;
        case Shape::Flake: edge = std::max(edge, d.hardness * (0.35f + d.toughness) * 0.8f * std::min(1.0f, sz)); break;
        case Shape::Blade: edge = std::max(edge, d.hardness * (0.35f + d.toughness) * std::min(1.2f, sz)); break;
        case Shape::Point: point = std::max(point, d.hardness * (0.3f + d.toughness) * std::min(1.0f, sz)); break;
        case Shape::Head: headMass += m; headHard = std::max(headHard, d.hardness); break;
        case Shape::Lump:
            lumps++; headMass += m * 0.6f; headHard = std::max(headHard, d.hardness * 0.8f);
            if (d.hardness >= 6 && !d.metal) hardLump = &p;
            if (d.metal && d.reactivity >= 0.3f) sparkMetal = &p;
            break;
        case Shape::Sheet:
            sheets++;
            warm += (1.0f - d.conductivity) * p.size * 8.0f;
            if (!d.porous) sheetTough = std::max(sheetTough, d.toughness * std::min(1.0f, p.size));
            carry += 6 * p.size;   // túi / bao
            break;
        case Shape::Bowl:
            if (!d.porous) dispVol += kGeo[static_cast<int>(Shape::Bowl)].dispVol * p.size;
            carry += 15 * p.size;
            break;
        case Shape::Plank: dispVol += kGeo[static_cast<int>(Shape::Plank)].dispVol * p.size; break;
        case Shape::Wheel: if (d.hardness >= 2 && d.toughness >= 0.35f) wheels++; break;
        case Shape::Wall:
            walls++; wallCond += d.conductivity; wallMelt = std::min(wallMelt, d.meltPoint); wallSize += p.size;
            break;
        default: break;
        }
    }
    if (parts.empty()) return P;
    // Đòn bẩy: cán dài tăng mô-men (cán giòn thì gãy, mất tác dụng)
    float lever = 1.0f;
    if (shaft) lever = 1.0f + 0.6f * shaft->size * std::min(1.0f, reg[shaft->mat].toughness * 1.5f);

    V(Purpose::Cut) = edge * lever;
    float digTip = std::max(point, edge * 0.8f);
    if (digTip <= 0 && headHard > 0) digTip = headHard * 0.3f;
    V(Purpose::Dig) = digTip * lever * (1.0f + std::min(2.0f, headMass) * 0.1f);
    V(Purpose::Pierce) = std::max(point, edge * 0.6f) * (shaft ? 1.0f + 0.5f * shaft->size : 1.0f);
    V(Purpose::Smash) = (headMass > 0 ? std::sqrt(headMass) * std::sqrt(std::max(1.0f, headHard)) * lever * 1.2f : 0) + edge * 0.4f;

    // Bắn xa: năng lượng đàn hồi tích trong cánh cung → động năng mũi tên (½mv²)
    if (limb && cord) {
        const MaterialDef& ld = reg[limb->mat];
        float energy = ld.elasticity * limb->size * 70.0f * std::min(1.0f, reg[cord->mat].toughness * 1.2f) * std::max(0.3f, ld.toughness);
        float arrow = 0.03f + (point > 0 ? 0.02f : 0.04f);
        float v = std::min(90.0f, std::sqrt(2.0f * energy / arrow));
        P.launchSpeed = v;
        P.projMass = arrow;
        V(Purpose::Ranged) = v * (point > 0 ? 0.3f + point * 0.15f : 0.2f);
        // Máy bắn đá: thân đàn hồi cỡ lớn ném đạn đá 10 kg
        if (limb->size >= 1.8f) {
            float bigE = ld.elasticity * limb->size * limb->size * 900.0f * std::max(0.3f, ld.toughness);
            float vs = std::sqrt(2.0f * bigE / 10.0f);
            V(Purpose::Siege) = 0.5f * 10.0f * vs * vs / 1000.0f;   // kJ
            if (V(Purpose::Siege) > 0 && vs > P.launchSpeed * 0.5f) { P.launchSpeed = std::max(P.launchSpeed, vs); }
        }
    } else if (cord && sheets > 0) {
        // Ná: dây + túi quay ném đá
        float v = 18.0f * std::min(1.0f, reg[cord->mat].toughness * 1.3f);
        P.launchSpeed = v; P.projMass = 0.2f;
        V(Purpose::Ranged) = v * 0.15f;
    }
    // Máy bắn đá kiểu đòn bẩy (trebuchet): đối trọng nặng trên cán dài
    if (shaft && shaft->size >= 2.0f && headMass >= 5.0f) {
        float e = headMass * 9.8f * 3.0f * lever;
        float vs = std::sqrt(2.0f * e / 10.0f);
        float kj = 0.5f * 10.0f * vs * vs / 1000.0f;
        if (kj > V(Purpose::Siege)) { V(Purpose::Siege) = kj; P.launchSpeed = std::max(P.launchSpeed, vs); P.projMass = std::max(P.projMass, 10.0f); }
    }
    // Chuyên chở: vật chứa; bánh xe nhân sức chở (ma sát lăn << ma sát trượt)
    V(Purpose::Carry) = carry * (1.0f + wheels * 1.5f);
    // Sức nổi (Archimedes): lực đẩy = khối lượng nước bị chiếm chỗ − khối lượng bản thân
    if (sheets > 0 && shaft) dispVol += 40.0f * std::min(1.5f, sheetTough * 2.0f); // da căng trên khung → thuyền thúng
    V(Purpose::Float) = std::max(0.0f, dispVol * 1.0f - P.mass);
    // Lò: tường cách nhiệt giữ nhiệt; thành lò không được chảy; ống bễ thổi khí → nóng hơn
    if (walls > 0) {
        float c = wallCond / walls;
        float tmax = 600.0f + 900.0f * (1.0f - c) * std::min(1.0f, wallSize / 1.5f);
        if (pipe && sheetTough > 0.3f) tmax += 400.0f * std::min(1.0f, sheetTough * 1.5f);
        P.heatLimit = wallMelt * 0.97f;
        V(Purpose::Heat) = std::min(tmax, P.heatLimit);
    }
    // Tạo lửa: ma sát (khoan gỗ) hoặc đánh đá lửa vào thép tạo tia lửa
    float fire = 0;
    if (shaft && (lumps > 0 || std::any_of(parts.begin(), parts.end(), [](const Part& p) { return p.shape == Shape::Plank; }))) {
        const MaterialDef& sd = reg[shaft->mat];
        fire = 100.0f + 220.0f * (1.0f - sd.conductivity) * std::min(1.0f, sd.toughness + 0.3f);
        if (cord) fire *= 1.7f; // khoan cung: quay nhanh hơn nhiều lần
    }
    if (hardLump && sparkMetal) fire = std::max(fire, 800.0f);
    V(Purpose::FireStart) = fire;
    V(Purpose::Warmth) = sheets > 0 ? warm + (cord ? 1.0f : 0.0f) : 0.0f;   // áo = tấm che + dây buộc
    return P;
}

bool canFabricate(const MaterialRegistry& reg, const Part& p, const Workshop& ws) {
    const MaterialDef& d = reg[p.mat];
    bool cast = d.metal && ws.heat >= d.meltPoint;                               // đúc: nung chảy rồi rót khuôn
    bool forge = d.metal && ws.heat >= d.meltPoint * 0.6f && ws.smash >= d.hardness; // rèn nóng
    bool carveable = d.organic || p.mat == mat::Wood || p.mat == mat::Bone;
    switch (p.shape) {
    case Shape::Lump: case Shape::Head:
        if (d.hardness < 1.5f || d.powder) return false;  // bột / sợi / da không vo thành cục cứng được
        return !d.metal || cast || d.phaseAt(20) == Phase::Solid;
    case Shape::Shaft:
        if (!d.metal && d.hardness < 1.5f) return false;   // sợi/da mềm không làm cán được
        return carveable || cast || forge;
    case Shape::Cord:
        return p.mat == mat::Fiber || p.mat == mat::Hide || (d.metal && (cast || forge) && d.toughness > 0.6f);
    case Shape::Flake:
        return knappable(d);
    case Shape::Point: case Shape::Blade:
        if (d.hardness < 2.0f) return false;        // vật liệu mềm không giữ được cạnh sắc
        if (knappable(d)) return p.size <= 1.2f;
        if (carveable) return ws.cut >= d.hardness * 0.8f;
        if (d.metal) return cast || forge;
        return ws.cut >= d.hardness;   // mài đá cứng không giòn
    case Shape::Sheet:
        if (p.mat == mat::Hide) return ws.cut >= 0.5f;
        if (p.mat == mat::Fiber) return true;
        return d.metal && forge;
    case Shape::Bowl:
        if (p.mat == mat::Clay || p.mat == mat::Brick) return true;
        if (carveable) return ws.cut >= 3.0f;
        if (d.metal) return cast;
        return ws.cut >= d.hardness;
    case Shape::Plank:
        return p.mat == mat::Wood && ws.cut >= 2.0f;
    case Shape::Wheel:
        if (p.mat == mat::Wood) return ws.cut >= 3.0f;
        if (d.metal) return cast;
        return ws.cut >= d.hardness;
    case Shape::Limb:
        if (!d.metal && d.hardness < 1.5f) return false;   // cánh cung cần vật liệu cứng mà dẻo (gỗ, xương, sừng)
        if (carveable) return ws.cut >= 2.0f;
        return d.metal && forge;
    case Shape::Pipe:
        if (p.mat == mat::Clay) return true;
        if (p.mat == mat::Wood) return ws.cut >= 3.0f;
        return d.metal && cast;
    case Shape::Wall:
        return !d.metal && !d.organic && d.phaseAt(20) == Phase::Solid;
    default:
        return false;
    }
}

std::string Design::recipe(const MaterialRegistry& reg) const {
    std::string s;
    for (const Part& p : parts) {
        if (!s.empty()) s += " + ";
        s += reg[p.mat].name + " (" + shapeName(p.shape) + ")";
    }
    return s;
}

int DesignRegistry::findSame(const std::vector<Part>& parts, Purpose p) const {
    for (const Design& d : all) {
        if (d.primary != p || d.parts.size() != parts.size()) continue;
        bool same = true;
        for (size_t i = 0; i < parts.size() && same; ++i) same = d.parts[i] == parts[i];
        if (same) return d.id;
    }
    return -1;
}

int DesignRegistry::add(Design d) {
    d.id = static_cast<int>(all.size());
    all.push_back(d);
    for (int p = 0; p < kPurposeCount; ++p) {
        int& wb = worldBest[p];
        if (all.back().value(static_cast<Purpose>(p)) > 0 && (wb < 0 || all.back().value(static_cast<Purpose>(p)) > all[wb].value(static_cast<Purpose>(p))))
            wb = d.id;
    }
    return d.id;
}

int ProcessRegistry::fromEvent(const ReactionEvent& e, const MaterialRegistry& reg, int group, long tick, bool& isNew) {
    isNew = false;
    switch (e.kind) {
    case ReactionKind::Pyrolysis: case ReactionKind::Smelting: case ReactionKind::Alloying: case ReactionKind::Firing:
    case ReactionKind::Hydration: case ReactionKind::Neutralization: case ReactionKind::Mixing: case ReactionKind::Quench:
    case ReactionKind::Vitrification: case ReactionKind::Precipitation: case ReactionKind::AcidMetal:
        break;
    default:
        return -1; // không phải quy trình sản xuất (cháy, sôi, gỉ...) → chỉ dạy khái niệm
    }
    std::vector<MatId> in, out;
    for (MatId m : e.in) if (m != kNoMat) in.push_back(m);
    for (MatId m : e.out) if (m != kNoMat && m != mat::CO2 && m != mat::Water) out.push_back(m);
    if (in.empty() || out.empty()) return -1;
    std::sort(in.begin(), in.end());
    for (const Process& p : all)
        if (p.kind == e.kind && p.inputs == in && p.outputs == out) return p.id;
    Process p;
    p.id = static_cast<int>(all.size());
    p.kind = e.kind;
    p.inputs = in;
    p.outputs = out;
    // Nhiệt độ cần thiết: làm tròn xuống, có biên an toàn
    p.minTemp = (e.kind == ReactionKind::Hydration || e.kind == ReactionKind::Neutralization || e.kind == ReactionKind::Mixing ||
                 e.kind == ReactionKind::AcidMetal)
                    ? 20.0f
                    : std::floor(e.temp / 50.0f) * 50.0f;
    p.lowOxygen = e.lowOxygen;
    p.group = group;
    p.tick = tick;
    std::string ins, outs;
    for (MatId m : in) { if (!ins.empty()) ins += " + "; ins += reg[m].name; }
    for (MatId m : out) { if (!outs.empty()) outs += " + "; outs += reg[m].name; }
    p.name = std::string(reactionName(e.kind)) + ": " + ins + " → " + outs;
    all.push_back(p);
    isNew = true;
    return p.id;
}

bool Knowhow::knowsMat(MatId m) const { return std::find(materials.begin(), materials.end(), m) != materials.end(); }
bool Knowhow::learnMat(MatId m) {
    if (m == kNoMat || knowsMat(m)) return false;
    materials.push_back(m);
    return true;
}
bool Knowhow::knowsDesign(int id) const { return std::find(designs.begin(), designs.end(), id) != designs.end(); }
bool Knowhow::knowsProcess(int id) const { return std::find(processes.begin(), processes.end(), id) != processes.end(); }
void Knowhow::learnDesign(const DesignRegistry& reg, int id) {
    if (id < 0 || knowsDesign(id)) return;
    designs.push_back(id);
    const Design& d = reg[id];
    for (const Part& p : d.parts) learnMat(p.mat);
    for (int p = 0; p < kPurposeCount; ++p) {
        Purpose pp = static_cast<Purpose>(p);
        if (d.value(pp) > 0 && (best[p] < 0 || d.value(pp) > reg[best[p]].value(pp))) best[p] = id;
    }
}
void Knowhow::learnProcess(int id) {
    if (id >= 0 && !knowsProcess(id)) processes.push_back(id);
}

std::string eraName(const MaterialRegistry& reg, MatId m) {
    if (m == kNoMat) return "Thời đại Tay không";
    return "Thời đại " + reg[m].name;
}

} // namespace wb
