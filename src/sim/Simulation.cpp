// =============================================================================
// Simulation.cpp — Vòng lặp mô phỏng, sinh tử, trao đổi chất, LOD.
// =============================================================================
#include "Simulation.h"
#include "../ai/Brain.h"
#include "../civ/SocietySystem.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace wb {

const char* roleName(Role r) {
    static const char* n[] = {"Không", "Thủ lĩnh", "Trưởng lão", "Thợ săn", "Hái lượm", "Thợ xây", "Nông dân",
                              "Binh lính", "Thợ thủ công", "Tư tế", "Trẻ con"};
    return n[static_cast<int>(r)];
}

Simulation::Simulation(const SimConfig& c) { reset(c); }
Simulation::~Simulation() = default;

void Simulation::reset(const SimConfig& c) {
    cfg = c;
    pool_ = std::make_unique<ThreadPool>(cfg.threads);
    society_ = std::make_unique<SocietySystem>(*this);
    reg = Registry();
    // Tạo sẵn mọi pool component trước khi chạy song song (pool<T>() tạo lười → không an toàn đa luồng)
    reg.pool<Transform>(); reg.pool<Body>(); reg.pool<Inventory>(); reg.pool<Social>(); reg.pool<GenomeC>(); reg.pool<Mind>();
    groups.clear();
    agents.clear();
    dying_.clear();
    killEvents.clear();
    chronicle.clear();
    worldFirsts.reset();
    designs.clear();
    processes.clear();
    physics.clear();
    tick = 0;
    stats_ = SimStats{};
    rng.reseed(cfg.seed);
    world.generate(cfg.width, cfg.height, cfg.seed, cfg.worldType);
    grid.init(cfg.width, cfg.height, 8, true);   // hành tinh quấn vòng đông–tây
    species.initDefaults();
    speciesLang_.clear();
    for (int i = 0; i < species.count(); ++i)
        speciesLang_.push_back(Language::generate(rng, species[i].baseGenome[Gene::Aggression]));
}

int Simulation::aliveGroupCount() const {
    int n = 0;
    for (auto& g : groups) n += g.alive;
    return n;
}

std::string Simulation::dateString() const {
    long year = tick / kTicksPerYear;
    int month = static_cast<int>((tick % kTicksPerYear) * 12 / kTicksPerYear) + 1;
    return "Năm " + std::to_string(year) + ", tháng " + std::to_string(month);
}

bool Simulation::findSpawnTile(int& x, int& y, int radius, int sp) {
    int ox, oy;
    auto ok = [&](int tx, int ty) { return sp < 0 ? world.walkable(tx, ty) : habitable(sp, tx, ty); };
    if (world.inBounds(x, y) && ok(x, y)) return true;
    if (world.findNearestTile(x, y, radius, [&](const Tile&, int tx, int ty) { return ok(tx, ty); }, ox, oy)) {
        x = ox; y = oy; return true;
    }
    return false;
}

bool Simulation::habitable(int sp, int x, int y) const {
    if (!world.inBounds(x, y)) return false;
    switch (species[sp].habitat) {
    case Habitat::Water: return world.isWater(x, y);
    case Habitat::Air: return world.at(x, y).terrain != Terrain::Lava;   // bay qua mọi thứ trừ miệng núi lửa
    case Habitat::Cave: return world.caveOpen(x, y);
    case Habitat::Amphibious: return world.walkable(x, y) || world.isWater(x, y);
    default: return world.walkable(x, y);
    }
}

int Simulation::foodAt(int sp, int x, int y) const {
    if (!habitable(sp, x, y)) return 0;
    if (species[sp].habitat == Habitat::Cave) return world.under(x, y).fungus;
    return world.at(x, y).food;
}

void Simulation::eatFoodAt(int sp, int x, int y, int amount) {
    if (!world.inBounds(x, y)) return;
    uint8_t& f = species[sp].habitat == Habitat::Cave ? world.under(x, y).fungus : world.at(x, y).food;
    f = static_cast<uint8_t>(std::max(0, f - amount));
}

