// =============================================================================
// Creativity.cpp — Sáng tạo, thử nghiệm và học hỏi tri thức vật chất.
//
// Quy trình một lần "sáng tạo" (conceiveIdea → đi kiếm vật liệu → realizeIdea):
//   1. Xác định những hình dạng cá thể NGHĨ RA ĐƯỢC (theo khái niệm đã hiểu) và
//      những vật liệu nó ĐÃ BIẾT (chưa có thì sẽ tự đi kiếm sau khi chọn ý tưởng).
//   2. Sinh N phương án (N tăng theo trí tuệ): đột biến thiết kế tốt nhất đã biết,
//      mượn bộ phận từ thiết kế khác (loại suy), hoặc ghép mới hoàn toàn.
//   3. Loại phương án không chế tạo nổi (thiếu công cụ cắt / lò không đủ nóng…).
//   4. Ước lượng hiệu năng bằng "mô hình tinh thần" = vật lý thật × sai số
//      (sai số càng nhỏ khi càng thông minh) → chọn phương án hứa hẹn nhất.
//   5. Làm NGUYÊN MẪU (tiêu tốn vật liệu thật) và THỬ: hiệu năng thật = vật lý.
//   6. Tốt hơn → ghi nhận thiết kế mới (đặt tên theo ngôn ngữ nhóm), tăng chất
//      lượng tay nghề, ghi biên niên sử nếu là kỷ lục thế giới. Thất bại → học
//      được khái niệm liên quan (vd. bè bằng đá chìm → hiểu "sức nổi").
// =============================================================================
#include "Invention.h"
#include "Brain.h"
#include "Mind.h"
#include "../sim/Simulation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace wb {

namespace {
Concept conceptForPurpose(Purpose p) {
    switch (p) {
    case Purpose::Cut: case Purpose::Pierce: return Concept::Sharpness;
    case Purpose::Dig: case Purpose::Smash: case Purpose::Siege: return Concept::Leverage;
    case Purpose::Ranged: return Concept::Elasticity;
    case Purpose::Carry: return Concept::Containment;
    case Purpose::Float: return Concept::Buoyancy;
    case Purpose::Heat: case Purpose::Warmth: return Concept::Insulation;
    case Purpose::FireStart: return Concept::Friction;
    case Purpose::Blast: return Concept::Pressure;
    default: return Concept::Leverage;
    }
}

// Hình dạng mà cá thể có thể HÌNH DUNG ra (phụ thuộc khái niệm đã hiểu, không theo thứ tự cố định)
bool conceivable(const Knowhow& k, Shape s) {
    auto has = [&](Concept c) { return k.concepts.test(static_cast<int>(c)); };
    switch (s) {
    case Shape::Lump: case Shape::Shaft: case Shape::Cord: case Shape::Head: return true;
    case Shape::Flake: case Shape::Point: case Shape::Blade: return has(Concept::Sharpness);
    case Shape::Sheet: return has(Concept::Containment) || has(Concept::Insulation) || has(Concept::Buoyancy);
    case Shape::Bowl: return has(Concept::Containment);
    case Shape::Plank: return has(Concept::Buoyancy) || has(Concept::Leverage);
    case Shape::Wheel: return has(Concept::Rotation);
    case Shape::Limb: return has(Concept::Elasticity);
    case Shape::Pipe: return has(Concept::Pressure);
    case Shape::Wall: return has(Concept::Insulation);
    default: return false;
    }
}

Group* groupOfEntity(Simulation& sim, Entity e) {
    const Social* so = sim.reg.tryGet<Social>(e);
    return so ? sim.group(so->group) : nullptr;
}

std::string designName(Simulation& sim, Entity e, Purpose p, const std::vector<Part>& parts) {
    Group* g = groupOfEntity(sim, e);
    Mind& m = sim.reg.get<Mind>(e);
    std::string noun = purposeNoun(p);
    bool wheel = std::any_of(parts.begin(), parts.end(), [](const Part& x) { return x.shape == Shape::Wheel; });
    bool bowl = std::any_of(parts.begin(), parts.end(), [](const Part& x) { return x.shape == Shape::Bowl; });
    if (p == Purpose::Carry) noun = wheel ? "Xe" : (bowl ? "Vò" : "Túi");
    if (p == Purpose::Float) noun = bowl ? "Thuyền" : "Bè";
    if (p == Purpose::Ranged && !std::any_of(parts.begin(), parts.end(), [](const Part& x) { return x.shape == Shape::Limb; })) noun = "Ná";
    std::string word = g ? g->language.word(m.rng, 2) : std::string("Vô danh");
    return noun + " " + word;
}

} // namespace

