// =============================================================================
// Brain.cpp — Cài đặt AI cá nhân.
//
// Cấu trúc file:
//   1. Tiện ích (di chuyển, tìm ô)
//   2. QUAN SÁT  (perceive)       — nhìn sinh vật, tài nguyên, lửa, vật phẩm; ghi nhớ
//   3. TRẠNG THÁI (buildState)    — chuyển hiểu biết thành bit WorldState cho GOAP
//   4. SUY NGHĨ  (scoreGoals)     — Utility AI: chấm điểm mọi mục tiêu theo nhu cầu × tính cách
//   5. QUYẾT ĐỊNH (plan)          — GOAP A* tìm chuỗi hành động
//   6. HÀNH ĐỘNG (act + executors)
//   7. HỌC       (onDone/onFail, stimulate, learnTech, dạy học, trí tuệ)
// =============================================================================
#include "Brain.h"
#include "Mind.h"
#include "../sim/Simulation.h"
#include <cmath>
#include <cstring>

namespace wb {

namespace {

// ---------------------------------------------------------------------------
// 1. Tiện ích
// ---------------------------------------------------------------------------
enum class Res { Running, Done, Failed };

inline int tileX(Vec2 p) { return static_cast<int>(p.x); }
inline int tileY(Vec2 p) { return static_cast<int>(p.y); }
inline Vec2 tileCenter(int x, int y) { return {x + 0.5f, y + 0.5f}; }

bool isAdult(const Simulation& sim, const Body& b) {
    return b.ageTicks >= static_cast<long>(sim.species[b.species].adultYears) * kTicksPerYear;
}

// Di chuyển về phía mục tiêu; tự lách vật cản bằng cách thử các hướng lệch.
// Trả về true khi đã tới nơi (trong bán kính `arrive`).
bool moveTo(Simulation& sim, Entity e, Transform& tr, Body& body, Mind& m, Vec2 tgt, float arrive) {
    Vec2 d = tgt - tr.pos;
    float L = d.len();
    if (L <= arrive) { tr.stuck = 0; tr.moving = false; return true; }
    tr.moving = true;
    tr.target = tgt;
    float cost = sim.world.moveCost(tileX(tr.pos), tileY(tr.pos));
    float spd = std::min(L, body.speed / std::max(0.5f, cost));
    Vec2 dir = d * (1.0f / L);
    static const float angles[] = {0.f, 0.6f, -0.6f, 1.2f, -1.2f, 1.9f, -1.9f};
    // Hướng lệch ưu tiên "nhớ" theo entity để không bị lắc qua lại
    float side = (entityIndex(e) & 1) ? 1.f : -1.f;
    for (float a0 : angles) {
        float a = a0 * side;
        float c = std::cos(a), s = std::sin(a);
        Vec2 nd{dir.x * c - dir.y * s, dir.x * s + dir.y * c};
        Vec2 np = tr.pos + nd * spd;
        if (sim.world.walkable(tileX(np), tileY(np))) {
            tr.pos = np;
            if (a0 == 0.f) tr.stuck = std::max(0, tr.stuck - 1);
            else tr.stuck++;
            return false;
        }
    }
    tr.stuck += 3;
    // Bị nước chặn → ghi nhớ (kích thích phát minh "Xây cầu")
    Vec2 ahead = tr.pos + dir;
    if (sim.world.isWater(tileX(ahead), tileY(ahead))) {
        m.blockedWater = {tileX(ahead), tileY(ahead)};
        m.pendingStim[static_cast<int>(Stimulus::SeeWater)]++;
    }
    return false;
}

Vec2 randomPointNear(Simulation& sim, Mind& m, Vec2 c, float r) {
    for (int i = 0; i < 8; ++i) {
        Vec2 p{c.x + m.rng.range(-r, r), c.y + m.rng.range(-r, r)};
        if (sim.world.walkable(tileX(p), tileY(p))) return p;
    }
    return c;
}

Vec2 homePos(const Group& g) { return tileCenter(g.homeX, g.homeY); }

bool canEatPlants(const SpeciesDef& s) { return s.diet != Diet::Carnivore; }
bool canEatMeat(const SpeciesDef& s) { return s.diet != Diet::Herbivore; }

int carryCap(const SpeciesDef& s) { return s.dexterous ? 8 : 2; }

// ---------------------------------------------------------------------------
// 2. QUAN SÁT
// ---------------------------------------------------------------------------
void perceive(Simulation& sim, Entity e, Mind& m, const Transform& tr, const Body& body, const Social& so) {
    const SpeciesDef& me = sim.species[body.species];
    const Group* grp = so.group >= 0 ? &sim.groups[so.group] : nullptr;
    float radius = 5.0f + m.intellect * 6.0f + m.pers.openness * 3.0f + (me.diet == Diet::Carnivore ? 3.0f : 0.0f);
    m.stm.clear();
    float bestPrey = 1e9f, bestThreat = 1e9f, bestMate = 1e9f, bestStranger = 1e9f, bestAlly = 1e9f, bestEnemy = 1e9f;
    bool adult = isAdult(sim, body);
    int myRoot = sim.species.root(body.species);

    sim.grid.query(tr.pos, radius, [&](Entity o, Vec2 opos) {
        if (o == e) return;
        const Body* ob = sim.reg.pool<Body>().tryGet(o);
        if (!ob || ob->dead) return;
        const Social* os = sim.reg.pool<Social>().tryGet(o);
        float d2 = dist2(opos, tr.pos);
        int oRoot = sim.species.root(ob->species);
        const SpeciesDef& other = sim.species[ob->species];
        Percept p{PerceptKind::Stranger, o, opos, 0};
        bool sameGroup = os && so.group >= 0 && os->group == so.group;
        if (sameGroup || (oRoot == myRoot && so.group < 0 && os && os->group < 0)) {
            p.kind = sameGroup ? PerceptKind::Ally : PerceptKind::Kin;
            m.stm.allies++;
            if (d2 < bestAlly) { bestAlly = d2; m.stm.nearestAlly = o; }
            // Bạn đời tiềm năng: cùng loài, khác giới, trưởng thành, chưa mang thai
            if (adult && ob->species == body.species && ob->female != body.female && ob->pregnancyTicks == 0 &&
                isAdult(sim, *ob) && d2 < bestMate) { bestMate = d2; m.stm.nearestMate = o; }
        } else if (sim.species.canHunt(ob->species, body.species) && !ob->sleeping) {
            p.kind = PerceptKind::Predator;
            p.strength = (ob->damage * ob->hp) / std::max(1.0f, body.damage * body.hp);
            m.stm.predators++;
            m.stm.threat += p.strength;
            if (d2 < bestThreat) { bestThreat = d2; m.stm.nearestThreat = o; m.stm.threatPos = opos; }
        } else if (sim.species.canHunt(body.species, ob->species)) {
            p.kind = PerceptKind::Prey;
            m.stm.prey++;
            if (d2 < bestPrey) { bestPrey = d2; m.stm.nearestPrey = o; }
            m.ltm.remember(FactType::HuntingGround, tileX(opos), tileY(opos), sim.tick);
        } else if (oRoot == myRoot || (me.dexterous && other.dexterous)) {
            // Đồng loại khác nhóm hoặc chủng tộc có trí khôn khác: kẻ lạ / kẻ thù
            bool enemy = false;
            if (grp && os && os->group >= 0 && grp->atWarWith(os->group)) enemy = true;
            // Kẻ lạ xâm phạm lãnh thổ + luật bảo vệ lãnh thổ + tính hung hăng → coi là kẻ thù
            const Tile& t = sim.world.at(tileX(opos), tileY(opos));
            // Thú: bản năng lãnh thổ. Loài có trí khôn: chỉ khi quan hệ giữa hai nhóm đã xấu.
            if (grp && t.owner == so.group && grp->laws.test(static_cast<int>(Law::DefendTerritory)) &&
                m.pers.agreeableness < 0.45f && (!os || os->group != so.group)) {
                float opinion = 0;
                if (os && os->group >= 0) {
                    auto it = grp->relations.find(os->group);
                    if (it != grp->relations.end()) opinion = it->second.opinion;
                }
                if (grp->animal || opinion < -15) enemy = true;
            }
            if (grp && grp->laws.test(static_cast<int>(Law::Xenophobia)) && oRoot != myRoot) enemy = true;
            if (enemy) {
                p.kind = PerceptKind::Enemy;
                m.stm.enemies++;
                p.strength = (ob->damage * ob->hp) / std::max(1.0f, body.damage * body.hp);
                if (d2 < bestEnemy) { bestEnemy = d2; m.stm.nearestEnemy = o; }
            }
            if (d2 < bestStranger) { bestStranger = d2; m.stm.nearestStranger = o; }
            if (oRoot == myRoot && grp) m.pendingStim[static_cast<int>(Stimulus::TerritoryClash)]++;
        } else if (other.monster) {
            p.kind = PerceptKind::Predator;
            m.stm.threat += 0.5f;
            m.stm.predators++;
            if (d2 < bestThreat) { bestThreat = d2; m.stm.nearestThreat = o; m.stm.threatPos = opos; }
        }
        m.stm.add(p);
    });
    if (m.stm.predators > 0 && m.stm.nearestThreat != NullEntity)
        m.ltm.remember(FactType::Danger, tileX(m.stm.threatPos), tileY(m.stm.threatPos), sim.tick);
    if (m.stm.allies >= 3) m.pendingStim[static_cast<int>(Stimulus::GroupLife)]++;

    // ---- Quét tài nguyên trên bản đồ (một lượt duy nhất quanh vị trí) ----
    m.seenFood.reset(); m.seenTree.reset(); m.seenStone.reset(); m.seenOre.reset();
    m.seenCorpse.reset(); m.seenTool.reset(); m.seenFire.reset();
    int cx = tileX(tr.pos), cy = tileY(tr.pos);
    int r = std::min(12, static_cast<int>(radius));
    int bf = 1 << 30, bt = bf, bs = bf, bo = bf, bc = bf, bfi = bf;
    bool wantsPlants = canEatPlants(me), hands = me.dexterous;
    // Tối ưu: tầm nhìn rộng thì quét thưa (bước 2) theo ô bàn cờ, xen kẽ giữa các lần
    // suy nghĩ → mỗi lần chỉ đọc ~1/2 số ô nhưng vẫn phủ đủ sau 2 nhịp.
    int step = r > 6 ? 2 : 1;
    int phase = static_cast<int>((sim.tick / std::max(1, m.thinkInterval)) & 1);
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r + ((step == 2) ? ((dy + phase) & 1) : 0); dx <= r; dx += step) {
            int x = cx + dx, y = cy + dy;
            if (!sim.world.inBounds(x, y)) continue;
            int d = dx * dx + dy * dy;
            if (d > r * r) continue;
            const Tile& t = sim.world.at(x, y);
            if (wantsPlants && t.food > 25 && !t.fire && d < bf) { bf = d; m.seenFood = {x, y}; }
            if (hands && t.wood > 25 && !t.fire && d < bt) { bt = d; m.seenTree = {x, y}; }
            if (hands && t.stone > 15 && d < bs) { bs = d; m.seenStone = {x, y}; }
            if (t.ore > 15 && d < bo) { bo = d; m.seenOre = {x, y}; }
            if (t.fire && d < bfi) { bfi = d; m.seenFire = {x, y}; }
            if (t.itemFlag && d < bc) {
                for (const auto& it : sim.world.items())
                    if (it.x == x && it.y == y && it.amount > 0) {
                        if ((it.kind == ItemKind::Corpse || it.kind == ItemKind::Meat) && canEatMeat(me)) { bc = d; m.seenCorpse = {x, y}; }
                        if ((it.kind == ItemKind::OldTool || it.kind == ItemKind::OldWeapon) && hands) m.seenTool = {x, y};
                        if (it.kind == ItemKind::Bones || it.kind == ItemKind::Corpse) m.pendingStim[static_cast<int>(Stimulus::SeeDeath)]++;
                    }
            }
        }
    long now = sim.tick;
    if (m.seenFood.valid()) m.ltm.remember(FactType::FoodSource, m.seenFood.x, m.seenFood.y, now);
    if (m.seenTree.valid()) m.ltm.remember(FactType::WoodSource, m.seenTree.x, m.seenTree.y, now);
    if (m.seenStone.valid()) m.ltm.remember(FactType::StoneSource, m.seenStone.x, m.seenStone.y, now);
    if (m.seenOre.valid()) {
        m.ltm.remember(FactType::OreSource, m.seenOre.x, m.seenOre.y, now);
        if (hands) m.pendingStim[static_cast<int>(Stimulus::SeeOre)]++;
    }
    if (m.seenFire.valid()) {
        m.ltm.remember(FactType::FireSeen, m.seenFire.x, m.seenFire.y, now);
        m.pendingStim[static_cast<int>(Stimulus::SeeFire)]++;
    }
    if (m.seenTool.valid()) m.ltm.remember(FactType::OldTool, m.seenTool.x, m.seenTool.y, now);
    // Sinh vật có tay khéo đứng gần đá thì hay "nghịch" đá → tích luỹ ý tưởng công cụ đá
    if (hands && m.seenStone.valid() && bs <= 4) m.pendingStim[static_cast<int>(Stimulus::HandleStone)]++;
    if (grp) m.ltm.remember(FactType::Home, grp->homeX, grp->homeY, now, 100000 + so.group);
    // Lạnh / mưa khi không có nơi trú → kích thích phát minh "Nơi trú ẩn"
    if (body.cold > 0.3f) m.pendingStim[static_cast<int>(Stimulus::ColdOrRain)]++;
    const Weather& w = sim.world.weather();
    if (w.rainTicks > 0 && dist2(tr.pos, {w.rainX, w.rainY}) < w.rainRadius * w.rainRadius)
        m.pendingStim[static_cast<int>(Stimulus::ColdOrRain)]++;
    // Quên dần
    m.ltm.decay(0.004f * (1.2f - m.intellect));
}

// ---------------------------------------------------------------------------
// 3. TRẠNG THÁI WORLDSTATE
// ---------------------------------------------------------------------------
bool hasMaterialsFor(const Inventory& inv, Building b) {
    const BuildingInfo& bi = buildingInfo(b);
    return inv.wood >= bi.wood && inv.stone >= bi.stone && inv.ore >= bi.ore;
}

WorldState buildState(Simulation& sim, Mind& m, const Transform& tr, const Body& body, const Inventory& inv, const Social& so) {
    using namespace ws;
    WorldState s = 0;
    if (inv.food > 0) s |= HasFood;
    if (body.hunger < 0.12f) s |= Fed;
    if (body.fatigue < 0.08f) s |= Rested;
    if (m.stm.threat < 0.3f) s |= Safe;
    const Tile& here = sim.world.at(tileX(tr.pos), tileY(tr.pos));
    if (here.food > 10) s |= NearFood;
    if (m.seenFood.valid() || m.ltm.knows(FactType::FoodSource)) s |= KnowsFood;
    if (m.stm.nearestPrey != NullEntity) s |= SeesPrey;
    if (m.seenCorpse.valid()) s |= SeesCarrion;
    if (inv.wood >= 2) s |= HasWood;
    if (inv.stone >= 2) s |= HasStone;
    if (inv.ore >= 1) s |= HasOre;
    if (inv.toolTier > 0) s |= HasTool;
    if (inv.weaponTier > 0) s |= HasWeapon;
    if (m.seenTree.valid() || m.ltm.knows(FactType::WoodSource)) s |= KnowsTree;
    if (m.seenStone.valid() || m.ltm.knows(FactType::StoneSource)) s |= KnowsStone;
    if (m.seenOre.valid() || m.ltm.knows(FactType::OreSource)) s |= KnowsOre;
    if (m.buildType != Building::None && hasMaterialsFor(inv, m.buildType)) s |= HasMaterials;
    if (m.stm.nearestEnemy != NullEntity) s |= SeesEnemy;
    if (m.stm.nearestMate != NullEntity) {
        if (auto* t = sim.reg.pool<Transform>().tryGet(m.stm.nearestMate); t && dist2(t->pos, tr.pos) < 4) s |= MateNearby;
    }
    if (m.stm.nearestAlly != NullEntity) {
        if (auto* t = sim.reg.pool<Transform>().tryGet(m.stm.nearestAlly); t && dist2(t->pos, tr.pos) < 4) s |= AllyNearby;
    }
    if (so.group >= 0) {
        const Group& g = sim.groups[so.group];
        float hr = std::max(2.5f, g.territoryRadius * 0.35f);
        if (dist2(tr.pos, homePos(g)) < hr * hr) s |= AtHome;
        if (g.food > 0) s |= GroupFood;
    }
    return s;
}

// ---------------------------------------------------------------------------
// 4. SUY NGHĨ — Utility AI
// ---------------------------------------------------------------------------
Building pickBuilding(Simulation& sim, Mind& m, const Group& g, const SpeciesDef& sd) {
    auto knows = [&](Tech t) { return m.known.test(ti(t)); };
    int pop = g.population();
    if (!sd.dexterous) {
        if (knows(Tech::Shelter) && g.dwellings < pop / 3 + 1) return Building::Den;
        return Building::None;
    }
    if (m.blockedWater.valid() && knows(Tech::Bridges) && m.rng.uniform() < 0.3f) return Building::Bridge;
    if (g.housing < pop + 2) {
        if (knows(Tech::Masonry) && g.stone >= 4) return Building::StoneHouse;
        if (knows(Tech::WoodWorking)) return Building::House;
        if (knows(Tech::Shelter)) return Building::Hut;
    }
    if (knows(Tech::Farming) && !g.hasGranary && pop >= 8) return Building::Granary;
    if (knows(Tech::WoodWorking) && !g.hasWorkshop && pop >= 10) return Building::Workshop;
    if (knows(Tech::Religion) && !g.hasTemple) return knows(Tech::Masonry) ? Building::Temple : Building::Totem;
    if (knows(Tech::Metallurgy) && !g.hasForge) return Building::Forge;
    if (knows(Tech::Law) && !g.hasTownHall && pop >= 12) return Building::TownHall;
    if (knows(Tech::Masonry) && (g.atWar() || g.culture.militarism > 0.6f) && g.stage >= Stage::Town && m.rng.uniform() < 0.5f)
        return m.rng.uniform() < 0.8f ? Building::Wall : Building::Tower;
    if (knows(Tech::Law) && g.stage == Stage::Nation && g.stone > 20 && m.rng.uniform() < 0.1f) return Building::Castle;
    return Building::None;
}

void scoreGoals(Simulation& sim, Entity e, Mind& m, const Body& body, const Inventory& inv, const Social& so) {
    const SpeciesDef& sd = sim.species[body.species];
    const Genome& gn = sim.reg.get<GenomeC>(e).genome;
    Group* g = so.group >= 0 ? &sim.groups[so.group] : nullptr;
    const Personality& p = m.pers;
    auto& S = m.scores;
    S.fill(0);
    auto knows = [&](Tech t) { return m.known.test(ti(t)); };
    auto at = [&](GoalId id) -> float& { return S[static_cast<int>(id)]; };
    bool adult = isAdult(sim, body);
    long now = sim.tick;

    at(GoalId::Idle) = 0.05f;
    // --- Nhu cầu sinh tồn ---
    // Ăn: đường cong lồi — càng đói càng gấp, gần chết đói thì gần như tuyệt đối ưu tiên.
    if (body.hunger > 0.25f) at(GoalId::Eat) = std::pow((body.hunger - 0.2f) / 0.8f, 0.8f) * 1.3f + (body.hunger > 0.7f ? 0.6f : 0.f);
    if (body.fatigue > 0.5f) at(GoalId::Rest) = (body.fatigue - 0.4f) * 1.5f;
    if (body.cold > 0.4f && g) at(GoalId::Rest) += body.cold * 0.5f; // về nhà sưởi ấm
    if (m.stm.threat > 0.3f) at(GoalId::Flee) = std::min(1.6f, m.stm.threat * (0.4f + p.fearfulness) * 1.2f);
    // Bầy đông + hung hăng + biết săn theo bầy → không chạy mà cùng phản công
    if (m.stm.predators > 0 && m.stm.allies >= 3 && p.agreeableness < 0.5f && knows(Tech::PackHunting)) at(GoalId::Flee) *= 0.3f;
    if (m.stm.enemies > 0) {
        float d = 0.35f + (1 - p.agreeableness) * 0.4f - p.fearfulness * 0.3f + (so.role == Role::Soldier ? 0.35f : 0.f) +
                  (g && g->atWar() ? 0.25f : 0.f) + (body.lastAttacker != NullEntity ? 0.3f : 0.f);
        at(GoalId::Defend) = std::max(0.0f, d);
    }
    // --- Sinh sản: bị kìm hãm khi nhóm thiếu nhà ở hoặc thế giới quá đông ---
    if (adult && body.pregnancyTicks == 0 && body.reproCooldown == 0 && body.hunger < 0.5f && body.fatigue < 0.7f) {
        float r = 0.2f + gn[Gene::Fertility] * 0.35f;
        // Loài có trí khôn: kế hoạch hoá theo chỗ ở (nhà) và lương thực dự trữ của nhóm
        if (g && sd.dexterous) {
            int room = g->stage >= Stage::Tribe ? g->housing + 6 : 14;
            if (g->population() > room) r *= 0.3f;
            if (g->stage >= Stage::Village && g->food < g->population()) r *= 0.6f;
        }
        // Mật độ phụ thuộc (carrying capacity): quá đông đồng loại xung quanh, hoặc
        // loài đã chiếm quá nhiều chỗ trong thế giới → giảm mạnh ham muốn sinh sản.
        int pop = sim.species[body.species].population;
        int crowd = m.stm.allies;
        if (!sd.dexterous && crowd > 6) r *= 6.0f / crowd; // loài có trí khôn bị giới hạn bởi nhà ở thay vì mật độ
        if (body.hunger > 0.3f) r *= 0.5f;
        float cap = sim.cfg.maxPopulation * (sd.dexterous ? 0.5f : (sd.diet == Diet::Herbivore ? 0.12f : 0.08f));
        if (pop > cap) r = 0;
        else if (pop > cap * 0.7f) r *= 0.3f;
        if (sim.stats().population > sim.cfg.maxPopulation * 0.9f) r = 0;
        at(GoalId::Reproduce) = r;
    }
    // --- Xã hội ---
    if (sd.social != SocialType::Solitary) {
        float lonely = std::min(1.0f, (now - m.lastSocialTick) / 400.0f);
        at(GoalId::Socialize) = p.extraversion * 0.35f * lonely + (knows(Tech::Language) ? 0.08f : 0.f);
    }
    float bored = std::min(1.0f, (now - m.lastExploreTick) / 500.0f);
    at(GoalId::Explore) = 0.06f + p.openness * 0.22f * bored;
    if (so.role == Role::Child) at(GoalId::Explore) *= 0.5f;

    // --- Lao động, chế tạo, xây dựng (cần tay khéo hoặc tri thức) ---
    if (g && adult) {
        int pop = g->population();
        if (sd.dexterous) {
            float need = 0;
            if (g->food < pop * 4) need += 0.5f;
            if (g->wood < 10 + pop) need += 0.3f;
            if (knows(Tech::Masonry) && g->stone < 10 + pop) need += 0.25f;
            else if (g->stone < 4 + pop / 3) need += 0.12f; // tò mò nhặt đá về để dành
            float roleB = (so.role == Role::Gatherer || so.role == Role::Hunter || so.role == Role::Farmer) ? 0.2f : 0.f;
            at(GoalId::Work) = (0.12f + p.diligence * 0.3f) * need + roleB * need;
        }
        if (now % 7 == static_cast<long>(entityIndex(e) % 7) || m.buildType == Building::None) m.buildType = pickBuilding(sim, m, *g, sd);
        if (m.buildType != Building::None) {
            int reqT = buildingInfo(m.buildType).requiredTech;
            if (reqT < 0 || m.known.test(reqT))
                at(GoalId::Build) = 0.22f + p.diligence * 0.25f + (so.role == Role::Builder ? 0.35f : 0.f);
        }
        if (knows(Tech::Farming) && sd.dexterous) at(GoalId::Farm) = (so.role == Role::Farmer ? 0.55f : 0.08f) + p.diligence * 0.1f;
        if (knows(Tech::ScentMarking) && now - m.lastMarkTick > 350) at(GoalId::MarkTerritory) = 0.18f + (1 - p.agreeableness) * 0.2f;
        if (knows(Tech::Religion) && now - m.lastWorshipTick > 500)
            at(GoalId::Worship) = g->culture.piety * 0.4f + (so.role == Role::Priest ? 0.3f : 0.f);
        if (g->atWar() && g->warTarget >= 0 && body.hunger < 0.6f && (so.role == Role::Soldier || g->culture.militarism > 0.65f))
            at(GoalId::War) = 0.55f + g->culture.militarism * 0.3f;
        if (knows(Tech::Hierarchy) && g->leader != e && p.ambition > 0.7f && so.status > 0.4f)
            at(GoalId::Dominate) = (p.ambition - 0.6f) * 0.6f;
    }
    if (sd.dexterous && adult) {
        if (knows(Tech::StoneTools) && inv.toolTier == 0) at(GoalId::CraftTool) = 0.25f + p.openness * 0.2f;
        if (knows(Tech::Metallurgy) && inv.toolTier == 1) at(GoalId::CraftTool) = 0.2f;
        if (knows(Tech::Weapons) && inv.weaponTier == 0 &&
            (so.role == Role::Soldier || so.role == Role::Hunter || (g && g->culture.militarism > 0.55f)))
            at(GoalId::CraftWeapon) = 0.3f;
    }
    // Quán tính: giữ mục tiêu hiện tại một chút để không "đổi ý" liên tục
    if (m.goal != GoalId::Idle && !m.plan.done()) at(m.goal) += 0.12f;
}

// Bộ lọc hành động + chi phí phụ thuộc cá nhân cho GOAP.
void makeView(Simulation& sim, Entity e, const Mind& m, const Body& body, const Inventory& inv, const Social& so, PlannerAgentView& v) {
    (void)e;
    const SpeciesDef& sd = sim.species[body.species];
    for (int a = 0; a < kActionCount; ++a) {
        const ActionDef& d = actionDef(static_cast<ActionId>(a));
        bool ok = true;
        if (d.tech >= 0 && !m.known.test(d.tech)) ok = false;
        if (d.needsHands && !sd.dexterous) ok = false;
        if (d.needsGroup && so.group < 0) ok = false;
        if (d.diet == DietReq::Plants && !canEatPlants(sd)) ok = false;
        if (d.diet == DietReq::Meat && !canEatMeat(sd)) ok = false;
        v.available[a] = ok;
        // Học tăng cường: hành động hay thất bại → đắt hơn; kỹ năng cao → rẻ hơn
        float mul = 1.0f + m.stats[a].failRate() * 1.5f;
        if (d.skill >= 0) mul *= 1.0f - m.skill[d.skill] * 0.35f;
        v.costMul[a] = mul;
    }
    auto A = [](ActionId id) { return static_cast<int>(id); };
    // Ngữ cảnh cụ thể
    if (inv.weaponTier > 0) v.costMul[A(ActionId::Hunt)] *= 0.6f;
    if (m.known.test(ti(Tech::PackHunting)) && m.stm.allies > 0) v.costMul[A(ActionId::Hunt)] *= 0.6f;
    if (sd.diet == Diet::Carnivore) v.costMul[A(ActionId::Hunt)] *= 0.7f;
    if (so.role == Role::Gatherer) { v.costMul[A(ActionId::GatherWood)] *= 0.7f; v.costMul[A(ActionId::Forage)] *= 0.7f; }
    if (so.role == Role::Hunter) v.costMul[A(ActionId::Hunt)] *= 0.7f;
    if (so.group >= 0) {
        const Group& g = sim.groups[so.group];
        if (g.food <= 0) v.available[A(ActionId::EatFromStore)] = false;
        // Ưu tiên nộp loại vật liệu kho đang thiếu
        if (g.wood > g.food) v.costMul[A(ActionId::DepositWood)] *= 1.6f;
        else v.costMul[A(ActionId::DepositFood)] *= 1.6f;
    }
    if (m.stm.nearestPrey == NullEntity && !m.ltm.knows(FactType::HuntingGround)) v.costMul[A(ActionId::SearchPrey)] *= 1.5f;
}

// ---------------------------------------------------------------------------
// 6. HÀNH ĐỘNG — các executor
// ---------------------------------------------------------------------------
struct Ctx {
    Simulation& sim;
    Entity e;
    Mind& m;
    Transform& tr;
    Body& body;
    Inventory& inv;
    Social& so;
    const SpeciesDef& sd;
    Group* g;
};

// Chọn ô mục tiêu cho hành động thu thập: ưu tiên ô vừa nhìn thấy, sau đó tới trí nhớ dài hạn.
bool pickTarget(Ctx& c, Spot seen, FactType ft) {
    if (seen.valid()) { c.m.tx = seen.x; c.m.ty = seen.y; return true; }
    if (const Fact* f = c.m.ltm.best(ft, c.tr.pos)) { c.m.tx = f->x; c.m.ty = f->y; return true; }
    return false;
}

Res gatherResource(Ctx& c, FactType ft, Spot seen, uint8_t Tile::*field, int& invField, int amount, Stimulus stim) {
    if (c.m.actionTicks == 0 && !pickTarget(c, seen, ft)) return Res::Failed;
    if (!c.sim.world.inBounds(c.m.tx, c.m.ty)) return Res::Failed;
    Vec2 tgt = tileCenter(c.m.tx, c.m.ty);
    bool walk = c.sim.world.walkable(c.m.tx, c.m.ty);
    if (!moveTo(c.sim, c.e, c.tr, c.body, c.m, tgt, walk ? 0.6f : 1.4f)) return c.tr.stuck > 60 ? Res::Failed : Res::Running;
    Tile& t = c.sim.world.at(c.m.tx, c.m.ty);
    if (t.*field < 5) { c.m.ltm.contradict(ft, c.m.tx, c.m.ty); return Res::Failed; }
    // Thời gian làm việc giảm khi có công cụ
    int work = 18 - c.inv.toolTier * 6;
    if (c.m.actionTicks < work + 20) { c.m.actionTicks++; if (c.m.actionTicks < work) return Res::Running; }
    int take = std::min<int>(t.*field, 30 + c.inv.toolTier * 15);
    t.*field = static_cast<uint8_t>(t.*field - take);
    invField += amount + c.inv.toolTier;
    c.sim.world.markDirty(c.m.tx, c.m.ty);
    Brain::stimulate(c.sim, c.e, stim);
    if (c.inv.wood > 0 && c.inv.stone > 0) Brain::stimulate(c.sim, c.e, Stimulus::HandleStone);
    return Res::Done;
}

bool goHome(Ctx& c, float arrive) {
    if (!c.g) return true;
    Vec2 h = homePos(*c.g);
    return moveTo(c.sim, c.e, c.tr, c.body, c.m, h, arrive);
}

// Ăn một đơn vị thức ăn. Biết dùng lửa → nấu chín → bổ dưỡng hơn.
void eatUnit(Ctx& c) {
    float nutrition = 0.3f * (c.m.known.test(ti(Tech::FireUse)) ? 1.3f : 1.0f);
    c.body.hunger = std::max(0.0f, c.body.hunger - nutrition);
}

// Chọn ô xây dựng gần nhà, thưa thớt, có thể đi tới.
bool pickBuildSite(Ctx& c) {
    if (!c.g) return false;
    Building b = c.m.buildType;
    if (b == Building::Bridge && c.m.blockedWater.valid()) {
        int x = c.m.blockedWater.x, y = c.m.blockedWater.y;
        if (c.sim.world.isWater(x, y) && c.sim.world.at(x, y).terrain == Terrain::ShallowWater && c.sim.world.at(x, y).building == Building::None) {
            c.m.tx = x; c.m.ty = y; return true;
        }
        return false;
    }
    float r = 2.0f + std::sqrt(static_cast<float>(c.g->buildingsTotal)) * 1.3f;
    if (b == Building::Wall || b == Building::Tower) r = static_cast<float>(c.g->territoryRadius) * 0.8f;
    for (int i = 0; i < 30; ++i) {
        int x, y;
        if (b == Building::Wall || b == Building::Tower) {
            float a = c.m.rng.range(0, 6.2831853f);
            x = c.g->homeX + static_cast<int>(std::cos(a) * r);
            y = c.g->homeY + static_cast<int>(std::sin(a) * r);
        } else {
            x = c.g->homeX + c.m.rng.irange(-static_cast<int>(r), static_cast<int>(r));
            y = c.g->homeY + c.m.rng.irange(-static_cast<int>(r), static_cast<int>(r));
        }
        if (!c.sim.world.inBounds(x, y) || !c.sim.world.walkable(x, y)) continue;
        const Tile& t = c.sim.world.at(x, y);
        if (t.building != Building::None || t.terrain == Terrain::Farmland || t.fire) continue;
        if (t.owner >= 0 && t.owner != c.so.group) continue;
        // Giữ khoảng trống giữa các nhà (trừ tường) để tạo đường đi
        bool crowded = false;
        if (b != Building::Wall)
            for (int dy = -1; dy <= 1 && !crowded; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if ((dx || dy) && c.sim.world.inBounds(x + dx, y + dy)) {
                        Building nb = c.sim.world.at(x + dx, y + dy).building;
                        if (nb != Building::None && nb != Building::ScentMark && (dx == 0 || dy == 0)) { crowded = true; break; }
                    }
                }
        if (crowded) continue;
        c.m.tx = x; c.m.ty = y;
        return true;
    }
    return false;
}

Res exec(Ctx& c, ActionId a) {
    Simulation& sim = c.sim;
    Mind& m = c.m;
    switch (a) {
    case ActionId::EatCarried: {
        if (c.inv.food <= 0) return Res::Failed;
        if (++m.actionTicks < 4) return Res::Running;
        c.inv.food--; eatUnit(c);
        return c.body.hunger < 0.12f || c.inv.food == 0 ? Res::Done : (m.actionTicks = 0, Res::Running);
    }
    case ActionId::EatHere: {
        Tile& t = sim.world.at(tileX(c.tr.pos), tileY(c.tr.pos));
        if (t.food < 5) { m.ltm.contradict(FactType::FoodSource, tileX(c.tr.pos), tileY(c.tr.pos)); return Res::Failed; }
        if (++m.actionTicks % 5) return Res::Running;
        t.food = static_cast<uint8_t>(std::max(0, t.food - 12));
        c.body.hunger = std::max(0.0f, c.body.hunger - 0.1f);
        if (m.actionTicks % 25 == 0) Brain::stimulate(sim, c.e, Stimulus::EatPlants);
        return c.body.hunger < 0.12f ? Res::Done : Res::Running;
    }
    case ActionId::GoToFood: {
        if (m.actionTicks++ == 0 && !pickTarget(c, m.seenFood, FactType::FoodSource)) return Res::Failed;
        if (!moveTo(sim, c.e, c.tr, c.body, m, tileCenter(m.tx, m.ty), 0.5f)) return c.tr.stuck > 60 || m.actionTicks > 400 ? Res::Failed : Res::Running;
        if (sim.world.at(m.tx, m.ty).food < 10) { m.ltm.contradict(FactType::FoodSource, m.tx, m.ty); return Res::Failed; }
        return Res::Done;
    }
    case ActionId::SearchFood:
    case ActionId::Explore:
    case ActionId::SearchPrey: {
        if (m.actionTicks == 0) {
            Vec2 base = c.tr.pos;
            float r = 6 + m.pers.openness * 10;
            if (a == ActionId::SearchPrey) {
                if (const Fact* f = m.ltm.best(FactType::HuntingGround, c.tr.pos)) base = tileCenter(f->x, f->y), r = 4;
                // Săn theo bầy: đi theo con đầu đàn
                if (m.known.test(ti(Tech::PackHunting)) && c.g && c.g->leader != c.e && sim.reg.valid(c.g->leader))
                    base = sim.reg.get<Transform>(c.g->leader).pos, r = 3;
            }
            // Con non đi theo mẹ; thành viên nhóm không đi quá xa lãnh thổ
            if (c.so.role == Role::Child && sim.reg.valid(c.so.mother)) base = sim.reg.get<Transform>(c.so.mother).pos, r = 2;
            else if (c.g && a == ActionId::Explore && dist(c.tr.pos, homePos(*c.g)) > c.g->territoryRadius + 10) base = homePos(*c.g);
            Vec2 p = randomPointNear(sim, m, base, r);
            m.tx = tileX(p); m.ty = tileY(p);
        }
        m.actionTicks++;
        // Kiểm tra nhanh mục tiêu tìm kiếm mỗi vài tick
        if (a == ActionId::SearchFood && m.actionTicks % 8 == 0) {
            const Tile& t = sim.world.at(tileX(c.tr.pos), tileY(c.tr.pos));
            if (t.food > 25) { m.seenFood = {tileX(c.tr.pos), tileY(c.tr.pos)}; m.ltm.remember(FactType::FoodSource, m.seenFood.x, m.seenFood.y, sim.tick); return Res::Done; }
        }
        if (a == ActionId::SearchPrey && m.actionTicks % 6 == 0) {
            bool found = false;
            sim.grid.query(c.tr.pos, 7, [&](Entity o, Vec2) {
                if (found || o == c.e) return;
                const Body* ob = sim.reg.pool<Body>().tryGet(o);
                if (ob && !ob->dead && sim.species.canHunt(c.body.species, ob->species)) { m.stm.nearestPrey = o; found = true; }
            });
            if (found) return Res::Done;
        }
        // Nhặt công cụ cũ tìm thấy → "học ngược" công nghệ của nền văn minh khác
        if (a == ActionId::Explore && m.seenTool.valid() && dist2(c.tr.pos, tileCenter(m.seenTool.x, m.seenTool.y)) < 2.5f) {
            for (auto& it : sim.world.items())
                if (it.x == m.seenTool.x && it.y == m.seenTool.y && it.amount > 0 && (it.kind == ItemKind::OldTool || it.kind == ItemKind::OldWeapon)) {
                    it.amount = 0;
                    Brain::stimulate(sim, c.e, Stimulus::FindOldTool, 3);
                    Brain::stimulate(sim, c.e, Stimulus::HandleStone, 4);
                    if (it.techHint >= 0 && !m.known.test(it.techHint) && techPrereqsMet(static_cast<Tech>(it.techHint), m.known) &&
                        m.intellect >= techDef(it.techHint).minIntellect * 0.85f && m.rng.chance(0.5f))
                        Brain::learnTech(sim, c.e, static_cast<Tech>(it.techHint), false);
                    if (it.kind == ItemKind::OldTool) c.inv.toolTier = std::max<uint8_t>(c.inv.toolTier, 1);
                    else c.inv.weaponTier = std::max<uint8_t>(c.inv.weaponTier, 1);
                    m.seenTool.reset();
                    break;
                }
        }
        bool arrived = moveTo(sim, c.e, c.tr, c.body, m, tileCenter(m.tx, m.ty), 0.7f);
        if (arrived || m.actionTicks > 160 || c.tr.stuck > 40) {
            if (a == ActionId::Explore) m.lastExploreTick = sim.tick;
            return a == ActionId::Explore ? Res::Done : Res::Failed; // tìm không thấy → lên kế hoạch lại
        }
        return Res::Running;
    }
    case ActionId::Forage: {
        Tile& t = sim.world.at(tileX(c.tr.pos), tileY(c.tr.pos));
        if (t.food < 5) return Res::Failed;
        if (++m.actionTicks < 10 - c.inv.toolTier * 3) return Res::Running;
        int units = std::min(carryCap(c.sd) - c.inv.food, 1 + static_cast<int>(m.skillOf(Skill::Foraging) * 3));
        if (units <= 0) return Res::Done;
        t.food = static_cast<uint8_t>(std::max(0, t.food - units * 15));
        c.inv.food += units;
        Brain::stimulate(sim, c.e, Stimulus::EatPlants);
        return Res::Done;
    }
    case ActionId::Hunt: {
        if (m.actionTicks == 0) m.target = m.stm.nearestPrey;
        if (!sim.reg.valid(m.target) || sim.reg.get<Body>(m.target).dead) return m.actionTicks > 0 && c.inv.food > 0 ? Res::Done : Res::Failed;
        if (++m.actionTicks > 260) return Res::Failed;
        Transform& pt = sim.reg.get<Transform>(m.target);
        if (!moveTo(sim, c.e, c.tr, c.body, m, pt.pos, 0.9f)) return c.tr.stuck > 50 ? Res::Failed : Res::Running;
        if (m.actionTicks % 6) return Res::Running;
        // Săn theo bầy: mỗi đồng loại cùng nhóm ở gần cộng thêm 35% hiệu quả
        int helpers = 0;
        if (c.so.group >= 0)
            sim.grid.query(pt.pos, 4.0f, [&](Entity o, Vec2) {
                if (o == c.e) return;
                const Social* os = sim.reg.pool<Social>().tryGet(o);
                if (os && os->group == c.so.group) helpers++;
            });
        if (helpers > 0) Brain::stimulate(sim, c.e, Stimulus::GroupHunt);
        float mul = 1.0f + c.inv.weaponTier * 0.7f + c.inv.toolTier * 0.2f + m.skillOf(Skill::Hunting) * 0.6f;
        if (m.known.test(ti(Tech::PackHunting))) mul *= 1.0f + std::min(helpers, 5) * 0.35f;
        mul *= 0.7f + m.intellect * 0.8f; // phối hợp thông minh hơn → hiệu quả hơn
        if (Brain::attack(sim, c.e, m.target, mul)) {
            int meat = static_cast<int>(2 + sim.species[sim.reg.valid(m.target) ? sim.reg.get<Body>(m.target).species : 0].size * 4);
            int take = std::min(meat, carryCap(c.sd) + 1);
            c.inv.food += take;
            // Chia sẻ con mồi: cả bầy xung quanh đều được ăn (luật hoặc bản năng bầy)
            if (c.g && (c.g->laws.test(static_cast<int>(Law::ShareKills)) || c.sd.social == SocialType::Pack)) {
                sim.grid.query(c.tr.pos, 5.0f, [&](Entity o, Vec2) {
                    if (o == c.e) return;
                    const Social* os = sim.reg.pool<Social>().tryGet(o);
                    if (os && os->group == c.so.group) {
                        Body& ob = sim.reg.get<Body>(o);
                        ob.hunger = std::max(0.0f, ob.hunger - 0.4f);
                    }
                });
            }
            sim.world.dropItem(tileX(pt.pos), tileY(pt.pos), ItemKind::Corpse, std::max(0, meat - take));
            return Res::Done;
        }
        return Res::Running;
    }
    case ActionId::Scavenge: {
        if (!m.seenCorpse.valid()) return Res::Failed;
        if (!moveTo(sim, c.e, c.tr, c.body, m, tileCenter(m.seenCorpse.x, m.seenCorpse.y), 0.7f)) return c.tr.stuck > 40 ? Res::Failed : Res::Running;
        for (auto& it : sim.world.items())
            if (it.x == m.seenCorpse.x && it.y == m.seenCorpse.y && (it.kind == ItemKind::Corpse || it.kind == ItemKind::Meat) && it.amount > 0) {
                int take = std::min(it.amount, carryCap(c.sd));
                it.amount -= take; c.inv.food += take;
                return Res::Done;
            }
        m.seenCorpse.reset();
        return Res::Failed;
    }
    case ActionId::EatFromStore: {
        if (!c.g || c.g->food <= 0) return Res::Failed;
        if (++m.actionTicks < 5) return Res::Running;
        c.g->food--; eatUnit(c);
        m.actionTicks = 1;
        return c.body.hunger < 0.12f ? Res::Done : Res::Running;
    }
    case ActionId::GoHome: {
        if (!c.g) return Res::Failed;
        if (m.actionTicks++ == 0) { Vec2 p = randomPointNear(sim, m, homePos(*c.g), 1.5f); m.tx = tileX(p); m.ty = tileY(p); }
        if (moveTo(sim, c.e, c.tr, c.body, m, tileCenter(m.tx, m.ty), 0.8f)) return Res::Done;
        return c.tr.stuck > 80 || m.actionTicks > 600 ? Res::Failed : Res::Running;
    }
    case ActionId::SleepAtHome:
    case ActionId::SleepHere: {
        c.body.sleeping = true;
        float rate = a == ActionId::SleepAtHome ? 0.02f : 0.01f;
        c.body.fatigue = std::max(0.0f, c.body.fatigue - rate);
        if (a == ActionId::SleepAtHome) c.body.cold = std::max(0.0f, c.body.cold - 0.02f);
        if (c.body.fatigue < 0.04f && c.body.cold < 0.3f) { c.body.sleeping = false; return Res::Done; }
        return Res::Running;
    }
    case ActionId::Flee: {
        Vec2 away = (c.tr.pos - m.stm.threatPos).normalized();
        if (away.len2() < 0.1f) away = {m.rng.range(-1, 1), m.rng.range(-1, 1)};
        // Chạy về nhà nếu có nhóm (an toàn trong đám đông)
        Vec2 tgt = c.tr.pos + away * 6.0f;
        if (c.g && dist2(c.tr.pos, homePos(*c.g)) > 9) tgt = homePos(*c.g);
        moveTo(sim, c.e, c.tr, c.body, m, tgt, 0.5f);
        if (++m.actionTicks > 40 || dist2(c.tr.pos, m.stm.threatPos) > 100) return Res::Done;
        return Res::Running;
    }
    case ActionId::Fight: {
        if (m.actionTicks == 0) m.target = m.stm.nearestEnemy != NullEntity ? m.stm.nearestEnemy : m.stm.nearestThreat;
        if (!sim.reg.valid(m.target) || sim.reg.get<Body>(m.target).dead) return m.actionTicks > 0 ? Res::Done : Res::Failed;
        if (++m.actionTicks > 300) return Res::Failed;
        Transform& et = sim.reg.get<Transform>(m.target);
        if (dist2(et.pos, c.tr.pos) > 225) return Res::Failed; // địch chạy mất
        if (!moveTo(sim, c.e, c.tr, c.body, m, et.pos, 0.9f)) return Res::Running;
        if (m.actionTicks % 6) return Res::Running;
        float mul = 1.0f + c.inv.weaponTier * 0.8f + m.skillOf(Skill::Combat) * 0.6f;
        Brain::stimulate(sim, c.e, Stimulus::Combat);
        if (Brain::attack(sim, c.e, m.target, mul)) return Res::Done;
        return Res::Running;
    }
    case ActionId::SeekEnemy: {
        if (!c.g || c.g->warTarget < 0) return Res::Failed;
        Group* eg = sim.group(c.g->warTarget);
        if (!eg) return Res::Failed;
        m.actionTicks++;
        if (m.actionTicks % 10 == 0) {
            bool found = false;
            sim.grid.query(c.tr.pos, 8, [&](Entity o, Vec2) {
                if (found) return;
                const Social* os = sim.reg.pool<Social>().tryGet(o);
                if (os && os->group == eg->id) { m.stm.nearestEnemy = o; found = true; }
            });
            if (found) return Res::Done;
        }
        Vec2 tgt = homePos(*eg);
        if (moveTo(sim, c.e, c.tr, c.body, m, tgt, 1.5f)) {
            // Đã tới thủ phủ địch: phá công trình
            Tile& t = sim.world.at(tileX(c.tr.pos), tileY(c.tr.pos));
            (void)t;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    int x = tileX(c.tr.pos) + dx, y = tileY(c.tr.pos) + dy;
                    if (sim.world.inBounds(x, y) && sim.world.at(x, y).building != Building::None && sim.world.at(x, y).owner == eg->id) {
                        Tile& bt = sim.world.at(x, y);
                        bt.buildingHp = static_cast<uint8_t>(std::max(0, bt.buildingHp - 10));
                        if (bt.buildingHp == 0) sim.world.setBuilding(x, y, Building::None);
                    }
                }
            return m.actionTicks > 200 ? Res::Done : Res::Running;
        }
        return m.actionTicks > 900 || c.tr.stuck > 80 ? Res::Failed : Res::Running;
    }
    case ActionId::SeekMate:
    case ActionId::SeekAlly: {
        Entity t = a == ActionId::SeekMate ? m.stm.nearestMate : m.stm.nearestAlly;
        m.actionTicks++;
        if (sim.reg.valid(t)) {
            if (moveTo(sim, c.e, c.tr, c.body, m, sim.reg.get<Transform>(t).pos, 1.2f)) return Res::Done;
        } else if (c.g) {
            if (goHome(c, 2.0f)) return Res::Failed; // về nhà vẫn không thấy ai → thôi
        } else {
            if (m.actionTicks == 1) { Vec2 p = randomPointNear(sim, m, c.tr.pos, 10); m.tx = tileX(p); m.ty = tileY(p); }
            if (moveTo(sim, c.e, c.tr, c.body, m, tileCenter(m.tx, m.ty), 0.8f)) return Res::Failed;
        }
        return m.actionTicks > 300 || c.tr.stuck > 60 ? Res::Failed : Res::Running;
    }
    case ActionId::Mate: {
        Entity mate = m.stm.nearestMate;
        if (!sim.reg.valid(mate)) return Res::Failed;
        Body& mb = sim.reg.get<Body>(mate);
        if (mb.pregnancyTicks > 0 || mb.dead || c.body.pregnancyTicks > 0) return Res::Failed;
        if (++m.actionTicks < 8) return Res::Running;
        Body& fem = c.body.female ? c.body : mb;
        Entity father = c.body.female ? mate : c.e;
        fem.pregnancyTicks = sim.species[c.body.species].gestationTicks;
        fem.pregnantBy = father;
        c.body.reproCooldown = mb.reproCooldown = sim.species[c.body.species].gestationTicks / 2 + 60;
        c.so.mate = mate;
        if (auto* ms = sim.reg.tryGet<Social>(mate)) ms->mate = c.e;
        return Res::Done;
    }
    case ActionId::Socialize: {
        Entity o = m.stm.nearestAlly;
        if (!sim.reg.valid(o)) return Res::Failed;
        if (++m.actionTicks < 12) return Res::Running;
        Mind* om = sim.reg.tryGet<Mind>(o);
        m.lastSocialTick = sim.tick;
        if (!om) return Res::Done;
        om->lastSocialTick = sim.tick;
        Brain::stimulate(sim, c.e, Stimulus::Communicate);
        Brain::stimulate(sim, o, Stimulus::Communicate);
        // ---- Dạy học: truyền một tri thức mà mình biết còn người kia chưa biết ----
        bool lang = m.known.test(ti(Tech::Language)) && om->known.test(ti(Tech::Language));
        const SpeciesDef& osd = sim.species[sim.reg.get<Body>(o).species];
        int start = m.rng.irange(0, kTechCount - 1);
        for (int k = 0; k < kTechCount; ++k) {
            int t = (start + k) % kTechCount;
            if (!m.known.test(t) || om->known.test(t)) continue;
            const TechDef& td = techDef(t);
            if (!td.animalsCan && !osd.dexterous) continue;
            if (!techPrereqsMet(static_cast<Tech>(t), om->known) || om->intellect < td.minIntellect * 0.8f) continue;
            float p = (0.12f + (lang ? 0.35f : 0.0f) + m.skillOf(Skill::Social) * 0.2f) * sim.cfg.learningRate;
            const Social& oso = sim.reg.get<Social>(o);
            if (oso.role == Role::Child && c.g && c.g->laws.test(static_cast<int>(Law::TeachTheYoung))) p += 0.25f;
            if (m.rng.chance(p)) Brain::learnTech(sim, o, static_cast<Tech>(t), false);
            break;
        }
        // ---- Chia sẻ trí nhớ: "ở chỗ kia có thức ăn / có thú dữ" ----
        float shareP = lang ? 0.9f : 0.3f;
        for (FactType ft : {FactType::FoodSource, FactType::Danger, FactType::HuntingGround, FactType::OreSource}) {
            if (!m.rng.chance(shareP)) continue;
            if (const Fact* f = m.ltm.best(ft, c.tr.pos)) om->ltm.remember(ft, f->x, f->y, sim.tick, -1, f->confidence * 0.8f);
        }
        // Uy tín xã hội
        c.so.status = std::min(1.0f, c.so.status + 0.01f);
        return Res::Done;
    }
    case ActionId::GatherWood:
        return gatherResource(c, FactType::WoodSource, m.seenTree, &Tile::wood, c.inv.wood, 2, Stimulus::ChopWood);
    case ActionId::GatherStone:
        return gatherResource(c, FactType::StoneSource, m.seenStone, &Tile::stone, c.inv.stone, 2, Stimulus::HandleStone);
    case ActionId::GatherOre:
        return gatherResource(c, FactType::OreSource, m.seenOre, &Tile::ore, c.inv.ore, 1, Stimulus::SeeOre);
    case ActionId::CraftTool:
    case ActionId::CraftWeapon: {
        if (++m.actionTicks < 30 - static_cast<int>(m.skillOf(Skill::Crafting) * 15)) return Res::Running;
        bool metal = m.known.test(ti(Tech::Metallurgy)) && c.inv.ore > 0;
        if (c.inv.wood < 1 || c.inv.stone < 1) return Res::Failed;
        c.inv.wood--; c.inv.stone--;
        if (metal) c.inv.ore--;
        uint8_t tier = metal ? 2 : 1;
        if (a == ActionId::CraftTool) c.inv.toolTier = std::max(c.inv.toolTier, tier);
        else c.inv.weaponTier = std::max(c.inv.weaponTier, tier);
        Brain::stimulate(sim, c.e, Stimulus::HandleStone, 2);
        return Res::Done;
    }
    case ActionId::GatherMaterials: {
        if (m.buildType == Building::None || !c.g) return Res::Failed;
        const BuildingInfo& bi = buildingInfo(m.buildType);
        if (hasMaterialsFor(c.inv, m.buildType)) return Res::Done;
        // Ưu tiên lấy từ kho chung
        int needW = std::max(0, bi.wood - c.inv.wood), needS = std::max(0, bi.stone - c.inv.stone), needO = std::max(0, bi.ore - c.inv.ore);
        if ((needW == 0 || c.g->wood >= needW) && (needS == 0 || c.g->stone >= needS) && (needO == 0 || c.g->ore >= needO)) {
            if (!goHome(c, 2.0f)) return c.tr.stuck > 60 ? Res::Failed : Res::Running;
            c.g->wood -= needW; c.g->stone -= needS; c.g->ore -= needO;
            c.inv.wood += needW; c.inv.stone += needS; c.inv.ore += needO;
            return Res::Done;
        }
        // Tự đi khai thác phần còn thiếu (một "hành động con")
        Res r;
        if (needW > 0) r = gatherResource(c, FactType::WoodSource, m.seenTree, &Tile::wood, c.inv.wood, 2, Stimulus::ChopWood);
        else if (needS > 0) r = gatherResource(c, FactType::StoneSource, m.seenStone, &Tile::stone, c.inv.stone, 2, Stimulus::HandleStone);
        else if (m.known.test(ti(Tech::Mining))) r = gatherResource(c, FactType::OreSource, m.seenOre, &Tile::ore, c.inv.ore, 1, Stimulus::SeeOre);
        else return Res::Failed;
        if (r == Res::Done) { m.actionTicks = 0; return hasMaterialsFor(c.inv, m.buildType) ? Res::Done : Res::Running; }
        return r;
    }
    case ActionId::Build: {
        if (!c.g || m.buildType == Building::None) return Res::Failed;
        if (m.actionTicks == 0 && !pickBuildSite(c)) return Res::Failed;
        m.actionTicks++;
        Vec2 site = tileCenter(m.tx, m.ty);
        bool water = sim.world.isWater(m.tx, m.ty);
        if (!moveTo(sim, c.e, c.tr, c.body, m, site, water ? 1.5f : 1.2f)) return c.tr.stuck > 60 ? Res::Failed : Res::Running;
        if (m.actionTicks < 40 - static_cast<int>(m.skillOf(Skill::Building) * 20)) return Res::Running;
        if (!hasMaterialsFor(c.inv, m.buildType)) return Res::Failed;
        Tile& t = sim.world.at(m.tx, m.ty);
        if (t.building != Building::None) return Res::Failed;
        const BuildingInfo& bi = buildingInfo(m.buildType);
        c.inv.wood -= bi.wood; c.inv.stone -= bi.stone; c.inv.ore -= bi.ore;
        sim.world.setBuilding(m.tx, m.ty, m.buildType);
        t.owner = static_cast<int16_t>(c.so.group);
        if (bi.stone > 0) Brain::stimulate(sim, c.e, Stimulus::BuildStone, 2);
        if (m.buildType == Building::Bridge) m.blockedWater.reset();
        c.so.status = std::min(1.0f, c.so.status + 0.02f);
        c.g->buildingsTotal++;
        m.buildType = Building::None;
        return Res::Done;
    }
    case ActionId::DepositWood:
    case ActionId::DepositStone:
    case ActionId::DepositFood: {
        if (!c.g) return Res::Failed;
        if (!goHome(c, 2.0f)) return c.tr.stuck > 80 ? Res::Failed : Res::Running;
        if (a == ActionId::DepositWood) { c.g->wood += c.inv.wood; c.inv.wood = 0; }
        if (a == ActionId::DepositStone) { c.g->stone += c.inv.stone; c.g->ore += c.inv.ore; c.inv.stone = c.inv.ore = 0; }
        if (a == ActionId::DepositFood) { c.g->food += c.inv.food; c.inv.food = 0; }
        return Res::Done;
    }
    case ActionId::Farm: {
        if (!c.g) return Res::Failed;
        if (m.actionTicks == 0) {
            // Tìm ruộng chín gần nhà, nếu không có thì khai hoang ô cỏ mới
            int ox, oy;
            int hx = c.g->homeX, hy = c.g->homeY, R = std::max(4, c.g->territoryRadius);
            bool found = sim.world.findNearestTile(hx, hy, R, [&](const Tile& t, int, int) { return t.terrain == Terrain::Farmland && t.food > 120; }, ox, oy);
            if (!found)
                found = sim.world.findNearestTile(hx, hy, R, [&](const Tile& t, int x, int y) {
                    return (t.terrain == Terrain::Grass || t.terrain == Terrain::Savanna) && t.building == Building::None &&
                           (t.owner < 0 || t.owner == c.so.group) && ((x + y) % 3 != 0); // chừa lối đi
                }, ox, oy);
            if (!found) return Res::Failed;
            m.tx = ox; m.ty = oy;
        }
        m.actionTicks++;
        if (!moveTo(sim, c.e, c.tr, c.body, m, tileCenter(m.tx, m.ty), 0.6f)) return c.tr.stuck > 60 ? Res::Failed : Res::Running;
        if (m.actionTicks % 30) return Res::Running;
        Tile& t = sim.world.at(m.tx, m.ty);
        if (t.terrain != Terrain::Farmland) { sim.world.setTerrain(m.tx, m.ty, Terrain::Farmland); t.food = 20; t.owner = static_cast<int16_t>(c.so.group); }
        else if (t.food > 60) {
            int crop = 3 + static_cast<int>(m.skillOf(Skill::Farming) * 4) + c.inv.toolTier;
            t.food = 0;
            c.g->food += crop; // nộp thẳng vào kho (đóng thuế nếu có luật)
            sim.world.markDirty(m.tx, m.ty);
        }
        Brain::stimulate(sim, c.e, Stimulus::Settled);
        return Res::Done;
    }
    case ActionId::MarkTerritory: {
        if (!c.g) return Res::Failed;
        if (m.actionTicks == 0) {
            float ang = m.rng.range(0, 6.2831853f);
            float r = std::max(3.0f, c.g->territoryRadius * 0.9f);
            m.tx = c.g->homeX + static_cast<int>(std::cos(ang) * r);
            m.ty = c.g->homeY + static_cast<int>(std::sin(ang) * r);
            if (!sim.world.walkable(m.tx, m.ty)) return Res::Failed;
        }
        m.actionTicks++;
        if (!moveTo(sim, c.e, c.tr, c.body, m, tileCenter(m.tx, m.ty), 0.8f)) return c.tr.stuck > 60 || m.actionTicks > 500 ? Res::Failed : Res::Running;
        Tile& t = sim.world.at(m.tx, m.ty);
        if (t.building == Building::None) sim.world.setBuilding(m.tx, m.ty, Building::ScentMark);
        if (t.owner < 0) t.owner = static_cast<int16_t>(c.so.group);
        m.lastMarkTick = sim.tick;
        return Res::Done;
    }
    case ActionId::Worship: {
        if (!c.g) return Res::Failed;
        if (!goHome(c, 2.5f)) return c.tr.stuck > 60 ? Res::Failed : Res::Running;
        if (++m.actionTicks < 30) return Res::Running;
        m.lastWorshipTick = sim.tick;
        c.so.status = std::min(1.0f, c.so.status + (c.so.role == Role::Priest ? 0.02f : 0.005f));
        return Res::Done;
    }
    case ActionId::ChallengeLeader: {
        if (!c.g || !sim.reg.valid(c.g->leader) || c.g->leader == c.e) return Res::Failed;
        Entity L = c.g->leader;
        if (!moveTo(sim, c.e, c.tr, c.body, m, sim.reg.get<Transform>(L).pos, 1.0f)) return ++m.actionTicks > 400 ? Res::Failed : Res::Running;
        // Thách đấu (không chết người): so sức mạnh, máu, uy tín + may rủi
        Body& lb = sim.reg.get<Body>(L);
        Social& ls = sim.reg.get<Social>(L);
        float mine = c.body.damage * c.body.hp * (1 + c.so.status) * m.rng.range(0.6f, 1.4f);
        float theirs = lb.damage * lb.hp * (1 + ls.status) * m.rng.range(0.6f, 1.4f);
        c.body.hp *= 0.85f; lb.hp *= 0.85f;
        if (mine > theirs) {
            c.g->leader = c.e;
            c.so.role = Role::Leader; c.so.status = std::min(1.0f, c.so.status + 0.3f);
            ls.role = Role::None; ls.status *= 0.5f;
            if (c.g->stage >= Stage::Tribe)
                sim.chronicle.add(sim.tick, EventKind::Society, c.so.name + " đã lật đổ " + ls.name + " và trở thành thủ lĩnh " + c.g->displayName(),
                                  tileX(c.tr.pos), tileY(c.tr.pos));
            return Res::Done;
        }
        c.so.status *= 0.6f;
        m.pers.ambition = clamp01(m.pers.ambition - 0.1f); // thất bại → bớt tham vọng
        return Res::Failed;
    }
    default:
        return Res::Failed;
    }
}