Entity Simulation::spawnCreature(int sp, Vec2 pos, const Genome* genome, Entity mother, Entity father) {
    if (static_cast<int>(agents.size()) >= cfg.maxPopulation) return NullEntity;
    pos = wrapPos(pos);
    int tx = static_cast<int>(pos.x), ty = static_cast<int>(pos.y);
    if (!findSpawnTile(tx, ty, species[sp].habitat == Habitat::Cave ? 14 : 6, sp)) return NullEntity;
    if (!habitable(sp, static_cast<int>(pos.x), static_cast<int>(pos.y))) pos = {tx + 0.5f, ty + 0.5f};
    const SpeciesDef& sd = species[sp];
    Entity e = reg.create();
    Genome g;
    if (genome) g = *genome;
    else {
        g = sd.baseGenome;
        for (float& v : g.g) v = clamp01(v + rng.gauss(0.05f));
    }
    auto& tr = reg.emplace<Transform>(e);
    tr.pos = pos; tr.target = pos;
    auto& b = reg.emplace<Body>(e);
    b.species = sp;
    b.female = rng.chance(0.5f);
    float sz = sd.size;
    float bodySize = 0.7f + g[Gene::Size] * 0.6f;   // gen kích thước (tiến hoá được)
    b.maxHp = sd.baseHp * (0.6f + g[Gene::Strength] * 0.8f) * bodySize;
    b.hp = b.maxHp;
    b.damage = sd.baseDamage * (0.6f + g[Gene::Strength] * 0.8f) * bodySize;
    b.speed = sd.baseSpeed * (0.6f + g[Gene::Speed] * 0.8f);
    (void)sz;
    if (mother == NullEntity) b.ageTicks = static_cast<long>(sd.adultYears * kTicksPerYear * rng.range(1.0f, 2.0f)); // sinh vật được "thả" là con trưởng thành
    b.hunger = rng.range(0.1f, 0.3f);
    reg.emplace<Inventory>(e);
    reg.emplace<GenomeC>(e).genome = g;
    auto& so = reg.emplace<Social>(e);
    so.mother = mother; so.father = father;
    // Đặt tên theo ngôn ngữ nhóm của mẹ (văn hoá) hoặc ngôn ngữ mặc định của loài
    const Social* ms = reg.tryGet<Social>(mother);
    Group* mg = ms ? group(ms->group) : nullptr;
    so.name = (mg ? mg->language : speciesLang_[std::min<int>(sp, static_cast<int>(speciesLang_.size()) - 1)]).word(rng);
    auto& m = reg.emplace<Mind>(e);
    Brain::initMind(*this, e, m, g, mother, father);
    Brain::updateIntellect(*this, e);
    agents.push_back(e);
    if (mg) joinGroup(e, mg->id);
    if (mother != NullEntity) {
        stats_.births++;
        if (auto* ps = reg.tryGet<Social>(mother)) ps->children++;
    }
    return e;
}

void Simulation::kill(Entity e, int cause) {
    Body* b = reg.tryGet<Body>(e);
    if (!b || b->dead) return;
    b->dead = true;
    b->deathCause = cause;
    if (std::getenv("WB_DEBUG_DEATH") && cause == 3 && species[b->species].dexterous) {
        const Transform& t = reg.get<Transform>(e);
        int x = static_cast<int>(t.pos.x), y = static_cast<int>(t.pos.y);
        const MatterCell& c = world.matter.cell(x, y);
        std::fprintf(stderr, "DEATH tick %ld at %d,%d T=%.0f gas=%s/%d fire=%d terrain=%s bld=%d expl=%zu\n", tick, x, y, world.matter.temp(x, y),
                     world.materials[c.gas.mat].name.c_str(), c.gas.amount, world.at(x, y).fire, terrainInfo(world.at(x, y).terrain).name,
                     static_cast<int>(world.at(x, y).building), world.matter.explosions().size());
    }
    dying_.push_back(e);
}