Purpose mostFrustrated(const Knowhow& k) {
    int best = 0;
    for (int p = 1; p < kPurposeCount; ++p) if (k.frustration[p] > k.frustration[best]) best = p;
    return static_cast<Purpose>(best);
}

// Chọn mục đích để sáng tạo: ngẫu nhiên có trọng số theo bức xúc² (việc bức xúc nhất hay được nghĩ
// tới nhất, nhưng không bị "kẹt" mãi ở một việc bất khả thi)
Purpose pickInventPurpose(const Knowhow& k, Rng& rng) {
    float total = 0;
    for (float f : k.frustration) total += f * f;
    if (total <= 0) return mostFrustrated(k);
    float r = rng.uniform() * total;
    for (int p = 0; p < kPurposeCount; ++p) {
        r -= k.frustration[p] * k.frustration[p];
        if (r <= 0) return static_cast<Purpose>(p);
    }
    return mostFrustrated(k);
}

int availableMat(Simulation& sim, Entity e, MatId m) {
    int n = sim.reg.get<Inventory>(e).count(m);
    if (Group* g = groupOfEntity(sim, e)) n += g->matCount(m);
    return n;
}

bool consumeMat(Simulation& sim, Entity e, MatId m, int units) {
    if (availableMat(sim, e, m) < units) return false;
    int got = sim.reg.get<Inventory>(e).take(m, units);
    if (got < units) if (Group* g = groupOfEntity(sim, e)) g->takeMat(m, units - got);
    return true;
}

float heldValue(const Simulation& sim, Entity e, Purpose p) {
    const Inventory* inv = const_cast<Simulation&>(sim).reg.tryGet<Inventory>(e);
    if (!inv) return 0;
    float best = 0;
    for (int s = 0; s < kPurposeCount; ++s)
        if (inv->item[s] >= 0) best = std::max(best, sim.designs[inv->item[s]].value(p));
    return best;
}

Workshop workshopOf(Simulation& sim, Entity e) {
    Workshop ws;
    const Mind& m = sim.reg.get<Mind>(e);
    ws.cut = std::max(0.5f, m.know.bestValue(sim.designs, Purpose::Cut));
    ws.smash = std::max(1.0f, m.know.bestValue(sim.designs, Purpose::Smash));
    ws.heat = 20;
    if (m.know.bestValue(sim.designs, Purpose::FireStart) >= 260) ws.heat = 650; // tự nhóm được lửa trại
    if (Group* g = groupOfEntity(sim, e)) {
        if (g->hasHearth) ws.heat = std::max(ws.heat, 750.0f);
        if (g->forgeX >= 0) ws.heat = std::max(ws.heat, g->forgeHeat);
    }
    return ws;
}

void experience(Simulation& sim, Entity e, Concept c, float chance) {
    Mind* m = sim.reg.tryGet<Mind>(e);
    if (!m || m->know.concepts.test(static_cast<int>(c))) return;
    if (m->intellect < 0.25f) return;  // sinh vật bản năng không trừu tượng hoá được
    if (!m->rng.chance(chance * (0.3f + m->intellect) * sim.cfg.learningRate)) return;
    m->know.concepts.set(static_cast<int>(c));
    m->think("Ồ! Hiểu ra: %s (%s)", conceptName(c), scienceName(conceptField(c)));
    // Lần đầu tiên trên thế giới có ai hiểu ra khái niệm này → ghi vào biên niên sử
    if (!sim.conceptFirsts.test(static_cast<int>(c))) {
        sim.conceptFirsts.set(static_cast<int>(c));
        const Social& so = sim.reg.get<Social>(e);
        const Transform& tr = sim.reg.get<Transform>(e);
        Group* g = sim.group(so.group);
        sim.chronicle.add(sim.tick, EventKind::Discovery,
                          std::string("Khoa học (") + scienceName(conceptField(c)) + "): " + so.name +
                              (g ? " (" + g->displayName() + ")" : std::string()) + " lần đầu hiểu ra \"" + conceptName(c) + "\"",
                          static_cast<int>(tr.pos.x), static_cast<int>(tr.pos.y));
    }
}