// ---------------------------------------------------------------------------
// 7. HỌC từ kết quả hành động
// ---------------------------------------------------------------------------
void onActionResult(Simulation& sim, Entity e, Mind& m, ActionId a, bool success) {
    ActionStat& st = m.stats[static_cast<int>(a)];
    if (success) { if (st.success < 60000) st.success++; }
    else if (st.fail < 60000) st.fail++;
    // Kinh nghiệm cũ được "làm mờ" để thích nghi khi môi trường thay đổi
    if (st.success + st.fail > 40) { st.success = static_cast<uint16_t>(st.success * 0.75f); st.fail = static_cast<uint16_t>(st.fail * 0.75f); }
    const ActionDef& d = actionDef(a);
    if (success && d.skill >= 0) {
        float& s = m.skill[d.skill];
        s = std::min(1.0f, s + 0.015f * (1.0f - s) * sim.cfg.learningRate);
    }
    if (success) {
        m.experience += 1;
        // Trí tuệ "học được" tăng chậm theo kinh nghiệm, nhanh hơn nếu gen thông minh
        const Genome& gn = sim.reg.get<GenomeC>(e).genome;
        m.learned = std::min(0.25f, m.learned + 0.0006f * sim.cfg.learningRate * (0.5f + gn[Gene::Intelligence]));
    }
}