int Simulation::createGroup(Entity founder) {
    Group g;
    g.id = static_cast<int>(groups.size());
    const Body& b = reg.get<Body>(founder);
    g.species = b.species;
    g.animal = !species[b.species].dexterous;
    g.language = Language::generate(rng, species[b.species].baseGenome[Gene::Aggression]);
    g.name = g.language.word(rng, 2);
    uint32_t h = hash32(static_cast<uint32_t>(g.id) * 2654435761u + cfg.seed);
    g.colR = static_cast<uint8_t>(60 + (h & 0xBF)); g.colG = static_cast<uint8_t>(60 + ((h >> 8) & 0xBF)); g.colB = static_cast<uint8_t>(60 + ((h >> 16) & 0xBF));
    g.symbol = static_cast<uint8_t>((h >> 24) & 7);
    const Transform& tr = reg.get<Transform>(founder);
    g.homeX = static_cast<int>(tr.pos.x); g.homeY = static_cast<int>(tr.pos.y);
    g.leader = founder;
    g.foundedTick = g.stageTick = tick;
    const Mind& m = reg.get<Mind>(founder);
    g.culture.militarism = (1 - m.pers.agreeableness) * 0.6f + m.pers.ambition * 0.4f;
    g.culture.curiosity = m.pers.openness;
    g.culture.collectivism = m.pers.extraversion;
    g.culture.xenophobia = (1 - m.pers.agreeableness) * 0.5f + m.pers.fearfulness * 0.5f;
    g.culture.tradition = m.pers.diligence;
    g.knowledge = m.known;
    g.lastSplitCheck = tick;
    groups.push_back(g);
    joinGroup(founder, g.id);
    reg.get<Social>(founder).role = Role::Leader;
    return g.id;
}

void Simulation::joinGroup(Entity e, int gid) {
    Social& so = reg.get<Social>(e);
    if (so.group == gid) return;
    if (so.group >= 0) leaveGroup(e);
    Group* g = group(gid);
    if (!g) return;
    so.group = gid;
    so.joinedTick = tick;
    so.role = Role::None;
    g->members.push_back(e);
}

void Simulation::leaveGroup(Entity e) {
    Social& so = reg.get<Social>(e);
    if (so.group < 0) return;
    Group& g = groups[so.group];
    g.members.erase(std::remove(g.members.begin(), g.members.end(), e), g.members.end());
    if (g.leader == e) g.leader = NullEntity;
    so.group = -1;
    so.role = Role::None;
}

Group* Simulation::groupOf(Entity e) {
    const Social* so = reg.tryGet<Social>(e);
    return so ? group(so->group) : nullptr;
}

void Simulation::rebuildGrid() {
    gridItems_.clear();
    auto& tp = reg.pool<Transform>();
    for (size_t i = 0; i < tp.size(); ++i) gridItems_.push_back({tp.entities[i], tp.data[i].pos});
    grid.rebuild(gridItems_);
}