bool conceiveIdea(Simulation& sim, Entity e, Purpose p) {
    Mind& m = sim.reg.get<Mind>(e);
    Knowhow& K = m.know;
    const MaterialRegistry& R = sim.world.materials;
    DesignRegistry& D = sim.designs;
    Workshop ws = workshopOf(sim, e);
    // Nghĩ bằng MỌI vật liệu rắn đã biết (kể cả chưa có trong tay — sẽ đi kiếm sau)
    std::vector<MatId> pool;
    for (MatId mt : K.materials) {
        const MaterialDef& d = R[mt];
        if (d.phaseAt(20) == Phase::Solid && d.nutrition < 0.5f) pool.push_back(mt);
    }
    K.lastInvent = sim.tick;
    sim.statsMut().inventAttempts++;
    if (pool.empty()) return false;
    int baseId = K.best[static_cast<int>(p)];
    float cur = baseId >= 0 ? D[baseId].value(p) : 0.0f;
    std::vector<Shape> shapes;
    for (int s = 0; s < kShapeCount; ++s) if (conceivable(K, static_cast<Shape>(s))) shapes.push_back(static_cast<Shape>(s));
    auto randMat = [&]() { return pool[m.rng.irange(0, static_cast<int>(pool.size()) - 1)]; };
    auto randShape = [&]() { return shapes[m.rng.irange(0, static_cast<int>(shapes.size()) - 1)]; };

    const int N = 4 + static_cast<int>(m.intellect * 14 + m.pers.openness * 4);
    std::vector<Part> bestParts;
    float bestEst = cur * 1.03f;
    int bestParent = -1;
    for (int i = 0; i < N; ++i) {
        std::vector<Part> parts;
        int parent = -1;
        float r = m.rng.uniform();
        if (baseId >= 0 && r < 0.6f) { parts = D[baseId].parts; parent = baseId; }                 // cải tiến cái đang có
        else if (!K.designs.empty() && r < 0.8f) {                                                    // loại suy từ thiết kế khác
            parent = K.designs[m.rng.irange(0, static_cast<int>(K.designs.size()) - 1)];
            parts = D[parent].parts;
        }
        int mutations = m.rng.irange(1, 3);
        for (int k = 0; k < mutations || parts.empty(); ++k) {
            int op = parts.empty() ? 2 : m.rng.irange(0, 4);
            switch (op) {
            case 0: parts[m.rng.irange(0, static_cast<int>(parts.size()) - 1)].mat = randMat(); break;
            case 1: { Part& pt = parts[m.rng.irange(0, static_cast<int>(parts.size()) - 1)]; pt.size = std::clamp(pt.size * m.rng.range(0.7f, 1.5f), 0.3f, 3.0f); break; }
            case 2: if (parts.size() < 6) parts.push_back(Part{randMat(), randShape(), m.rng.range(0.5f, 1.5f)}); break;
            case 3: if (parts.size() > 1) parts.erase(parts.begin() + m.rng.irange(0, static_cast<int>(parts.size()) - 1)); break;
            case 4: parts[m.rng.irange(0, static_cast<int>(parts.size()) - 1)].shape = randShape(); break;
            }
            if (k > 6) break;
        }
        bool ok = !parts.empty();
        for (const Part& pt : parts) ok = ok && canFabricate(R, pt, ws) && conceivable(K, pt.shape);
        if (!ok) continue;
        DesignProps props = evaluateDesign(R, parts);
        float q = parent >= 0 ? D[parent].quality : 1.0f;
        float trueVal = props.v[static_cast<int>(p)] * q;
        // Mô hình tinh thần: kẻ kém thông minh ước lượng sai nhiều hơn
        float est = trueVal * (1.0f + m.rng.gauss((1.0f - m.intellect) * 0.35f));
        est *= 1.0f - 0.04f * parts.size();   // ưu tiên phương án gọn (ít bộ phận thừa, dễ làm)
        if (est > bestEst) { bestEst = est; bestParts = parts; bestParent = parent; }
    }
    if (bestParts.empty()) {
        m.think("Nghĩ mãi chưa ra cách %s tốt hơn", purposeName(p));
        experience(sim, e, conceptForPurpose(p), 0.08f);
        K.frustration[static_cast<int>(p)] *= 0.5f;   // tạm gác lại, nghĩ việc khác
        return false;
    }
    m.idea = bestParts;
    m.ideaPurpose = p;
    m.ideaParent = bestParent;
    m.hasIdea = true;
    sim.statsMut().inventTried++;
    std::string recipe;
    for (const Part& pt : bestParts) { if (!recipe.empty()) recipe += " + "; recipe += R[pt.mat].name + " " + shapeName(pt.shape); }
    m.think("Ý tưởng: %s", recipe.c_str());
    return true;
}