// Câu "suy nghĩ" hiển thị: "Mục tiêu: hành động đang làm" (gộp nếu trùng tên).
void thinkStep(Mind& m) {
    const char* g = goalName(m.goal);
    const char* a = actionDef(m.plan.current()).name;
    if (std::strcmp(g, a) == 0) m.think("%s", g);
    else m.think("%s: %s", g, a);
}

} // namespace

// ===========================================================================
// API công khai
// ===========================================================================
void Brain::initMind(Simulation& sim, Entity e, Mind& m, const Genome& g, Entity mother, Entity father) {
    m.rng.reseed((static_cast<uint64_t>(e) << 20) ^ sim.rng.next() ^ (static_cast<uint64_t>(sim.tick) << 40));
    m.pers = Personality::fromGenome(g, m.rng);
    m.ltm.capacity = 8 + static_cast<int>(g[Gene::Intelligence] * 30);
    m.nextThink = sim.tick + m.rng.irange(0, 5);
    m.lastSocialTick = m.lastExploreTick = sim.tick;
    // ---- Di truyền văn hoá: con cái thừa hưởng một phần tri thức của bố mẹ ----
    for (Entity parent : {mother, father}) {
        Mind* pm = sim.reg.tryGet<Mind>(parent);
        if (!pm) continue;
        for (int t = 0; t < kTechCount; ++t)
            if (pm->known.test(t) && m.rng.chance(0.4f)) m.known.set(t);
        // con non nhớ nơi bố mẹ biết có thức ăn
        if (const Fact* f = pm->ltm.best(FactType::FoodSource, sim.reg.get<Transform>(parent).pos))
            m.ltm.remember(FactType::FoodSource, f->x, f->y, sim.tick);
        m.pers.diligence = clamp01(lerp(m.pers.diligence, pm->pers.diligence, 0.3f)); // học theo nếp nhà
    }
    m.think("Vừa chào đời");
}

