// =============================================================================
// SocietySystem.cpp — Hình thành và tiến hoá các nền văn minh.
// =============================================================================
#include "SocietySystem.h"
#include "../sim/Simulation.h"
#include "../ai/Brain.h"
#include <algorithm>
#include <cmath>

namespace wb {

namespace {
bool knows(const Group& g, Tech t) { return g.knowledge.test(ti(t)); }
int territoryBase(Stage s) {
    static const int r[] = {4, 7, 9, 12, 15, 18};
    return r[static_cast<int>(s)];
}
int splitCap(Stage s) {
    static const int c[] = {14, 30, 45, 70, 120, 99999};
    return c[static_cast<int>(s)];
}
} // namespace

void SocietySystem::update() {
    long t = sim_.tick;
    if (t % 20 == 0) formGroups();
    // Duyệt theo chỉ số: updateGroup có thể tạo nhóm mới (tách nhóm)
    for (size_t i = 0; i < sim_.groups.size(); ++i) {
        Group& g = sim_.groups[i];
        if (!g.alive) continue;
        if ((t + g.id * 7) % 50 == 0) updateGroup(g);
    }
    if (t % 100 == 50) diplomacy();
}

// ---------------------------------------------------------------------------
// Hình thành nhóm: cá thể chưa có nhóm + đồng loại xung quanh → Bầy / Nhóm du mục
// ---------------------------------------------------------------------------
void SocietySystem::formGroups() {
    for (Entity e : sim_.agents) {
        Social& so = sim_.reg.get<Social>(e);
        if (so.group >= 0) continue;
        Body& b = sim_.reg.get<Body>(e);
        if (b.dead) continue;
        const SpeciesDef& sd = sim_.species[b.species];
        if (sd.social != SocialType::Pack && sd.social != SocialType::Tribal) continue;
        Transform& tr = sim_.reg.get<Transform>(e);
        int root = sim_.species.root(b.species);
        std::vector<Entity> kin;
        int joinGroup = -1;
        sim_.grid.query(tr.pos, 6.0f, [&](Entity o, Vec2) {
            if (o == e) return;
            const Body* ob = sim_.reg.pool<Body>().tryGet(o);
            if (!ob || ob->dead || sim_.species.root(ob->species) != root) return;
            const Social& os = sim_.reg.get<Social>(o);
            if (os.group < 0) kin.push_back(o);
            else if (joinGroup < 0) joinGroup = os.group;
        });
        Mind& m = sim_.reg.get<Mind>(e);
        if (joinGroup >= 0) {
            Group* g = sim_.group(joinGroup);
            // Nhóm bài ngoại không nhận người lạ; nhóm cởi mở dễ nhận hơn
            float p = 0.3f;
            if (g && g->laws.test(static_cast<int>(Law::Xenophobia))) p = 0.05f;
            if (g && g->laws.test(static_cast<int>(Law::OpenBorders))) p = 0.6f;
            if (g && m.rng.chance(p)) sim_.joinGroup(e, joinGroup);
            continue;
        }
        if (kin.size() >= 2) {
            int gid = sim_.createGroup(e);
            for (Entity k : kin) sim_.joinGroup(k, gid);
        }
    }
}

// ---------------------------------------------------------------------------
// Cập nhật một nhóm (mỗi 50 tick)
// ---------------------------------------------------------------------------
void SocietySystem::updateGroup(Group& g) {
    // Loại thành viên đã chết / không hợp lệ
    g.members.erase(std::remove_if(g.members.begin(), g.members.end(), [&](Entity e) {
        return !sim_.reg.valid(e) || sim_.reg.get<Body>(e).dead || sim_.reg.get<Social>(e).group != g.id;
    }), g.members.end());
    if (g.members.empty()) { dissolve(g); return; }
    g.peakPopulation = std::max(g.peakPopulation, g.population());

    // ---- Tri thức tập thể + văn hoá (trung bình tính cách) ----
    std::array<int, kTechCount> count{};
    Culture c{0, 0, 0, 0, 0, 0};
    float intel = 0;
    int maxGen = 0;
    for (Entity e : g.members) {
        Mind& m = sim_.reg.get<Mind>(e);
        for (int t = 0; t < kTechCount; ++t) if (m.known.test(t)) count[t]++;
        c.militarism += (1 - m.pers.agreeableness) * 0.6f + m.pers.ambition * 0.4f;
        c.curiosity += m.pers.openness;
        c.collectivism += m.pers.extraversion;
        c.xenophobia += (1 - m.pers.agreeableness) * 0.5f + m.pers.fearfulness * 0.5f;
        c.tradition += m.pers.diligence;
        intel += m.intellect;
        maxGen = std::max(maxGen, sim_.reg.get<GenomeC>(e).genome.generation);
        Brain::updateIntellect(sim_, e);
    }
    float n = static_cast<float>(g.members.size());
    float k = 0.3f; // làm mượt: văn hoá thay đổi chậm
    g.culture.militarism = lerp(g.culture.militarism, c.militarism / n, k);
    g.culture.curiosity = lerp(g.culture.curiosity, c.curiosity / n, k);
    g.culture.collectivism = lerp(g.culture.collectivism, c.collectivism / n, k);
    g.culture.xenophobia = lerp(g.culture.xenophobia, c.xenophobia / n, k);
    g.culture.tradition = lerp(g.culture.tradition, c.tradition / n, k);
    g.avgIntellect = intel / n;
    g.generationDepth = maxGen;
    bool writing = g.knowledge.test(ti(Tech::Writing));
    TechSet before = g.knowledge;
    for (int t = 0; t < kTechCount; ++t) {
        // Tri thức "thuộc về văn hoá" khi ≥25% thành viên biết (có chữ viết: chỉ cần 1 người)
        int need = writing ? 1 : std::max(1, static_cast<int>(std::ceil(n * 0.25f)));
        if (count[t] >= need) g.knowledge.set(t);
        else if (count[t] == 0 && !writing) g.knowledge.reset(t); // tri thức thất truyền!
    }
    for (int t = 0; t < kTechCount; ++t)
        if (before.test(t) && !g.knowledge.test(t) && g.stage >= Stage::Tribe)
            sim_.chronicle.add(sim_.tick, EventKind::Society, g.displayName() + " đã thất truyền tri thức \"" + techDef(t).name + "\"", g.homeX, g.homeY);
    // Có chữ viết: thành viên "đọc" được tri thức lưu trữ của nhóm
    if (writing)
        for (Entity e : g.members) {
            Mind& m = sim_.reg.get<Mind>(e);
            int t = m.rng.irange(0, kTechCount - 1);
            if (g.knowledge.test(t) && !m.known.test(t) && techPrereqsMet(static_cast<Tech>(t), m.known) &&
                m.intellect >= techDef(t).minIntellect * 0.8f && m.rng.chance(0.25f))
                Brain::learnTech(sim_, e, static_cast<Tech>(t), false);
        }
    // Tôn giáo: lần đầu có tín ngưỡng → sinh ra một vị thần
    if (knows(g, Tech::Religion) && g.deity.empty()) {
        Rng& r = sim_.rng;
        const char* domain = g.animal ? "Mặt Trăng" : "Mặt Trời";
        if (sim_.world.burningCount() > 5) domain = "Lửa";
        else if (sim_.world.weather().rainTicks > 0) domain = "Mưa Giông";
        else if (r.chance(0.3f)) domain = "Cái Chết";
        else if (r.chance(0.3f)) domain = "Rừng Thẳm";
        g.deity = g.language.word(r, 2) + ", Thần " + domain;
        g.culture.piety = 0.6f;
        announce(g, ("bắt đầu thờ phụng " + g.deity).c_str());
    }
    if (knows(g, Tech::Religion)) g.culture.piety = std::min(1.0f, g.culture.piety + 0.002f);

    chooseLeader(g);
    // Nhóm du mục: nhà di chuyển theo thủ lĩnh khi khu vực cạn thức ăn
    if (g.stage == Stage::Pack && g.dwellings == 0 && sim_.reg.valid(g.leader)) {
        Vec2 lp = sim_.reg.get<Transform>(g.leader).pos;
        float moved = std::fabs(lp.x - g.homeX) + std::fabs(lp.y - g.homeY);
        if (g.food < g.population() && moved > 8) { g.homeX = static_cast<int>(lp.x); g.homeY = static_cast<int>(lp.y); g.settledTicks = 0; }
    }
    g.settledTicks += 50;
    // Sống cố định lâu → kích thích hình thành luật lệ
    if (g.stage >= Stage::Tribe)
        for (Entity e : g.members) Brain::stimulate(sim_, e, Stimulus::Settled, 2);

    recountBuildings(g);
    claimTerritory(g);
    assignRoles(g);
    advanceStage(g);
    enactLaws(g);
    // Lương thực trong kho hư hỏng dần (không có kho lương thì hỏng nhanh hơn)
    if (g.food > 0) g.food -= std::max(1, g.food / (g.hasGranary ? 80 : 25));

    if (sim_.tick - g.lastSplitCheck > 500) { g.lastSplitCheck = sim_.tick; splitGroup(g); }
    if ((sim_.tick + g.id * 31) % 2000 < 50) speciation(g);
}

void SocietySystem::chooseLeader(Group& g) {
    if (sim_.reg.valid(g.leader) && !sim_.reg.get<Body>(g.leader).dead && sim_.reg.get<Social>(g.leader).group == g.id) return;
    Entity best = NullEntity;
    float bs = -1;
    for (Entity e : g.members) {
        const Social& so = sim_.reg.get<Social>(e);
        const Body& b = sim_.reg.get<Body>(e);
        const Mind& m = sim_.reg.get<Mind>(e);
        if (so.role == Role::Child) continue;
        // Bầy thú: mạnh nhất làm đầu đàn; xã hội có trí khôn: uy tín + trí tuệ + tham vọng
        float s = g.animal ? b.damage * b.hp * (0.5f + so.status) : (so.status + m.intellect + m.pers.ambition * 0.5f + m.skillOf(Skill::Leadership));
        if (s > bs) { bs = s; best = e; }
    }
    if (best == NullEntity) best = g.members.front();
    Entity old = g.leader;
    g.leader = best;
    Social& ls = sim_.reg.get<Social>(best);
    ls.role = Role::Leader;
    ls.status = std::min(1.0f, ls.status + 0.2f);
    if (old != NullEntity && g.stage >= Stage::Tribe)
        sim_.chronicle.add(sim_.tick, EventKind::Society, ls.name + " trở thành thủ lĩnh mới của " + g.displayName(), g.homeX, g.homeY);
}

void SocietySystem::recountBuildings(Group& g) {
    g.dwellings = g.housing = g.buildingsTotal = 0;
    g.hasGranary = g.hasWorkshop = g.hasForge = g.hasTemple = g.hasTownHall = false;
    int R = std::max(6, g.territoryRadius + 2);
    for (int y = g.homeY - R; y <= g.homeY + R; ++y)
        for (int x = g.homeX - R; x <= g.homeX + R; ++x) {
            if (!sim_.world.inBounds(x, y)) continue;
            const Tile& t = sim_.world.at(x, y);
            if (t.building == Building::None || t.owner != g.id) continue;
            const BuildingInfo& bi = buildingInfo(t.building);
            if (t.building != Building::ScentMark) g.buildingsTotal++;
            if (bi.housing > 0) { g.dwellings++; g.housing += bi.housing; }
            switch (t.building) {
            case Building::Granary: g.hasGranary = true; break;
            case Building::Workshop: g.hasWorkshop = true; break;
            case Building::Forge: g.hasForge = true; break;
            case Building::Temple: case Building::Totem: g.hasTemple = true; break;
            case Building::TownHall: case Building::Castle: g.hasTownHall = true; break;
            default: break;
            }
        }
}

void SocietySystem::claimTerritory(Group& g) {
    // Nhóm đã di cư → trả lại lãnh thổ cũ (trừ ô có công trình của nhóm)
    if (g.claimX >= 0 && (g.claimX != g.homeX || g.claimY != g.homeY)) {
        int R0 = g.territoryRadius + 1;
        for (int y = g.claimY - R0; y <= g.claimY + R0; ++y)
            for (int x = g.claimX - R0; x <= g.claimX + R0; ++x)
                if (sim_.world.inBounds(x, y)) {
                    Tile& t = sim_.world.at(x, y);
                    if (t.owner == g.id && (t.building == Building::None || t.building == Building::ScentMark)) { t.owner = -1; sim_.world.markDirty(x, y); }
                }
    }
    g.claimX = g.homeX; g.claimY = g.homeY;
    bool claims = g.stage >= Stage::Tribe || knows(g, Tech::ScentMarking);
    int newR = claims ? territoryBase(g.stage) + static_cast<int>(std::sqrt(static_cast<float>(g.population()))) : 0;
    int R = std::max(newR, g.territoryRadius) + 1;
    g.territoryTiles = 0;
    for (int y = g.homeY - R; y <= g.homeY + R; ++y)
        for (int x = g.homeX - R; x <= g.homeX + R; ++x) {
            if (!sim_.world.inBounds(x, y)) continue;
            Tile& t = sim_.world.at(x, y);
            int d2 = (x - g.homeX) * (x - g.homeX) + (y - g.homeY) * (y - g.homeY);
            bool inside = d2 <= newR * newR && !sim_.world.isWater(x, y);
            if (inside && (t.owner < 0 || t.owner == g.id)) {
                if (t.owner != g.id) { t.owner = static_cast<int16_t>(g.id); sim_.world.markDirty(x, y); }
                g.territoryTiles++;
            } else if (!inside && t.owner == g.id && t.building == Building::None) {
                t.owner = -1; sim_.world.markDirty(x, y);
            } else if (t.owner == g.id) g.territoryTiles++;
        }
    g.territoryRadius = newR;
}

void SocietySystem::assignRoles(Group& g) {
    const SpeciesDef& sd = sim_.species[g.species];
    std::vector<Entity> adults;
    for (Entity e : g.members) {
        Social& so = sim_.reg.get<Social>(e);
        const Body& b = sim_.reg.get<Body>(e);
        bool adult = b.ageTicks >= static_cast<long>(sd.adultYears) * kTicksPerYear;
        if (!adult) { so.role = Role::Child; continue; }
        if (e == g.leader) { so.role = Role::Leader; continue; }
        adults.push_back(e);
    }
    int n = static_cast<int>(adults.size());
    if (n == 0) return;
    // Sắp xếp theo tính hung hăng × sức mạnh → người đầu danh sách làm lính
    std::sort(adults.begin(), adults.end(), [&](Entity a, Entity b) {
        auto score = [&](Entity e) { const Mind& m = sim_.reg.get<Mind>(e); return (1 - m.pers.agreeableness) + sim_.reg.get<Body>(e).damage * 0.05f; };
        return score(a) > score(b);
    });
    int soldiers = 0, builders = 0, farmers = 0, priests = 0, hunters = 0;
    if (sd.dexterous) {
        float sf = 0.05f + g.culture.militarism * 0.15f + (g.atWar() ? 0.25f : 0.f) + (g.laws.test(static_cast<int>(Law::Conscription)) ? 0.1f : 0.f);
        soldiers = g.stage >= Stage::Tribe ? static_cast<int>(n * sf) : 0;
        builders = std::max(1, static_cast<int>(n * (g.housing < g.population() ? 0.3f : 0.12f)));
        farmers = knows(g, Tech::Farming) ? static_cast<int>(n * 0.3f) : 0;
        priests = knows(g, Tech::Religion) ? std::min(2, n / 8 + 1) : 0;
        hunters = static_cast<int>(n * 0.15f);
    } else {
        hunters = n; // bầy thú: mọi con trưởng thành đều đi săn
    }
    int i = 0;
    auto give = [&](Role r, int count) { for (int k = 0; k < count && i < n; ++k) sim_.reg.get<Social>(adults[i++]).role = r; };
    give(Role::Soldier, soldiers);
    // Phần còn lại: xáo theo tính chăm chỉ (người chăm làm xây dựng/nông nghiệp)
    std::sort(adults.begin() + i, adults.end(), [&](Entity a, Entity b) {
        return sim_.reg.get<Mind>(a).pers.diligence > sim_.reg.get<Mind>(b).pers.diligence;
    });
    give(Role::Builder, builders);
    give(Role::Farmer, farmers);
    give(Role::Priest, priests);
    give(Role::Hunter, hunters);
    give(Role::Gatherer, n);
    // Người già nhất (không phải thủ lĩnh) làm trưởng lão
    if (g.stage >= Stage::Tribe && n >= 4) {
        Entity oldest = *std::max_element(adults.begin(), adults.end(), [&](Entity a, Entity b) {
            return sim_.reg.get<Body>(a).ageTicks < sim_.reg.get<Body>(b).ageTicks;
        });
        Social& os = sim_.reg.get<Social>(oldest);
        if (os.role != Role::Soldier) os.role = Role::Elder;
    }
}

void SocietySystem::announce(Group& g, const char* what) {
    sim_.chronicle.add(sim_.tick, EventKind::Society, g.displayName() + " (" + sim_.species[g.species].name + ") " + what, g.homeX, g.homeY);
}

// ---------------------------------------------------------------------------
// Thăng cấp văn minh
// ---------------------------------------------------------------------------
void SocietySystem::advanceStage(Group& g) {
    int pop = g.population();
    Stage s = g.stage;
    Stage next = s;
    bool animal = g.animal;
    switch (s) {
    case Stage::Pack:
        if (pop >= 6 && knows(g, Tech::Hierarchy) && (knows(g, Tech::Language) || (knows(g, Tech::ScentMarking) && knows(g, Tech::PackHunting))))
            next = Stage::Tribe;
        break;
    case Stage::Tribe:
        if (pop >= 10 && g.dwellings >= 3 && g.settledTicks >= 600) next = Stage::Village;
        break;
    case Stage::Village:
        if (animal ? (pop >= 18 && knows(g, Tech::Language) && knows(g, Tech::Religion))
                   : (pop >= 22 && g.dwellings >= 7 && knows(g, Tech::Farming)))
            next = Stage::Town;
        break;
    case Stage::Town:
        if (animal ? (pop >= 30 && knows(g, Tech::Law)) : (pop >= 40 && g.dwellings >= 12 && knows(g, Tech::Masonry)))
            next = Stage::City;
        break;
    default: break;
    }
    // Quốc gia: cần luật pháp + (chữ viết & toà thị chính với loài có tay khéo)
    if (s != Stage::Nation && s >= (animal ? Stage::Village : Stage::Town) && knows(g, Tech::Law) &&
        (animal ? pop >= 20 : (pop >= 30 && knows(g, Tech::Writing) && g.hasTownHall)))
        next = Stage::Nation;
    // Suy tàn: dân số giảm mạnh → tụt cấp
    if (next == s && s > Stage::Pack) {
        static const int minPop[] = {0, 3, 5, 10, 18, 12};
        if (pop < minPop[static_cast<int>(s)]) {
            g.stage = static_cast<Stage>(static_cast<int>(s) - 1);
            g.stageTick = sim_.tick;
            announce(g, (std::string("suy tàn, chỉ còn là ") + stageName(g.stage, animal)).c_str());
            return;
        }
    }
    if (next == s) return;
    g.stage = next;
    g.stageTick = sim_.tick;
    if (next == Stage::Tribe) {
        g.gov = animal ? Government::AlphaRule : (g.culture.tradition > 0.5f ? Government::ElderCouncil : Government::Chiefdom);
        announce(g, (std::string("đã trở thành ") + stageName(next, animal) + ", có lãnh thổ riêng, thể chế: " + governmentName(g.gov)).c_str());
    } else if (next == Stage::Nation) {
        foundNation(g);
    } else {
        announce(g, (std::string("phát triển thành ") + stageName(next, animal) + " (" + std::to_string(pop) + " dân)").c_str());
    }
}

void SocietySystem::foundNation(Group& g) {
    Rng& r = sim_.rng;
    const Culture& c = g.culture;
    if (c.piety > 0.65f) g.gov = Government::Theocracy;
    else if (c.militarism > 0.62f) g.gov = Government::MilitaryJunta;
    else if (c.curiosity > 0.58f && c.militarism < 0.45f) g.gov = Government::Republic;
    else g.gov = g.animal ? Government::AlphaRule : Government::Monarchy;
    const char* prefix = "Vương quốc ";
    if (g.gov == Government::Republic) prefix = "Cộng hoà ";
    if (g.gov == Government::Theocracy) prefix = "Thánh quốc ";
    if (g.gov == Government::MilitaryJunta) prefix = "Đế chế ";
    if (g.animal) prefix = "Vương quốc Sói ";
    if (g.animal && sim_.species[g.species].name.find("Sói") == std::string::npos) prefix = "Vương quốc thú ";
    g.nationName = std::string(prefix) + g.language.word(r, 2);
    g.symbol = static_cast<uint8_t>(r.irange(0, 7));
    g.col2R = static_cast<uint8_t>(r.irange(0, 255)); g.col2G = static_cast<uint8_t>(r.irange(0, 255)); g.col2B = static_cast<uint8_t>(r.irange(0, 255));
    sim_.chronicle.add(sim_.tick, EventKind::Society,
                       "QUỐC GIA MỚI: " + g.name + " (" + sim_.species[g.species].name + ") lập nên \"" + g.nationName + "\" — thể chế " +
                           governmentName(g.gov) + ", ngôn ngữ " + g.language.familyName + (g.deity.empty() ? "" : ", quốc đạo thờ " + g.deity),
                       g.homeX, g.homeY);
}

void SocietySystem::enactLaws(Group& g) {
    if (g.stage < Stage::Tribe && !knows(g, Tech::Hierarchy)) return;
    const Culture& c = g.culture;
    auto enact = [&](Law l, bool cond) {
        if (!cond || g.laws.test(static_cast<int>(l))) return;
        g.laws.set(static_cast<int>(l));
        if (g.stage >= Stage::Tribe) announce(g, (std::string("ban hành luật: \"") + lawName(l) + "\"").c_str());
    };
    enact(Law::AlphaEatsFirst, knows(g, Tech::Hierarchy) && (g.animal || c.militarism > 0.6f));
    enact(Law::ShareKills, knows(g, Tech::PackHunting) || c.collectivism > 0.6f);
    enact(Law::DefendTerritory, g.stage >= Stage::Tribe && (knows(g, Tech::ScentMarking) || c.militarism > 0.5f));
    enact(Law::NoKillingKin, knows(g, Tech::Language) && c.militarism < 0.6f);
    enact(Law::TeachTheYoung, knows(g, Tech::Language) && c.tradition > 0.4f);
    enact(Law::Taxation, knows(g, Tech::Law) && g.stage >= Stage::Town);
    enact(Law::Conscription, knows(g, Tech::Law) && c.militarism > 0.5f && g.stage >= Stage::Village);
    enact(Law::WorshipDuty, knows(g, Tech::Religion) && c.piety > 0.55f);
    bool xeno = c.xenophobia > 0.62f && g.stage >= Stage::Tribe;
    enact(Law::Xenophobia, xeno && !g.laws.test(static_cast<int>(Law::OpenBorders)));
    enact(Law::OpenBorders, !xeno && c.curiosity > 0.6f && g.stage >= Stage::Village && !g.laws.test(static_cast<int>(Law::Xenophobia)));
}

// ---------------------------------------------------------------------------
// Ngoại giao & chiến tranh
// ---------------------------------------------------------------------------
void SocietySystem::diplomacy() {
    // Xử lý các vụ giết chóc giữa các nhóm
    for (auto [killer, victim] : sim_.killEvents) {
        Group* a = sim_.group(killer);
        Group* b = sim_.group(victim);
        if (!a || !b) continue;
        b->relations[killer].opinion -= 8;
        a->relations[victim].opinion -= 2;
        if (b->relations[killer].atWar) b->relations[killer].warCasualties++;
        a->kills++; b->losses++;
    }
    sim_.killEvents.clear();

    auto& G = sim_.groups;
    for (size_t i = 0; i < G.size(); ++i) {
        Group& a = G[i];
        if (!a.alive || a.stage < Stage::Tribe) continue;
        for (size_t j = 0; j < G.size(); ++j) {
            if (i == j) continue;
            Group& b = G[j];
            if (!b.alive || b.stage < Stage::Pack) continue;
            float d = std::sqrt(static_cast<float>((a.homeX - b.homeX) * (a.homeX - b.homeX) + (a.homeY - b.homeY) * (a.homeY - b.homeY)));
            float reach = static_cast<float>(a.territoryRadius + b.territoryRadius + 15);
            Relation& rel = a.relations[b.id];
            if (d > reach) { rel.opinion *= 0.98f; continue; }
            bool sameRoot = sim_.species.root(a.species) == sim_.species.root(b.species);
            bool kinGroup = a.parentGroup == b.id || b.parentGroup == a.id;      // cùng gốc văn hoá
            bool pacifist = sameRoot && a.laws.test(static_cast<int>(Law::NoKillingKin));
            rel.opinion += sameRoot ? (kinGroup ? 0.6f : 0.3f) : -0.6f * a.culture.xenophobia;
            if (d < a.territoryRadius + b.territoryRadius) {
                // Tranh chấp biên giới: văn hoá hiếu chiến càng gay gắt; luật "cấm giết đồng loại" làm dịu đi
                float friction = 2.0f * a.culture.militarism * a.culture.militarism;
                rel.opinion -= pacifist ? friction * 0.4f : friction;
            }
            if (a.laws.test(static_cast<int>(Law::OpenBorders))) rel.opinion += 0.5f;
            rel.opinion = std::clamp(rel.opinion, -100.0f, 100.0f);

            // Lan truyền văn hoá: láng giềng thân thiện học hỏi tri thức của nhau (giao thương, kết hôn...)
            if (!rel.atWar && rel.opinion > 15 && !b.members.empty() && sim_.rng.chance(0.08f)) {
                int t = sim_.rng.irange(0, kTechCount - 1);
                if (a.knowledge.test(t) && !b.knowledge.test(t)) {
                    Entity learner = b.members[sim_.rng.irange(0, static_cast<int>(b.members.size()) - 1)];
                    Mind& lm = sim_.reg.get<Mind>(learner);
                    const TechDef& td = techDef(t);
                    if ((td.animalsCan || !b.animal) && techPrereqsMet(static_cast<Tech>(t), lm.known) && lm.intellect >= td.minIntellect * 0.8f)
                        Brain::learnTech(sim_, learner, static_cast<Tech>(t), false);
                }
            }

            // Tuyên chiến
            if (!rel.atWar && rel.opinion < (pacifist ? -70 : -40) && a.culture.militarism > 0.5f && a.population() >= b.population() * 0.6f &&
                sim_.rng.chance(0.3f)) {
                rel.atWar = true; rel.warStart = sim_.tick; rel.warCasualties = 0;
                b.relations[a.id].atWar = true; b.relations[a.id].warStart = sim_.tick;
                a.warTarget = b.id;
                if (b.warTarget < 0) b.warTarget = a.id;
                sim_.chronicle.add(sim_.tick, EventKind::War, "CHIẾN TRANH: " + a.displayName() + " tuyên chiến với " + b.displayName(), b.homeX, b.homeY);
            }
            // Nghị hoà khi chiến tranh kéo dài / tổn thất nặng
            if (rel.atWar) {
                long dur = sim_.tick - rel.warStart;
                bool weary = dur > 1800 && (rel.warCasualties > a.population() / 3 || sim_.rng.chance(0.15f));
                if (weary) {
                    rel.atWar = false; rel.opinion = 0;
                    b.relations[a.id].atWar = false; b.relations[a.id].opinion = 0;
                    if (a.warTarget == b.id) a.warTarget = -1;
                    if (b.warTarget == a.id) b.warTarget = -1;
                    sim_.chronicle.add(sim_.tick, EventKind::War, "HOÀ BÌNH: " + a.displayName() + " và " + b.displayName() + " ký hiệp ước đình chiến", a.homeX, a.homeY);
                }
                // Chinh phục: địch gần như bị xoá sổ → sáp nhập phần còn lại
                if (b.population() <= 2 && a.population() >= 4) {
                    sim_.chronicle.add(sim_.tick, EventKind::War, a.displayName() + " đã chinh phục " + b.displayName(), b.homeX, b.homeY);
                    std::vector<Entity> rest = b.members;
                    for (Entity e : rest) {
                        if (sameRoot) sim_.joinGroup(e, a.id);
                        else sim_.leaveGroup(e);
                    }
                    // Lãnh thổ & công trình về tay kẻ chinh phục
                    int R = b.territoryRadius + 2;
                    for (int y = b.homeY - R; y <= b.homeY + R; ++y)
                        for (int x = b.homeX - R; x <= b.homeX + R; ++x)
                            if (sim_.world.inBounds(x, y) && sim_.world.at(x, y).owner == b.id) { sim_.world.at(x, y).owner = static_cast<int16_t>(a.id); sim_.world.markDirty(x, y); }
                    a.food += b.food; a.wood += b.wood; a.stone += b.stone; a.ore += b.ore;
                    a.knowledge |= b.knowledge; // học được tri thức của kẻ bại trận
                    dissolve(b);
                    a.warTarget = -1;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Tách nhóm → văn hoá phân nhánh
// ---------------------------------------------------------------------------
void SocietySystem::splitGroup(Group& g) {
    int pop = g.population();
    bool tooBig = pop > splitCap(g.stage);
    bool schism = pop >= 12 && !knows(g, Tech::Law) && sim_.rng.chance(0.05f);
    if (!tooBig && !schism) return;
    // Thủ lĩnh nhóm tách: thành viên tham vọng nhất (không phải thủ lĩnh hiện tại)
    Entity rebel = NullEntity;
    float best = -1;
    for (Entity e : g.members) {
        if (e == g.leader || sim_.reg.get<Social>(e).role == Role::Child) continue;
        float a = sim_.reg.get<Mind>(e).pers.ambition;
        if (a > best) { best = a; rebel = e; }
    }
    if (rebel == NullEntity) return;
    // Chọn hướng di cư: điểm đi được cách 14–24 ô
    Rng& r = sim_.rng;
    int nx = g.homeX, ny = g.homeY;
    bool ok = false;
    for (int tries = 0; tries < 20 && !ok; ++tries) {
        float ang = r.range(0, 6.2831853f), dd = r.range(14, 24);
        nx = g.homeX + static_cast<int>(std::cos(ang) * dd);
        ny = g.homeY + static_cast<int>(std::sin(ang) * dd);
        ok = sim_.world.inBounds(nx, ny) && sim_.world.walkable(nx, ny) && sim_.world.at(nx, ny).owner < 0;
    }
    if (!ok) return;
    std::string oldName = g.displayName();
    int gid = sim_.createGroup(rebel);
    Group& ng = sim_.groups[gid];
    Group& og = g; // std::deque giữ tham chiếu ổn định khi thêm phần tử
    ng.language = og.language.drift(r);
    ng.name = ng.language.word(r, 2);
    ng.culture = og.culture;
    ng.culture.militarism = clamp01(ng.culture.militarism + r.gauss(0.1f));
    ng.culture.xenophobia = clamp01(ng.culture.xenophobia + r.gauss(0.1f));
    ng.knowledge = og.knowledge;
    ng.parentGroup = og.id;
    ng.homeX = nx; ng.homeY = ny;
    ng.stage = og.stage >= Stage::Tribe ? Stage::Tribe : Stage::Pack;
    ng.gov = og.gov;
    ng.deity = og.deity;
    ng.laws = og.laws;
    ng.relations[og.id].opinion = 25;
    og.relations[gid].opinion = 25;
    // Mang theo ~40% thành viên (ưu tiên những ai trung thành với người tách nhóm: ngẫu nhiên)
    std::vector<Entity> movers;
    for (Entity e : og.members) if (e != og.leader && e != rebel && r.chance(0.4f)) movers.push_back(e);
    for (Entity e : movers) sim_.joinGroup(e, gid);
    int share = og.food / 3;
    og.food -= share; ng.food += share;
    sim_.chronicle.add(sim_.tick, EventKind::Society,
                       sim_.reg.get<Social>(rebel).name + " dẫn " + std::to_string(movers.size() + 1) + " cá thể tách khỏi " + oldName +
                           ", lập nên " + ng.name + " (văn hoá phân nhánh, ngôn ngữ trôi dạt)",
                       nx, ny);
}

// ---------------------------------------------------------------------------
// Hình thành loài mới
// ---------------------------------------------------------------------------
void SocietySystem::speciation(Group& g) {
    // Cần: nhóm đủ lớn, tồn tại lâu (cô lập địa lý/văn hoá), đã qua nhiều thế hệ
    if (g.population() < 6 || sim_.tick - g.foundedTick < 15L * kTicksPerYear || g.generationDepth < 6) return;
    int sp = g.species;
    // Gen trung bình của nhóm
    Genome avg;
    avg.g.fill(0);
    for (Entity e : g.members) {
        const Genome& gn = sim_.reg.get<GenomeC>(e).genome;
        for (int i = 0; i < kGeneCount; ++i) avg.g[i] += gn.g[i];
    }
    for (float& v : avg.g) v /= g.population();
    float d = Genome::distance(avg, sim_.species[sp].baseGenome);
    if (d < 0.30f) return;
    SpeciesDef nd = sim_.species[sp];
    std::string base = sim_.species[sim_.species.root(sp)].name;
    nd.name = base + " " + g.language.word(sim_.rng, 2);
    nd.parent = sp;
    nd.baseGenome = avg;
    nd.bornTick = sim_.tick;
    nd.population = 0;
    // Trí tuệ đủ cao → tiến hoá ra "đôi tay khéo léo" (chi trước biết cầm nắm)
    bool gainedHands = !nd.dexterous && avg[Gene::Intelligence] > 0.5f;
    if (gainedHands) nd.dexterous = true;
    nd.baseHp *= 0.8f + avg[Gene::Strength] * 0.4f;
    nd.r = static_cast<uint8_t>(std::clamp(nd.r + sim_.rng.irange(-40, 40), 30, 255));
    nd.g = static_cast<uint8_t>(std::clamp(nd.g + sim_.rng.irange(-40, 40), 30, 255));
    nd.b = static_cast<uint8_t>(std::clamp(nd.b + sim_.rng.irange(-40, 40), 30, 255));
    int nid = sim_.species.add(nd);
    for (Entity e : g.members) sim_.reg.get<Body>(e).species = nid;
    g.species = nid;
    g.animal = !nd.dexterous;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", avg[Gene::Intelligence]);
    sim_.chronicle.add(sim_.tick, EventKind::Evolution,
                       "TIẾN HOÁ: nhánh " + sim_.species[sp].name + " của " + g.displayName() + " trở thành loài mới \"" + nd.name +
                           "\" (trí tuệ gen trung bình " + buf + (gainedHands ? ", đã tiến hoá chi trước biết cầm nắm!" : "") + ")",
                       g.homeX, g.homeY);
}

void SocietySystem::dissolve(Group& g) {
    std::vector<Entity> rest = g.members;
    for (Entity e : rest) sim_.leaveGroup(e);
    if (g.stage >= Stage::Tribe) sim_.chronicle.add(sim_.tick, EventKind::Society, g.displayName() + " đã diệt vong", g.homeX, g.homeY);
    g.alive = false;
    g.members.clear();
    for (auto& other : sim_.groups) {
        other.relations.erase(g.id);
        if (other.warTarget == g.id) other.warTarget = -1;
    }
    // Lãnh thổ trở thành vô chủ (công trình còn lại thành phế tích)
    int R = g.territoryRadius + 3;
    for (int y = g.homeY - R; y <= g.homeY + R; ++y)
        for (int x = g.homeX - R; x <= g.homeX + R; ++x)
            if (sim_.world.inBounds(x, y) && sim_.world.at(x, y).owner == g.id) { sim_.world.at(x, y).owner = -1; sim_.world.markDirty(x, y); }
}

} // namespace wb