MatId missingForIdea(Simulation& sim, Entity e) {
    Mind& m = sim.reg.get<Mind>(e);
    const MaterialRegistry& R = sim.world.materials;
    std::vector<std::pair<MatId, int>> need;
    for (const Part& pt : m.idea) {
        int c = partCost(R, pt);
        auto it = std::find_if(need.begin(), need.end(), [&](auto& x) { return x.first == pt.mat; });
        if (it == need.end()) need.push_back({pt.mat, c}); else it->second += c;
    }
    for (auto& [mt, c] : need) if (availableMat(sim, e, mt) < c) return mt;
    return kNoMat;
}

InventResult realizeIdea(Simulation& sim, Entity e) {
    InventResult res;
    Mind& m = sim.reg.get<Mind>(e);
    Knowhow& K = m.know;
    const MaterialRegistry& R = sim.world.materials;
    DesignRegistry& D = sim.designs;
    if (!m.hasIdea) return res;
    m.hasIdea = false;
    Purpose p = m.ideaPurpose;
    std::vector<Part> bestParts = m.idea;
    int bestParent = m.ideaParent;
    int baseId = K.best[static_cast<int>(p)];
    float cur = baseId >= 0 ? D[baseId].value(p) : 0.0f;
    res.before = cur;
    res.tried = true;
    Workshop ws = workshopOf(sim, e);
    for (const Part& pt : bestParts) if (!canFabricate(R, pt, ws)) return res; // điều kiện đã thay đổi
    // ---- Làm nguyên mẫu: tiêu tốn vật liệu thật ----
    std::vector<std::pair<MatId, int>> need;
    for (const Part& pt : bestParts) {
        int c = partCost(R, pt);
        auto it = std::find_if(need.begin(), need.end(), [&](auto& x) { return x.first == pt.mat; });
        if (it == need.end()) need.push_back({pt.mat, c}); else it->second += c;
    }
    for (auto& [mt, c] : need)
        if (availableMat(sim, e, mt) < c) {
            m.think("Thiếu %s để làm thử %s", R[mt].name.c_str(), purposeName(p));
            K.frustration[static_cast<int>(p)] *= 0.8f;
            return res;
        }
    for (auto& [mt, c] : need) consumeMat(sim, e, mt, c);
    // ---- Thử nghiệm thật ----
    DesignProps props = evaluateDesign(R, bestParts);
    float skill = m.skillOf(Skill::Crafting);
    float q = bestParent >= 0 ? sim.designs[bestParent].quality : 0.8f + 0.4f * skill;
    q *= 1.0f + 0.04f * m.intellect * (0.5f + skill) + m.rng.range(0.0f, 0.02f); // tay nghề tích luỹ — không có trần
    float trueVal = props.v[static_cast<int>(p)] * q;
    res.after = trueVal;
    m.skill[static_cast<int>(Skill::Crafting)] = std::min(1.0f, skill + 0.02f);
    if (trueVal <= cur) {
        m.think("Thử %s thất bại (%.1f ≤ %.1f) — rút kinh nghiệm", purposeName(p), trueVal, cur);
        experience(sim, e, conceptForPurpose(p), 0.25f);  // thất bại cũng dạy điều gì đó
        K.frustrate(p, -0.2f);
        return res;
    }
    // ---- Thành công: ghi nhận thiết kế ----
    int id = D.findSame(bestParts, p);
    bool brandNew = id < 0;
    int prevWorldBest = D.worldBest[static_cast<int>(p)];
    float prevWorld = prevWorldBest >= 0 ? D[prevWorldBest].value(p) : 0.0f;
    if (brandNew) {
        Design d;
        d.parts = bestParts;
        d.props = props;
        d.quality = q;
        d.primary = p;
        d.parent = bestParent;
        d.generation = bestParent >= 0 ? D[bestParent].generation + 1 : 0;
        const Social& so = sim.reg.get<Social>(e);
        d.group = so.group;
        d.inventor = so.name;
        d.tick = sim.tick;
        // Vật liệu chủ đạo = vật liệu của bộ phận "làm việc" nặng ký nhất về độ cứng
        float bestA = -1;
        for (const Part& pt : bestParts) {
            float a = R.advancement(pt.mat) * (pt.shape == Shape::Shaft || pt.shape == Shape::Cord ? 0.3f : 1.0f);
            if (a > bestA) { bestA = a; d.mainMat = pt.mat; }
        }
        d.name = designName(sim, e, p, bestParts);
        id = D.add(d);
    }
    K.learnDesign(D, id);
    K.inventions++;
    K.frustration[static_cast<int>(p)] *= 0.4f;
    Inventory& inv = sim.reg.get<Inventory>(e);
    inv.item[static_cast<int>(p)] = static_cast<int16_t>(id);
    inv.wear[static_cast<int>(p)] = 255;
    res.improved = true;
    res.design = id;
    sim.statsMut().inventSuccess++;
    m.learned = std::min(0.25f, m.learned + 0.004f);
    const Design& nd = D[id];
    m.think("Phát minh: %s (%s %.1f)", nd.name.c_str(), purposeName(p), nd.value(p));
    // Biên niên sử: phát minh đầu tiên cho mục đích, hoặc kỷ lục thế giới vượt ≥ 25%
    bool record = D.worldBest[static_cast<int>(p)] == id && (prevWorldBest < 0 || nd.value(p) > prevWorld * 1.25f);
    if (brandNew && record) {
        const Social& so = sim.reg.get<Social>(e);
        Group* g = sim.group(so.group);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f", nd.value(p));
        const Transform& tr = sim.reg.get<Transform>(e);
        sim.chronicle.add(sim.tick, EventKind::Discovery,
                          so.name + (g ? " (" + g->displayName() + ")" : std::string()) + " chế ra \"" + nd.name + "\" = " +
                              nd.recipe(R) + " → " + purposeName(p) + " " + buf + (prevWorldBest < 0 ? " (lần đầu trên thế giới)" : " (kỷ lục mới)"),
                          static_cast<int>(tr.pos.x), static_cast<int>(tr.pos.y));
    }
    // Hiểu thêm khái niệm nhờ thành công (vd. cán dài giúp chặt mạnh hơn → hiểu đòn bẩy)
    for (const Part& pt : bestParts) {
        if (pt.shape == Shape::Shaft) experience(sim, e, Concept::Leverage, 0.3f);
        if (pt.shape == Shape::Wheel) experience(sim, e, Concept::Rotation, 0.3f);
    }
    return res;
}