void Brain::updateIntellect(Simulation& sim, Entity e) {
    Mind& m = sim.reg.get<Mind>(e);
    const Body& b = sim.reg.get<Body>(e);
    const Genome& g = sim.reg.get<GenomeC>(e).genome;
    const Social& so = sim.reg.get<Social>(e);
    float adultTicks = static_cast<float>(sim.species[b.species].adultYears * kTicksPerYear);
    float maturity = clamp01(0.5f + 0.5f * b.ageTicks / std::max(1.0f, adultTicks));
    float culture = 0;
    if (so.group >= 0) culture = std::min(0.12f, sim.groups[so.group].knowledge.count() * 0.008f);
    m.intellect = clamp01(g[Gene::Intelligence] * maturity + m.learned + culture);
    m.ltm.capacity = 8 + static_cast<int>(m.intellect * 36);
}

void Brain::learnTech(Simulation& sim, Entity e, Tech t, bool invented) {
    Mind& m = sim.reg.get<Mind>(e);
    if (m.known.test(ti(t))) return;
    m.known.set(ti(t));
    m.lastDiscovery = ti(t);
    m.lastDiscoveryTick = sim.tick;
    m.learned = std::min(0.25f, m.learned + 0.01f);
    if (!invented) return;
    m.think("Eureka! Đã nghĩ ra: %s", techDef(t).name);
    if (!sim.worldFirsts.test(ti(t))) {
        sim.worldFirsts.set(ti(t));
        const Social& so = sim.reg.get<Social>(e);
        const Body& b = sim.reg.get<Body>(e);
        const Transform& tr = sim.reg.get<Transform>(e);
        std::string who = so.name + " (" + sim.species[b.species].name + (so.group >= 0 ? ", " + sim.groups[so.group].displayName() : "") + ")";
        sim.chronicle.add(sim.tick, EventKind::Discovery,
                          std::string("Lần đầu tiên trên thế giới: ") + who + " phát minh ra \"" + techDef(t).name + "\"",
                          tileX(tr.pos), tileY(tr.pos));
    }
}