void Simulation::metabolism() {
    std::vector<std::pair<Entity, int>> births;
    float popPressure = agents.size() > static_cast<size_t>(cfg.maxPopulation * 0.6f) ? 2.0f : 1.0f;
    for (Entity e : agents) {
        Body& b = reg.get<Body>(e);
        if (b.dead) continue;
        Mind& m = reg.get<Mind>(e);
        const Genome& g = reg.get<GenomeC>(e).genome;
        const SpeciesDef& sd = species[b.species];
        Transform& tr = reg.get<Transform>(e);
        Social& so = reg.get<Social>(e);
        b.ageTicks++;
        // Già yếu: tuổi thọ phụ thuộc gen tuổi thọ
        float maxAge = sd.maxAgeYears * (0.7f + g[Gene::Longevity] * 0.6f) * kTicksPerYear;
        if (b.ageTicks > maxAge && rng.chance(0.002f)) { kill(e, 0); continue; }
        // Đói & mệt
        b.hunger += (b.sleeping ? 0.0005f : 0.0010f) * (0.8f + sd.size * 0.2f) * (0.7f + g[Gene::Size] * 0.6f) * popPressure;
        if (!b.sleeping) b.fatigue += 0.0007f;
        b.fatigue = std::min(b.fatigue, 1.0f);
        if (b.hunger >= 1.0f) { b.hunger = 1.0f; b.hp -= 0.4f; if (b.hp <= 0) { kill(e, 1); continue; } }
        else if (b.hunger < 0.6f && b.hp < b.maxHp) b.hp = std::min(b.maxHp, b.hp + 0.03f);
        // Nhiệt độ THẬT (°C) từ lớp vật chất: lạnh, nóng, bỏng lửa, khí độc
        if ((b.ageTicks + entityIndex(e)) % 10 == 0) {
            int x = static_cast<int>(tr.pos.x), y = static_cast<int>(tr.pos.y);
            float t = sd.habitat == Habitat::Cave ? 13.0f : world.temperatureAt(x, y);   // lòng đất: nhiệt độ ổn định quanh năm
            float coldLimit = 8.0f - g[Gene::ColdResist] * 35.0f;   // lông dày / mỡ → chịu lạnh tốt
            float heatLimit = 34.0f + g[Gene::HeatResist] * 20.0f;
            float stress = std::max(0.0f, coldLimit - t) / 15.0f + std::max(0.0f, t - heatLimit) / 15.0f;
            // Quần áo (thiết kế "giữ ấm") cách nhiệt cơ thể
            if (t < coldLimit) stress /= 1.0f + heldValue(*this, e, Purpose::Warmth) * 0.15f;
            const Tile& here = world.at(x, y);
            if (here.building != Building::None && buildingInfo(here.building).housing > 0) stress = 0;
            if (Group* hg = group(so.group); hg && hg->hasHearth && t < coldLimit &&
                std::abs(hg->hearthX - x) + std::abs(hg->hearthY - y) <= 4) stress *= 0.2f;   // sưởi bên bếp lửa
            b.cold = clamp01(b.cold + stress * 0.05f - 0.02f);
            if (b.cold > 0.95f) { b.hp -= 1.5f; if (b.hp <= 0) { kill(e, 4); continue; } }
            // Bỏng: đứng trong lửa / dung nham / hơi nóng
            if (t > 70) {
                b.hp -= (t - 70) * 0.012f;
                b.sleeping = false;
                m.needReplan = true;
                m.nextThink = tick;          // phản xạ tức thời: bỏng → nghĩ ngay để bỏ chạy
                if (b.hp <= 0) { kill(e, 3); continue; }
            }
            // Khí độc (SO₂, axit bay hơi...) — liều lượng × độc tính
            const MatterSlot& gs = world.matter.cell(x, y).gas;
            if (!gs.empty()) {
                float tox = world.materials[gs.mat].toxicity * gs.amount * 0.02f;
                if (tox > 0) { b.hp -= tox; if (b.hp <= 0) { kill(e, 3); continue; } }
            }
        }
        // Ngoại lực (nổ, bị đạn bắn trúng) đẩy cơ thể đi, ma sát làm tắt dần
        if (tr.push.len2() > 1e-4f) {
            Vec2 np = tr.pos + tr.push;
            if (habitable(b.species, static_cast<int>(std::floor(np.x)), static_cast<int>(np.y))) tr.pos = wrapPos(np);
            else tr.push = Vec2{};
            tr.push = tr.push * 0.8f;
        }
        if (b.reproCooldown > 0) b.reproCooldown--;
        if (b.lastAttacker != NullEntity && rng.chance(0.01f)) b.lastAttacker = NullEntity;
        // Mang thai → sinh con
        if (b.pregnancyTicks > 0 && --b.pregnancyTicks == 0) births.push_back({e, rng.irange(1, sd.litterMax)});
        // LOD: tần suất suy nghĩ
        int interval = 5;
        if (!inFocus(tr.pos)) interval = 12;
        if (so.role == Role::Leader) interval = 4;
        if (m.intellect < 0.15f) interval += 3;      // sinh vật bản năng không cần nghĩ nhiều
        if (agents.size() > 2500) interval += 3;
        m.thinkInterval = interval;
    }
    for (auto [mother, n] : births) {
        // Lưu ý: spawnCreature thêm component → các mảng pool có thể cấp phát lại,
        // nên KHÔNG giữ tham chiếu tới component qua lời gọi đó (sao chép giá trị).
        if (reg.get<Body>(mother).dead) continue;
        const int sp = reg.get<Body>(mother).species;
        const Genome mg = reg.get<GenomeC>(mother).genome;
        Entity father = reg.get<Body>(mother).pregnantBy;
        if (!reg.valid(father)) father = NullEntity;
        const Genome fg = father != NullEntity ? reg.get<GenomeC>(father).genome : mg;
        const Vec2 p = reg.get<Transform>(mother).pos;
        for (int k = 0; k < n; ++k) {
            Genome child = Genome::crossover(mg, fg, rng, cfg.mutationRate);
            spawnCreature(sp, {p.x + rng.range(-0.5f, 0.5f), p.y + rng.range(-0.5f, 0.5f)}, &child, mother, father);
        }
        Body& mb = reg.get<Body>(mother);
        // Nuôi con trước khi sinh lứa mới: loài trưởng thành chậm thì giãn cách sinh dài hơn
        mb.reproCooldown = species[sp].gestationTicks + species[sp].adultYears * kTicksPerYear / 8;
        mb.hunger = std::min(1.0f, mb.hunger + 0.15f);
    }
}