MatId missingForDesign(Simulation& sim, Entity e, int designId) {
    const MaterialRegistry& R = sim.world.materials;
    std::vector<std::pair<MatId, int>> need;
    for (const Part& pt : sim.designs[designId].parts) {
        int c = partCost(R, pt);
        auto it = std::find_if(need.begin(), need.end(), [&](auto& x) { return x.first == pt.mat; });
        if (it == need.end()) need.push_back({pt.mat, c}); else it->second += c;
    }
    for (auto& [mt, c] : need) if (availableMat(sim, e, mt) < c) return mt;
    return kNoMat;
}

bool craftDesign(Simulation& sim, Entity e, int designId, Purpose slot) {
    if (designId < 0) return false;
    const Design& d = sim.designs[designId];
    const MaterialRegistry& R = sim.world.materials;
    Workshop ws = workshopOf(sim, e);
    for (const Part& pt : d.parts) if (!canFabricate(R, pt, ws)) return false;
    std::vector<std::pair<MatId, int>> need;
    for (const Part& pt : d.parts) {
        int c = partCost(R, pt);
        auto it = std::find_if(need.begin(), need.end(), [&](auto& x) { return x.first == pt.mat; });
        if (it == need.end()) need.push_back({pt.mat, c}); else it->second += c;
    }
    for (auto& [mt, c] : need) if (availableMat(sim, e, mt) < c) return false;
    for (auto& [mt, c] : need) consumeMat(sim, e, mt, c);
    Inventory& inv = sim.reg.get<Inventory>(e);
    inv.item[static_cast<int>(slot)] = static_cast<int16_t>(designId);
    inv.wear[static_cast<int>(slot)] = 255;
    return true;
}