void Brain::stimulate(Simulation& sim, Entity e, Stimulus s, int amount) {
    Mind* mp = sim.reg.tryGet<Mind>(e);
    if (!mp) return;
    Mind& m = *mp;
    uint16_t& ex = m.exposure[static_cast<int>(s)];
    ex = static_cast<uint16_t>(std::min(60000, ex + amount));
    const SpeciesDef& sd = sim.species[sim.reg.get<Body>(e).species];
    // Thử phát minh mọi tech dùng kích thích này
    for (int t = 0; t < kTechCount; ++t) {
        const TechDef& td = techDef(t);
        if (td.stimulus != s || m.known.test(t)) continue;
        if (!td.animalsCan && !sd.dexterous) continue;
        if (m.intellect < td.minIntellect || ex < td.exposureNeeded) continue;
        if (!techPrereqsMet(static_cast<Tech>(t), m.known)) continue;
        float p = (0.004f + 0.05f * (m.intellect - td.minIntellect + 0.05f)) * (0.4f + m.pers.openness) * sim.cfg.learningRate;
        if (m.rng.chance(p)) Brain::learnTech(sim, e, static_cast<Tech>(t), true);
    }
}

bool Brain::attack(Simulation& sim, Entity a, Entity v, float mul) {
    Body& ab = sim.reg.get<Body>(a);
    Body& vb = sim.reg.get<Body>(v);
    if (vb.dead) return false;
    float dmg = ab.damage * mul * sim.rng.range(0.7f, 1.3f);
    vb.hp -= dmg;
    vb.lastAttacker = a;
    vb.sleeping = false;
    if (Mind* vm = sim.reg.tryGet<Mind>(v)) { vm->needReplan = true; vm->nextThink = sim.tick; }
    // Con mồi to khoẻ phản đòn (gấu, quỷ)
    if (vb.damage > ab.damage * 0.8f && sim.rng.chance(0.4f)) ab.hp -= vb.damage * 0.5f * sim.rng.range(0.5f, 1.0f);
    if (vb.hp <= 0) {
        ab.kills++;
        const Social* as = sim.reg.tryGet<Social>(a);
        const Social* vs = sim.reg.tryGet<Social>(v);
        if (as && vs && as->group >= 0 && vs->group >= 0 && as->group != vs->group) sim.killEvents.push_back({as->group, vs->group});
        if (Social* asm_ = sim.reg.tryGet<Social>(a)) asm_->status = std::min(1.0f, asm_->status + 0.05f);
        sim.kill(v, 2);
        if (ab.hp <= 0) sim.kill(a, 2);
        return true;
    }
    if (ab.hp <= 0) sim.kill(a, 2);
    return false;
}