void Simulation::cleanupDead() {
    if (dying_.empty()) return;
    for (Entity e : dying_) {
        if (!reg.valid(e)) continue;
        const Body& b = reg.get<Body>(e);
        const Transform& tr = reg.get<Transform>(e);
        const Inventory& inv = reg.get<Inventory>(e);
        int x = static_cast<int>(tr.pos.x), y = static_cast<int>(tr.pos.y);
        // Để lại xác (thức ăn cho kẻ ăn xác) và công cụ (tri thức cho kẻ tò mò)
        world.dropItem(x, y, ItemKind::Corpse, static_cast<int>(species[b.species].size * 5) + 1);
        // Công cụ của người chết rơi lại — kẻ khác nhặt được có thể "học ngược" thiết kế
        for (int s = 0; s < kPurposeCount; ++s)
            if (inv.item[s] >= 0) world.dropItem(x, y, ItemKind::OldTool, 1, inv.item[s]);
        world.matter.addMatter(x, y, mat::Blood, 2, 37);
        if (Group* g = groupOf(e)) { g->food += inv.food / 2; }
        leaveGroup(e);
        stats_.deaths++;
        if (species[b.species].dexterous) stats_.sapientDeaths[std::clamp(b.deathCause, 0, 4)]++;
        else if (species[b.species].diet == Diet::Carnivore) stats_.carnivoreDeaths[std::clamp(b.deathCause, 0, 4)]++;
        reg.destroy(e);
    }
    dying_.clear();
    agents.erase(std::remove_if(agents.begin(), agents.end(), [&](Entity e) { return !reg.valid(e); }), agents.end());
}

void Simulation::step() {
    ++tick;
    world.update(tick, rng);
    rebuildGrid();

    // ---- THINK (song song): chỉ các agent đến lượt — batch + LOD ----
    thinkList_.clear();
    for (Entity e : agents) {
        Mind& m = reg.get<Mind>(e);
        if (m.nextThink <= tick) {
            thinkList_.push_back(e);
            m.nextThink = tick + m.thinkInterval;
        }
    }
    pool_->parallelFor(static_cast<int>(thinkList_.size()), [&](int i) { Brain::think(*this, thinkList_[i]); });
    stats_.thinksLastTick = static_cast<int>(thinkList_.size());

    // ---- ACT (tuần tự) ----
    size_t n = agents.size();
    for (size_t i = 0; i < n; ++i) Brain::act(*this, agents[i]);

    // ---- Vật lý: vật thể bay/lăn, rồi xử lý các vụ nổ do hoá học sinh ra ----
    physics.step(*this);
    for (const Explosion& ex : world.matter.explosions())
        physics.blast(*this, {ex.x + 0.5f, ex.y + 0.5f}, ex.energy);
    world.matter.explosions().clear();

    metabolism();
    society_->update();
    cleanupDead();

    // ---- Thống kê ----
    for (auto& s : species.all()) s.population = 0;
    float intel = 0;
    for (Entity e : agents) {
        species[reg.get<Body>(e).species].population++;
        intel += reg.get<Mind>(e).intellect;
    }
    stats_.population = static_cast<int>(agents.size());
    stats_.wildPopulation = 0;
    for (auto& s : species.all()) if (!s.dexterous) stats_.wildPopulation += s.population;
    stats_.avgIntellect = agents.empty() ? 0 : intel / agents.size();
    stats_.groups = 0; stats_.nations = 0;
    for (auto& g : groups) if (g.alive) { stats_.groups++; stats_.nations += g.stage == Stage::Nation; }
    stats_.species = 0;
    for (auto& s : species.all()) stats_.species += s.population > 0;
}

} // namespace wb