void observeReaction(Simulation& sim, Entity e, const ReactionEvent& ev) {
    Mind& m = sim.reg.get<Mind>(e);
    if (m.intellect < 0.2f) return; // bản năng: chỉ sợ lửa, không rút ra quy luật
    // 1) Khái niệm vật lý
    switch (ev.kind) {
    case ReactionKind::Combustion: case ReactionKind::Pyrolysis: case ReactionKind::Firing: experience(sim, e, Concept::Combustion, 0.3f); break;
    case ReactionKind::Melting: case ReactionKind::Solidifying: case ReactionKind::Vitrification: case ReactionKind::Quench:
    case ReactionKind::Freezing: case ReactionKind::Thawing: experience(sim, e, Concept::Melting, 0.25f); break;
    case ReactionKind::Smelting: experience(sim, e, Concept::Reduction, 0.4f); break;
    case ReactionKind::Alloying: experience(sim, e, Concept::Alloying, 0.5f); break;
    case ReactionKind::Explosion: case ReactionKind::Boiling: experience(sim, e, Concept::Pressure, 0.35f); break;
    case ReactionKind::Neutralization: case ReactionKind::AcidMetal: experience(sim, e, Concept::AcidBase, 0.4f); break;
    case ReactionKind::Decay: experience(sim, e, Concept::Contagion, 0.01f); break;   // xác thối bốc mùi, ruồi nhặng
    case ReactionKind::Precipitation: experience(sim, e, Concept::Preservation, 0.05f); break;   // muối kết tinh
    default: break;
    }
    // 2) Vật liệu mới thấy (sản phẩm) → biết đến sự tồn tại của nó
    for (MatId o : ev.out)
        if (o != kNoMat && sim.world.materials[o].phaseAt(20) == Phase::Solid && m.rng.chance(0.5f)) m.know.learnMat(o);
    // 3) Quy trình: hiểu "làm thế nào để tạo ra nó" — cần trí tuệ & đôi tay để tái tạo
    const Body& b = sim.reg.get<Body>(e);
    if (!sim.species[b.species].dexterous || m.intellect < 0.33f) return;
    if (!m.rng.chance(0.2f + m.intellect * 0.5f)) return;
    const Social& so = sim.reg.get<Social>(e);
    bool isNew = false;
    int pid = sim.processes.fromEvent(ev, sim.world.materials, so.group, sim.tick, isNew);
    if (pid < 0 || m.know.knowsProcess(pid)) return;
    m.know.learnProcess(pid);
    for (MatId in : sim.processes[pid].inputs) m.know.learnMat(in);
    m.think("Hiểu ra quy trình: %s", sim.processes[pid].name.c_str());
    if (isNew) {
        Group* g = sim.group(so.group);
        sim.chronicle.add(sim.tick, EventKind::Discovery,
                          so.name + (g ? " (" + g->displayName() + ")" : std::string()) + " quan sát và hiểu ra quy trình mới: " +
                              sim.processes[pid].name,
                          ev.x, ev.y);
    }
}