void Brain::think(Simulation& sim, Entity e) {
    Mind& m = sim.reg.get<Mind>(e);
    Transform& tr = sim.reg.get<Transform>(e);
    Body& body = sim.reg.get<Body>(e);
    Inventory& inv = sim.reg.get<Inventory>(e);
    Social& so = sim.reg.get<Social>(e);
    if (body.dead) return;

    // 1) QUAN SÁT
    perceive(sim, e, m, tr, body, so);
    // 2) SUY NGHĨ: chấm điểm mục tiêu
    scoreGoals(sim, e, m, body, inv, so);
    GoalId best = GoalId::Idle;
    float bestS = -1;
    for (int i = 0; i < kGoalCount; ++i)
        if (m.scores[i] > bestS) { bestS = m.scores[i]; best = static_cast<GoalId>(i); }
    // Bị tấn công thì luôn phản ứng ngay
    bool changed = best != m.goal;
    if (!changed && !m.needReplan && !m.plan.done()) { m.goalScore = bestS; return; }

    // 3) QUYẾT ĐỊNH: lập kế hoạch GOAP
    m.state = buildState(sim, m, tr, body, inv, so);
    PlannerAgentView view;
    makeView(sim, e, m, body, inv, so, view);
    Plan plan;
    GoalId chosen = best;
    if (!planActions(m.state, goalState(best), view, plan) || plan.len == 0) {
        // Không có cách đạt mục tiêu → thử mục tiêu điểm cao thứ nhì, rồi khám phá
        float second = -1; GoalId alt = GoalId::Explore;
        for (int i = 0; i < kGoalCount; ++i)
            if (static_cast<GoalId>(i) != best && m.scores[i] > second) { second = m.scores[i]; alt = static_cast<GoalId>(i); }
        if (!planActions(m.state, goalState(alt), view, plan) || plan.len == 0) {
            alt = GoalId::Explore;
            planActions(m.state, goalState(alt), view, plan);
        }
        chosen = alt;
    }
    m.goal = chosen;
    m.goalScore = m.scores[static_cast<int>(chosen)];
    m.plan = plan;
    m.actionTicks = 0;
    m.needReplan = false;
    if (m.plan.len > 0)
        thinkStep(m);
    else
        m.think("%s (không có kế hoạch)", goalName(m.goal));
}

