// =============================================================================
// SaveLoad.cpp — Tuần tự hoá toàn bộ Simulation (xem SaveLoad.h).
//
// Một hàm io(Archive&, T&) duy nhất cho mỗi kiểu dữ liệu, dùng chung cho CẢ lưu lẫn
// tải (Archive biết mình đang ghi hay đọc) → không thể lệch nhau giữa hai chiều.
// Kiểu "thuần dữ liệu" (trivially copyable) ghi thẳng từng byte; kiểu có chuỗi /
// vector / map được liệt kê từng trường.
// =============================================================================
#include "SaveLoad.h"
#include "Simulation.h"
#include "../civ/SocietySystem.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <type_traits>

namespace wb {

namespace {
constexpr char kMagic[8] = {'W', 'B', 'S', 'A', 'V', 'E', '0', '1'};
constexpr uint32_t kFormatVersion = 1;

class Archive {
public:
    Archive(FILE* f, bool loading) : f_(f), loading_(loading) {}
    bool loading() const { return loading_; }
    bool ok() const { return ok_; }
    void fail() { ok_ = false; }
    void raw(void* p, size_t n) {
        if (!ok_ || n == 0) return;
        if (loading_) ok_ = std::fread(p, 1, n, f_) == n;
        else ok_ = std::fwrite(p, 1, n, f_) == n;
    }
    // Đọc kích thước vector/chuỗi có kiểm tra (tệp hỏng không làm cấp phát khổng lồ)
    uint32_t size(size_t n) {
        uint32_t v = static_cast<uint32_t>(n);
        raw(&v, sizeof v);
        if (loading_ && v > 200'000'000u) { ok_ = false; v = 0; }
        return v;
    }

private:
    FILE* f_;
    bool loading_;
    bool ok_ = true;
};

// ---------------------------------------------------------------------------
// io cơ bản
// ---------------------------------------------------------------------------
template <class T>
std::enable_if_t<std::is_trivially_copyable_v<T>> io(Archive& a, T& v) { a.raw(&v, sizeof(T)); }

inline void io(Archive& a, std::string& s) {
    uint32_t n = a.size(s.size());
    if (a.loading()) s.resize(n);
    if (n) a.raw(&s[0], n);
}

template <class A, class B> void io(Archive& a, std::pair<A, B>& p) { io(a, p.first); io(a, p.second); }

template <class T> void io(Archive& a, std::vector<T>& v) {
    uint32_t n = a.size(v.size());
    if (a.loading()) v.resize(n);
    if constexpr (std::is_trivially_copyable_v<T>) {
        if (n) a.raw(v.data(), n * sizeof(T));
    } else {
        for (auto& x : v) io(a, x);
    }
}

template <class T> void io(Archive& a, std::deque<T>& d) {
    uint32_t n = a.size(d.size());
    if (a.loading()) d.resize(n);
    for (auto& x : d) io(a, x);
}

template <class K, class V> void io(Archive& a, std::map<K, V>& m) {
    uint32_t n = a.size(m.size());
    if (a.loading()) {
        m.clear();
        for (uint32_t i = 0; i < n && a.ok(); ++i) { K k{}; V v{}; io(a, k); io(a, v); m.emplace(k, v); }
    } else {
        for (auto& [k, v] : m) { K kk = k; io(a, kk); io(a, v); }
    }
}

template <class T, size_t N> std::enable_if_t<!std::is_trivially_copyable_v<std::array<T, N>>> io(Archive& a, std::array<T, N>& arr) {
    for (auto& x : arr) io(a, x);
}

#define F(x) io(a, d.x)
} // namespace

// Truy cập trường private của các lớp (được khai báo friend)
struct SaveAccess {
    // ---- Vật liệu ----
    static void ioMat(Archive& a, MaterialDef& d) {
        F(name); F(formula); F(density); F(meltPoint); F(boilPoint); F(heatCapacity); F(conductivity); F(hardness);
        F(toughness); F(elasticity); F(viscosity); F(powder); F(porous); F(ignitionPoint); F(combustionHeat); F(acidity);
        F(reactivity); F(toxicity); F(nutrition); F(metal); F(organic); F(oxidizer); F(explosive); F(burnResidue); F(burnGas);
        F(pyrolysisTo); F(solidifiesTo); F(firesTo); F(fireTemp); F(fireGas); F(smeltsTo); F(smeltTemp); F(oxide);
        F(hydratesTo); F(hydrationHeat); F(decaysTo); F(decayGas); F(dissolvedSalt); F(r); F(g); F(b); F(parentA); F(parentB);
        F(generation); F(natural);
    }
    static void ioMaterials(Archive& a, MaterialRegistry& d) {
        uint32_t n = a.size(d.defs_.size());
        if (a.loading()) d.defs_.resize(n);
        for (auto& m : d.defs_) ioMat(a, m);
        F(exoticOres); F(seed_);
        std::vector<std::pair<uint32_t, MatId>> cache(d.alloyCache_.begin(), d.alloyCache_.end());
        io(a, cache);
        if (a.loading()) { d.alloyCache_.clear(); for (auto& [k, v] : cache) d.alloyCache_[k] = v; }
    }
    // ---- Lưới vật chất ----
    static void ioMatter(Archive& a, MatterGrid& d) {
        F(w_); F(h_); F(temp_); F(cells_); F(active_); F(nextActive_); F(activeFlag_); F(burning_);
        F(recent_); F(tick_); F(explosions_); F(curTick_); F(counts); F(atmosCO2); F(humidity); F(wind);
        if (a.loading()) d.temp2_.assign(d.temp_.size(), 0.0f);
    }
    // ---- Thế giới ----
    static void ioWorld(Archive& a, World& d) {
        F(w_); F(h_); F(type_); F(continents_); F(tiles_); F(under_); F(items_); F(weather_);
        F(underVersion); F(caveTiles); F(veinTiles);
        ioMaterials(a, d.materials);
        ioMatter(a, d.matter);
        if (a.loading()) {
            d.dirtyFlag_.assign(static_cast<size_t>(d.w_) * d.h_, 0);
            d.dirty_.clear();
            d.fullRedraw = true;
            WrapSpace::width = static_cast<float>(d.w_);
            WrapSpace::height = static_cast<float>(d.h_);
        }
    }
    // ---- Trí nhớ dài hạn ----
    static void ioLtm(Archive& a, LongTermMemory& d) { F(capacity); F(facts_); }
    static void ioChronicle(Archive& a, Chronicle& d) {
        uint32_t n = a.size(d.entries_.size());
        if (a.loading()) d.entries_.resize(n);
        for (auto& e : d.entries_) { io(a, e.tick); io(a, e.kind); io(a, e.text); io(a, e.x); io(a, e.y); }
        F(total_);
    }
    static void ioRegistryIds(Archive& a, Registry& d) {
        F(versions_); F(freeList_);
        uint64_t alive = d.alive_;
        io(a, alive);
        d.alive_ = static_cast<size_t>(alive);
    }
    static void ioSimPrivate(Archive& a, Simulation& d);
};

namespace {
// ---- Các kiểu có chuỗi / vector ----
void io(Archive& a, Language& d) { F(onsets); F(vowels); F(codas); F(codaChance); F(minSyl); F(maxSyl); F(familyName); }
void io(Archive& a, CustomGene& d) { F(name); F(effect); F(strength); }
void io(Archive& a, SpeciesDef& d) {
    F(name); F(diet); F(social); F(dexterous); F(monster); F(habitat); F(capShare); F(plan); F(size); F(baseHp); F(baseDamage);
    F(baseSpeed); F(adultYears); F(maxAgeYears); F(gestationTicks); F(litterMax); F(baseGenome); F(r); F(g); F(b); F(parent);
    F(donor); F(engineered); F(customGenes); F(bornTick); F(population);
}
void io(Archive& a, Design& d) {
    F(id); F(name); F(parts); F(props); F(quality); F(primary); F(parent); F(generation); F(group); F(inventor); F(tick); F(mainMat);
}
void io(Archive& a, Process& d) { F(id); F(kind); F(inputs); F(outputs); F(minTemp); F(lowOxygen); F(name); F(group); F(tick); }
void io(Archive& a, Knowhow& d) {
    F(concepts); F(materials); F(designs); F(processes); F(best); F(frustration); F(lastInvent); F(lastExperiment); F(inventions);
}
void io(Archive& a, Mind& d) {
    F(pers); F(stm); SaveAccess::ioLtm(a, d.ltm); F(known); F(exposure); F(skill); F(stats);
    F(intellect); F(learned); F(experience);
    F(goal); F(goalScore); F(scores); F(state); F(plan); F(actionTicks); F(target); F(tx); F(ty); F(buildType);
    F(rng); F(nextThink); F(thinkInterval); F(needReplan);
    F(seenFood); F(seenTree); F(seenStone); F(seenOre); F(seenCorpse); F(seenTool); F(seenFire); F(blockedWater); F(seenRuin);
    F(pendingStim);
    io(a, d.know); F(craftPurpose); F(idea); F(ideaPurpose); F(ideaParent); F(hasIdea); F(craftBlockedUntil); F(processChoice);
    F(seenReactions); F(seenReactionCount); F(seenPhys); F(seenPhysCount); F(lastShot);
    F(lastSocialTick); F(lastMarkTick); F(lastWorshipTick); F(lastExploreTick); F(lastDiscovery); F(lastDiscoveryTick);
    F(thought); F(possessed);
}
void io(Archive& a, Inventory& d) { F(food); F(wood); F(stone); F(mats); F(item); F(wear); }
void io(Archive& a, Social& d) { F(group); F(role); F(mother); F(father); F(mate); F(name); F(status); F(children); F(joinedTick); }
void io(Archive& a, Group& d) {
    F(id); F(alive); F(species); F(animal); F(stage); F(gov); F(name); F(nationName); F(deity); io(a, d.language); F(culture);
    F(laws); F(knowledge); F(parentGroup); F(colR); F(colG); F(colB); F(col2R); F(col2G); F(col2B); F(symbol);
    F(members); F(leader); F(homeX); F(homeY); F(territoryRadius); F(claimX); F(claimY); F(territoryTiles);
    F(food); F(wood); F(stone); F(mats);
    F(concepts); F(materials); F(designs); F(processes); F(bestDesign); F(techLevel); F(era); F(eraMat);
    F(hasHearth); F(hearthX); F(hearthY); F(forgeX); F(forgeY); F(forgeHeat); F(forgeAnnounced); F(forgeLitUntil);
    F(dwellings); F(housing); F(buildingsTotal); F(hasGranary); F(hasWorkshop); F(hasForge); F(hasTemple); F(hasTownHall);
    F(relations); F(warTarget); F(foundedTick); F(stageTick); F(settledTicks); F(lastSplitCheck); F(avgIntellect);
    F(peakPopulation); F(generationDepth); F(kills); F(losses);
}
void io(Archive& a, SaveInfo& d) {
    F(title); F(savedAt); F(tick); F(population); F(groups); F(nations); F(species); F(scenario); F(worldType); F(worldSize);
    F(camX); F(camY); F(camZoom);
}

// Kiểm tra "dấu vân tay" bố cục dữ liệu: tệp lưu từ bản build khác bố cục → từ chối (không đọc rác)
uint64_t layoutFingerprint() {
    uint64_t h = 1469598103934665603ull;
    for (size_t v : {sizeof(Tile), sizeof(UnderTile), sizeof(MatterCell), sizeof(ReactionEvent), sizeof(PhysBody), sizeof(Body),
                     sizeof(Transform), sizeof(Genome), sizeof(ShortTermMemory), sizeof(Fact), sizeof(Plan), sizeof(Rng),
                     sizeof(DesignProps), sizeof(Part), sizeof(GroundItem), sizeof(Relation), sizeof(SimStats), sizeof(long),
                     static_cast<size_t>(kGeneCount), static_cast<size_t>(kPurposeCount), static_cast<size_t>(kActionCount)}) {
        h ^= v;
        h *= 1099511628211ull;
    }
    return h;
}
#undef F
} // namespace

#define F(x) io(a, d.x)
void SaveAccess::ioSimPrivate(Archive& a, Simulation& d) {
    F(speciesLang_); F(focus_); F(stats_);
}
#undef F

namespace {
// Toàn bộ mô phỏng (sau phần đầu tệp)
void ioSimulation(Archive& a, Simulation& sim) {
    io(a, sim.cfg);
    io(a, sim.tick);
    io(a, sim.rng);
    SaveAccess::ioWorld(a, sim.world);
    io(a, sim.species.all());
    io(a, sim.groups);
    SaveAccess::ioChronicle(a, sim.chronicle);
    io(a, sim.worldFirsts);
    io(a, sim.designs.all);
    io(a, sim.designs.worldBest);
    io(a, sim.processes.all);
    io(a, sim.physics.bodies);
    io(a, sim.physics.events);
    io(a, sim.killEvents);
    io(a, sim.agents);
    SaveAccess::ioSimPrivate(a, sim);
    // ---- Sinh vật (ECS): định danh + mọi component, theo đúng thứ tự trong pool ----
    SaveAccess::ioRegistryIds(a, sim.reg);
    std::vector<Entity> order;
    if (!a.loading()) order = sim.reg.pool<Transform>().entities;
    io(a, order);
    for (Entity e : order) {
        if (!a.ok()) return;
        if (a.loading()) {
            sim.reg.emplace<Transform>(e); sim.reg.emplace<Body>(e); sim.reg.emplace<Inventory>(e);
            sim.reg.emplace<Social>(e); sim.reg.emplace<GenomeC>(e); sim.reg.emplace<Mind>(e);
        }
        io(a, sim.reg.get<Transform>(e));
        io(a, sim.reg.get<Body>(e));
        io(a, sim.reg.get<Inventory>(e));
        io(a, sim.reg.get<Social>(e));
        io(a, sim.reg.get<GenomeC>(e));
        io(a, sim.reg.get<Mind>(e));
    }
}

bool readHeader(Archive& a, SaveInfo& info, std::string* error) {
    char magic[8];
    a.raw(magic, 8);
    uint32_t ver = 0;
    uint64_t fp = 0;
    io(a, ver);
    io(a, fp);
    if (!a.ok() || std::memcmp(magic, kMagic, 8) != 0) { if (error) *error = "Không phải tệp lưu WorldBox AI"; return false; }
    if (ver != kFormatVersion || fp != layoutFingerprint()) {
        if (error) *error = "Tệp lưu của phiên bản game khác — không tải được";
        return false;
    }
    io(a, info);
    return a.ok();
}
} // namespace

bool saveWorld(Simulation& sim, const std::string& path, const SaveInfo& infoIn, std::string* error) {
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) { if (error) *error = "Không mở được tệp để ghi: " + tmp; return false; }
    Archive a(f, false);
    char magic[8];
    std::memcpy(magic, kMagic, 8);
    a.raw(magic, 8);
    uint32_t ver = kFormatVersion;
    uint64_t fp = layoutFingerprint();
    io(a, ver);
    io(a, fp);
    SaveInfo info = infoIn;
    info.tick = sim.tick;
    info.population = sim.stats().population;
    info.groups = sim.stats().groups;
    info.nations = sim.stats().nations;
    info.species = sim.stats().species;
    if (info.savedAt.empty()) {
        std::time_t t = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof buf, "%d/%m/%Y %H:%M", std::localtime(&t));
        info.savedAt = buf;
    }
    io(a, info);
    ioSimulation(a, sim);
    bool ok = a.ok();
    ok = (std::fclose(f) == 0) && ok;
    // Ghi ra tệp tạm rồi mới đổi tên → mất điện / tắt máy giữa chừng không làm hỏng bản lưu cũ
    if (ok) {
        std::remove(path.c_str());
        ok = std::rename(tmp.c_str(), path.c_str()) == 0;
    }
    if (!ok) { std::remove(tmp.c_str()); if (error && error->empty()) *error = "Ghi tệp lưu thất bại (hết bộ nhớ?)"; }
    return ok;
}

std::unique_ptr<Simulation> loadWorld(const std::string& path, SaveInfo* infoOut, std::string* error) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { if (error) *error = "Chưa có bản lưu"; return nullptr; }
    Archive a(f, true);
    SaveInfo info;
    if (!readHeader(a, info, error)) { std::fclose(f); return nullptr; }
    // Đọc cấu hình trước để dựng khung mô phỏng (thread pool...) rồi đọc phần còn lại vào đó
    long pos = std::ftell(f);
    SimConfig cfg;
    io(a, cfg);
    std::fseek(f, pos, SEEK_SET);
    auto sim = std::make_unique<Simulation>(cfg, Simulation::NoGenerate{});
    ioSimulation(a, *sim);
    std::fclose(f);
    if (!a.ok()) { if (error) *error = "Tệp lưu bị hỏng hoặc không đầy đủ"; return nullptr; }
    sim->grid.init(sim->world.width(), sim->world.height(), 8, true);
    if (infoOut) *infoOut = info;
    return sim;
}

bool readSaveInfo(const std::string& path, SaveInfo& info) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    Archive a(f, true);
    bool ok = readHeader(a, info, nullptr);
    std::fclose(f);
    return ok;
}

} // namespace wb