void observePhysEvent(Simulation& sim, Entity e, int kind, MatId m) {
    switch (static_cast<PhysEventKind>(kind)) {
    case PhysEventKind::Float: experience(sim, e, Concept::Buoyancy, 0.35f);
        if (Mind* mm = sim.reg.tryGet<Mind>(e)) mm->know.learnMat(m);
        break;
    case PhysEventKind::Sink: experience(sim, e, Concept::Buoyancy, 0.1f); experience(sim, e, Concept::Gravity, 0.05f); break;
    case PhysEventKind::Roll: experience(sim, e, Concept::Rotation, 0.3f); experience(sim, e, Concept::Gravity, 0.05f); break;
    case PhysEventKind::Blast: experience(sim, e, Concept::Pressure, 0.4f); break;
    case PhysEventKind::Bounce: experience(sim, e, Concept::Elasticity, 0.15f); break;
    case PhysEventKind::Impact: experience(sim, e, Concept::Sharpness, 0.05f); experience(sim, e, Concept::Gravity, 0.1f); break;
    default: break;
    }
}

void shareKnowhow(Simulation& sim, Entity teacher, Entity learner, bool language) {
    Mind& t = sim.reg.get<Mind>(teacher);
    Mind* l = sim.reg.tryGet<Mind>(learner);
    if (!l || l->intellect < 0.2f) return;
    float p = (language ? 0.5f : 0.15f) * sim.cfg.learningRate;
    // Khái niệm
    for (int c = 0; c < kConceptCount; ++c)
        if (t.know.concepts.test(c) && !l->know.concepts.test(c) && t.rng.chance(p * 0.5f)) { l->know.concepts.set(c); break; }
    // Thiết kế: dạy cái mình giỏi nhất mà người kia làm kém hơn
    int start = t.rng.irange(0, kPurposeCount - 1);
    for (int k = 0; k < kPurposeCount; ++k) {
        int pi = (start + k) % kPurposeCount;
        int d = t.know.best[pi];
        if (d < 0) continue;
        Purpose pp = static_cast<Purpose>(pi);
        if (l->know.bestValue(sim.designs, pp) >= sim.designs[d].value(pp)) continue;
        if (t.rng.chance(p)) l->know.learnDesign(sim.designs, d);
        break;
    }
    // Quy trình
    if (!t.know.processes.empty() && t.rng.chance(p * 0.6f)) {
        int pid = t.know.processes[t.rng.irange(0, static_cast<int>(t.know.processes.size()) - 1)];
        l->know.learnProcess(pid);
    }
    // Vật liệu
    if (!t.know.materials.empty() && t.rng.chance(p)) l->know.learnMat(t.know.materials[t.rng.irange(0, static_cast<int>(t.know.materials.size()) - 1)]);
}

void inheritKnowhow(Simulation& sim, Entity child, Entity parent) {
    Mind* c = sim.reg.tryGet<Mind>(child);
    Mind* pm = sim.reg.tryGet<Mind>(parent);
    if (!c || !pm) return;
    for (int k = 0; k < kConceptCount; ++k) if (pm->know.concepts.test(k) && c->rng.chance(0.5f)) c->know.concepts.set(k);
    for (MatId mt : pm->know.materials) if (c->rng.chance(0.6f)) c->know.learnMat(mt);
    for (int d : pm->know.designs) if (c->rng.chance(0.35f)) c->know.learnDesign(sim.designs, d);
    for (int pr : pm->know.processes) if (c->rng.chance(0.3f)) c->know.learnProcess(pr);
}

} // namespace wb