void Brain::act(Simulation& sim, Entity e) {
    Mind& m = sim.reg.get<Mind>(e);
    Body& body = sim.reg.get<Body>(e);
    if (body.dead) return;
    Transform& tr = sim.reg.get<Transform>(e);
    Inventory& inv = sim.reg.get<Inventory>(e);
    Social& so = sim.reg.get<Social>(e);

    // Kích thích quan sát được trong pha song song → xử lý phát minh ở đây (tuần tự)
    for (int s = 0; s < kStimulusCount; ++s)
        if (m.pendingStim[s]) { int n = m.pendingStim[s]; m.pendingStim[s] = 0; stimulate(sim, e, static_cast<Stimulus>(s), n); }

    if (m.plan.done()) { m.needReplan = true; return; }
    Ctx c{sim, e, m, tr, body, inv, so, sim.species[body.species], so.group >= 0 ? sim.group(so.group) : nullptr};
    ActionId a = m.plan.current();
    if (a != ActionId::SleepAtHome && a != ActionId::SleepHere) body.sleeping = false; // (ghi ở pha tuần tự để tránh data race)
    Res r = exec(c, a);
    if (body.dead) return;
    if (r == Res::Running) return;
    onActionResult(sim, e, m, a, r == Res::Done);
    m.actionTicks = 0;
    tr.stuck = 0;
    if (r == Res::Done) {
        m.plan.idx++;
        if (m.plan.done()) { m.needReplan = true; m.nextThink = std::min(m.nextThink, sim.tick + 1); }
        else thinkStep(m);
    } else {
        m.think("%s: thất bại khi \"%s\", suy nghĩ lại...", goalName(m.goal), actionDef(a).name);
        m.plan.clear();
        m.needReplan = true;
        m.nextThink = std::min(m.nextThink, sim.tick + 2);
    }
}

} // namespace wb
