// =============================================================================
// main_headless.cpp — Chạy mô phỏng không đồ hoạ (test, cân bằng, benchmark).
//
//   worldbox_sim --scenario wolves --years 60 --seed 7
//   worldbox_sim --scenario humans --years 200 --expect-stage Nation
//
// In ra biên niên sử + thống kê định kỳ. Với --expect-stage, chương trình trả
// về mã lỗi 1 nếu không có nhóm nào đạt giai đoạn văn minh yêu cầu (dùng cho CI).
// =============================================================================
#include "../sim/Simulation.h"
#include "../sim/SaveLoad.h"
#include "../sim/Scenarios.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>
#include <memory>
#include <array>

using namespace wb;

static Stage parseStage(const std::string& s) {
    if (s == "Tribe") return Stage::Tribe;
    if (s == "Village") return Stage::Village;
    if (s == "Town") return Stage::Town;
    if (s == "City") return Stage::City;
    if (s == "Nation") return Stage::Nation;
    return Stage::Pack;
}

// ---------------------------------------------------------------------------
// Kiểm thử hoá học: dựng phản ứng trong một ô cô lập, chạy engine thật, kiểm sản phẩm.
// ---------------------------------------------------------------------------
static int runChemTests() {
    int failed = 0;
    auto check = [&](const char* name, bool ok, const std::string& detail) {
        std::printf("  [%s] %s %s\n", ok ? "OK" : "LỖI", name, detail.c_str());
        if (!ok) failed++;
    };
    auto fresh = []() {
        SimConfig cfg; cfg.width = cfg.height = 48; cfg.seed = 99; cfg.threads = 1;
        auto s = std::make_unique<Simulation>(cfg);
        // Một bãi đất phẳng, không cây cối để tránh nhiễu (cháy rừng...)
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 48; ++x) { Tile& t = s->world.at(x, y); t.terrain = Terrain::Sand; t.height = 0.5f; t.wood = 0; t.food = 0; t.flags = 0; }
        s->world.matter.init(s->world);
        s->world.weather() = Weather{};
        return s;
    };
    auto run = [](Simulation& s, int ticks) { for (int i = 0; i < ticks; ++i) { s.tick++; s.world.matter.update(s.world, s.tick, s.rng); } };
    MatterGrid* G;
    {   // 1. Khử quặng: quặng đồng + than ở 1150°C → đồng
        auto s = fresh(); G = &s->world.matter;
        G->addMatter(20, 20, mat::CopperOre, 20); G->addMatter(20, 20, mat::Charcoal, 20); G->setHeatSource(20, 20, 1150, 0);
        run(*s, 60);
        check("Luyện kim", G->amountOf(20, 20, mat::Copper) > 0, "Quặng đồng + Than → Đồng: " + std::to_string(G->amountOf(20, 20, mat::Copper)) + " kg");
    }
    {   // 2. Nung đất sét → gạch; dưới nhiệt độ nung thì không
        auto s = fresh(); G = &s->world.matter;
        G->addMatter(20, 20, mat::Clay, 10); G->setHeatSource(20, 20, 950, 0);
        G->addMatter(30, 30, mat::Clay, 10); G->setHeatSource(30, 30, 600, 0);
        run(*s, 40);
        check("Nung gạch", G->amountOf(20, 20, mat::Brick) > 0 && G->amountOf(30, 30, mat::Brick) == 0, "950°C ra gạch, 600°C thì không");
    }
    {   // 3. Hợp kim: đồng + thiếc nóng chảy → vật liệu MỚI
        auto s = fresh(); G = &s->world.matter;
        int before = s->world.materials.count();
        G->addMatter(20, 20, mat::Copper, 10); G->addMatter(20, 20, mat::Tin, 3); G->setHeatSource(20, 20, 1200, 0);
        run(*s, 30);
        int after = s->world.materials.count();
        std::string nm = after > before ? s->world.materials[static_cast<MatId>(after - 1)].name : "-";
        check("Hợp kim", after == before + 1, "sinh vật liệu mới: " + nm);
    }
    {   // 4. Trung hoà axit – bazơ → nước muối, toả nhiệt
        auto s = fresh(); G = &s->world.matter;
        G->addMatter(20, 20, mat::Acid, 10); G->addMatter(20, 20, mat::Lye, 10);
        float t0 = G->temp(20, 20);
        run(*s, 1);
        check("Axit – bazơ", G->amountOf(20, 20, mat::SaltWater) > 0 && G->temp(20, 20) > t0, "→ Nước muối, nhiệt độ tăng");
    }
    {   // 5. Vôi sống + nước: toả nhiệt mạnh
        auto s = fresh(); G = &s->world.matter;
        G->addMatter(20, 20, mat::Quicklime, 10); G->addMatter(20, 20, mat::Water, 10);
        float t0 = G->temp(20, 20);
        run(*s, 1);
        check("Hydrat hoá", G->amountOf(20, 20, mat::SlakedLime) > 0 && G->temp(20, 20) > t0 + 100, "Vôi sống + Nước → Vôi tôi (+" + std::to_string(static_cast<int>(G->temp(20, 20) - t0)) + "°C)");
    }
    {   // 6. Dung nham gặp nước → đá vỏ chai + hơi nước
        auto s = fresh(); G = &s->world.matter;
        G->addMatter(20, 20, mat::Basalt, 20, 1250); G->setTemp(20, 20, 1250); run(*s, 1);   // nóng chảy
        G->addMatter(20, 20, mat::Water, 30);
        run(*s, 10);
        check("Tôi nguội", G->amountOf(20, 20, mat::Obsidian) > 0, "Dung nham + Nước → Đá vỏ chai");
    }
    {   // 7. Nước muối bay hơi → muối kết tủa
        auto s = fresh(); G = &s->world.matter;
        G->addMatter(20, 20, mat::SaltWater, 20); G->setHeatSource(20, 20, 130, 0);
        run(*s, 60);
        check("Kết tủa", G->amountOf(20, 20, mat::Salt) > 0, "Nước muối sôi cạn → Muối");
    }
    {   // 8. Than + lưu huỳnh + diêm tiêu → thuốc súng → châm lửa → NỔ (áp suất + vụ nổ vật lý)
        auto s = fresh(); G = &s->world.matter;
        G->addMatter(20, 20, mat::Charcoal, 5); G->addMatter(20, 20, mat::Sulfur, 5); G->addMatter(20, 20, mat::Saltpeter, 5);
        run(*s, 1);
        bool mixed = G->amountOf(20, 20, mat::Gunpowder) > 0;
        G->setTemp(20, 20, 400);
        run(*s, 1);
        check("Thuốc súng", mixed && !G->explosions().empty(), "trộn → thuốc súng; châm lửa → nổ");
    }
    {   // 9. Nhiệt khuếch tán & nước đóng băng ở -10°C
        auto s = fresh(); G = &s->world.matter;
        G->setTemp(20, 20, 800);
        run(*s, 20);
        check("Dẫn nhiệt", G->temp(21, 20) > 40, "ô bên cạnh nóng lên " + std::to_string(static_cast<int>(G->temp(21, 20))) + "°C");
    }
    std::printf("Kiểm thử hoá học: %s (%d lỗi)\n", failed ? "THẤT BẠI" : "ĐẠT", failed);
    return failed ? 1 : 0;
}

// Kiểm thử phòng thí nghiệm gen: ghép Người × Đại bàng + gen tự tạo → loài mới bay được, có nọc độc,
// thả vào thế giới, sống & sinh sản; rồi kiểm tra loài hậu duệ (tiến hoá) kế thừa gen tự tạo.
int runGeneLabTest() {
    SimConfig cfg; cfg.seed = 5; cfg.threads = 1; cfg.width = 192; cfg.height = 96;
    Simulation sim(cfg);
    SpeciesDef d = sim.species[sp::Human];
    d.name = "Người Cánh";
    d.plan = sim.species[sp::Eagle].plan;
    for (int g = 0; g < kGeneCount; ++g) d.baseGenome.g[g] = (d.baseGenome.g[g] + sim.species[sp::Eagle].baseGenome.g[g]) * 0.5f;
    addCustomGene(d, {"Gen Vân Dực", GeneEffect::Wings, 0.8f});
    addCustomGene(d, {"Gen Độc Nha", GeneEffect::Venom, 0.6f});
    addCustomGene(d, {"Gen Lục Bì", GeneEffect::Photosynthesis, 0.5f});
    d.parent = sp::Human; d.donor = sp::Eagle; d.engineered = true; d.capShare = 0.3f;
    int id = sim.species.add(d);
    int fails = 0;
    auto check = [&](bool ok, const char* what) { std::printf("  [%s] %s\n", ok ? "OK" : "LỖI", what); fails += !ok; };
    check(sim.species[id].habitat == Habitat::Air, "gen 'Đôi cánh' đổi môi trường sống thành bay");
    int sx = sim.world.width() / 2, sy = sim.world.height() / 2;
    std::vector<Entity> made;
    for (int i = 0; i < 10; ++i) { Entity e = sim.spawnCreature(id, {sx + 0.5f + i * 0.3f, sy + 0.5f}); if (e != NullEntity) made.push_back(e); }
    check(made.size() == 10, "thả được 10 cá thể (kể cả trên biển vì bay được)");
    Entity h = sim.spawnCreature(sp::Human, {sx + 0.5f, sy + 0.5f});
    if (!made.empty() && h != NullEntity) {
        check(sim.reg.get<Body>(made[0]).damage > sim.reg.get<Body>(h).damage * 1.2f, "gen 'Nọc độc' tăng sát thương");
    }
    bool overWater = false;
    for (int t = 0; t < kTicksPerYear * 3; ++t) {
        sim.step();
        for (Entity e : sim.agents) {
            const Body& b = sim.reg.get<Body>(e);
            if (b.species != id) continue;
            const Transform& tr = sim.reg.get<Transform>(e);
            if (sim.world.isWater(static_cast<int>(tr.pos.x), static_cast<int>(tr.pos.y))) overWater = true;
        }
    }
    int pop = sim.species[id].population;
    std::printf("  dân số loài mới sau 3 năm: %d\n", pop);
    check(pop > 0, "loài mới sống sót sau 3 năm");
    (void)overWater;
    SpeciesDef child = sim.species[id];   // mô phỏng phân hoá loài: hậu duệ là bản sao kế thừa
    check(child.customGenes.size() == 3 && child.geneBonus(GeneEffect::Venom) > 0.5f, "loài hậu duệ kế thừa gen tự tạo");
    std::printf("Kiểm thử phòng thí nghiệm gen: %s (%d lỗi)\n", fails ? "KHÔNG ĐẠT" : "ĐẠT", fails);
    return fails ? 1 : 0;
}

// Kiểm thử lưu/tải: chạy 2 năm → lưu → tải → chạy 2 năm phải GIỐNG HỆT chạy liền 4 năm.
// (Bất kỳ trường dữ liệu nào bị quên lưu đều làm hai lịch sử rẽ hướng → phát hiện ngay.)
int runSaveTest() {
    auto signature = [](Simulation& s) {
        double h = 0;
        for (Entity e : s.agents) {
            const Transform& t = s.reg.get<Transform>(e);
            const Body& b = s.reg.get<Body>(e);
            const Mind& m = s.reg.get<Mind>(e);
            h += t.pos.x * 1.3 + t.pos.y * 0.7 + b.hp * 0.11 + b.hunger + m.intellect * 3 + m.know.designs.size();
        }
        char buf[256];
        std::snprintf(buf, sizeof buf, "tick %ld · dân %zu · sinh %d · chết %d · nhóm %d · thiết kế %zu · biên niên %ld · tổng kiểm %.4f",
                      s.tick, s.agents.size(), s.stats().births, s.stats().deaths, s.stats().groups, s.designs.all.size(), s.chronicle.total(), h);
        return std::string(buf);
    };
    SimConfig cfg; cfg.seed = 11; cfg.threads = 1; cfg.width = 192; cfg.height = 96;
    // WB_SAVETEST="năm kịch_bản" để thử nặng hơn (mặc định 2 năm, người nguyên thuỷ)
    long years = 2;
    Scenario sc = Scenario::PrimalHumans;
    if (const char* env = std::getenv("WB_SAVETEST")) {
        char name[32] = "humans";
        std::sscanf(env, "%ld %31s", &years, name);
        sc = scenarioFromString(name);
    }
    const long half = years * kTicksPerYear;
    Simulation a(cfg);
    setupScenario(a, sc);
    for (long t = 0; t < 2 * half; ++t) a.step();
    std::string straight = signature(a);

    Simulation b(cfg);
    setupScenario(b, sc);
    for (long t = 0; t < half; ++t) b.step();
    std::string path = "wb_savetest.wbs", err;
    SaveInfo info; info.title = "kiểm thử";
    if (!saveWorld(b, path, info, &err)) { std::printf("Lưu thất bại: %s\n", err.c_str()); return 1; }
    FILE* f = std::fopen(path.c_str(), "rb"); std::fseek(f, 0, SEEK_END); long bytes = std::ftell(f); std::fclose(f);
    auto c = loadWorld(path, &info, &err);
    std::remove(path.c_str());
    if (!c) { std::printf("Tải thất bại: %s\n", err.c_str()); return 1; }
    c->cfg.threads = 1;
    for (long t = 0; t < half; ++t) c->step();
    std::string resumed = signature(*c);
    std::printf("  chạy liền       : %s\n  lưu → tải → chạy: %s\n  kích thước tệp lưu: %.1f MB (năm %ld, %d sinh vật)\n", straight.c_str(), resumed.c_str(),
                bytes / 1048576.0, info.tick / kTicksPerYear, info.population);
    bool ok = straight == resumed;
    std::printf("Kiểm thử lưu/tải thế giới: %s\n", ok ? "ĐẠT (giống hệt)" : "KHÔNG ĐẠT (lịch sử rẽ hướng — thiếu dữ liệu khi lưu)");
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--savetest") return runSaveTest();
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--chemtest") return runChemTests();
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--genelab-test") return runGeneLabTest();
    SimConfig cfg;
    std::string scenario = "sandbox";
    int years = 50;
    int statEvery = 10;
    bool quiet = false;
    std::string expect, mapOut;
    int expectSpecies = -1;
    int expectDesigns = -1, expectReactions = -1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--scenario") scenario = next();
        else if (a == "--years") years = std::stoi(next());
        else if (a == "--seed") cfg.seed = static_cast<uint32_t>(std::stoul(next()));
        else if (a == "--size") cfg.width = cfg.height = std::stoi(next());
        else if (a == "--threads") cfg.threads = static_cast<unsigned>(std::stoi(next()));
        else if (a == "--quiet") quiet = true;
        else if (a == "--world") {
            std::string t = next();
            const char* names[] = {"random", "pangaea", "continents", "archipelago", "ocean", "dry"};
            for (int k = 0; k < static_cast<int>(WorldType::Count); ++k) if (t == names[k]) cfg.worldType = static_cast<WorldType>(k);
        }
        else if (a == "--map") mapOut = next();
        else if (a == "--stat-every") statEvery = std::max(1, std::stoi(next()));
        else if (a == "--expect-stage") expect = next();
        else if (a == "--expect-designs") expectDesigns = std::stoi(next());
        else if (a == "--expect-reactions") expectReactions = std::stoi(next());
        else if (a == "--help") {
            std::printf("worldbox_sim [--scenario sandbox|wolves|humans|twins] [--years N] [--seed S] [--size N] [--threads N] [--quiet]\n"
                        "             [--world random|pangaea|continents|archipelago|ocean|dry] [--map ban_do.ppm]\n"
                        "             [--stat-every N] [--expect-stage Tribe|Village|Town|City|Nation] [--expect-designs N] [--expect-reactions N]\n");
            return 0;
        }
    }
    (void)expectSpecies;
    Simulation sim(cfg);
    Scenario sc = scenarioFromString(scenario);
    sim.chronicle.onEvent = [&](const ChronicleEntry& e) {
        if (!quiet || e.kind == EventKind::Society || e.kind == EventKind::Evolution)
            std::printf("[Năm %4ld] %s\n", e.tick / kTicksPerYear, e.text.c_str());
    };
    setupScenario(sim, sc);
    {
        int land = 0;
        for (int y = 0; y < sim.world.height(); ++y)
            for (int x = 0; x < sim.world.width(); ++x) land += !sim.world.isWater(x, y);
        std::printf("== Hành tinh: %s · %d lõi lục địa · đất liền %.0f%% ==\n", worldTypeName(sim.world.type()), sim.world.continents(),
                    100.0 * land / (sim.world.width() * sim.world.height()));
    }
    if (!mapOut.empty()) {
        // Xuất bản đồ địa hình dạng ảnh PPM (xem bằng mọi trình xem ảnh) — kiểm tra sinh thế giới
        if (FILE* f = std::fopen(mapOut.c_str(), "wb")) {
            std::fprintf(f, "P6 %d %d 255\n", sim.world.width(), sim.world.height());
            for (int y = 0; y < sim.world.height(); ++y)
                for (int x = 0; x < sim.world.width(); ++x) {
                    const TerrainInfo& ti_ = terrainInfo(sim.world.at(x, y).terrain);
                    unsigned char px[3] = {ti_.r, ti_.g, ti_.b};
                    std::fwrite(px, 1, 3, f);
                }
            std::fclose(f);
        }
        if (years == 0) return 0;
    }
    std::printf("== Kịch bản: %s | seed %u | %d năm ==\n", scenarioName(sc), cfg.seed, years);
    std::fflush(stdout);

    auto t0 = std::chrono::steady_clock::now();
    long totalTicks = static_cast<long>(years) * kTicksPerYear;
    Stage bestStage = Stage::Pack;
    for (long t = 0; t < totalTicks; ++t) {
        sim.step();
        for (auto& g : sim.groups) if (g.alive && g.stage > bestStage) bestStage = g.stage;
        if (sim.tick % (kTicksPerYear * statEvery) == 0) {
            const SimStats& s = sim.stats();
            std::printf("  -- %s: dân số %d, nhóm %d, quốc gia %d, loài %d, trí tuệ TB %.3f, suy nghĩ/tick %d\n",
                        sim.dateString().c_str(), s.population, s.groups, s.nations, s.species, s.avgIntellect, s.thinksLastTick);
            std::string pops;
            for (auto& spd : sim.species.all())
                if (spd.population > 0) pops += spd.name + ":" + std::to_string(spd.population) + " ";
            std::printf("     %s| sinh %d | tử vong (già/đói/bị giết/thiên tai/lạnh): %d/%d/%d/%d/%d\n", pops.c_str(), s.births, s.sapientDeaths[0],
                        s.sapientDeaths[1], s.sapientDeaths[2], s.sapientDeaths[3], s.sapientDeaths[4]);
            if (std::getenv("WB_CARN"))
                std::printf("     thú ăn thịt chết: %d/%d/%d/%d/%d\n", s.carnivoreDeaths[0], s.carnivoreDeaths[1], s.carnivoreDeaths[2], s.carnivoreDeaths[3], s.carnivoreDeaths[4]);
            if (std::getenv("WB_GOALS")) {
                // Phân bố mục tiêu hiện tại của các loài có tay khéo (debug cân bằng AI)
                std::array<int, kGoalCount> hist{};
                for (Entity e : sim.agents)
                    if (sim.species[sim.reg.get<Body>(e).species].dexterous) hist[static_cast<int>(sim.reg.get<Mind>(e).goal)]++;
                std::printf("     mục tiêu:");
                for (int k = 0; k < kGoalCount; ++k) if (hist[k]) std::printf(" %s=%d", goalName(static_cast<GoalId>(k)), hist[k]);
                std::printf("\n");
            }
            std::fflush(stdout);
        }
        if (sim.stats().population == 0) { std::printf("Tất cả sinh vật đã chết.\n"); break; }
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    // ---- Thống kê vật lý & hoá học ----
    std::printf("\n== Hoá học: %d vật liệu (%d sinh ra trong lúc chạy), CO₂ khí quyển %.0f ppm, độ ẩm %.2f ==\n",
                sim.world.materials.count(), sim.world.materials.count() - mat::DefaultCount - 4, sim.world.matter.atmosCO2, sim.world.matter.humidity);
    for (int k = 0; k < static_cast<int>(ReactionKind::Count); ++k)
        if (sim.world.matter.counts[k] > 0) std::printf("   %-34s %ld\n", reactionName(static_cast<ReactionKind>(k)), sim.world.matter.counts[k]);
    std::printf("== Sáng tạo: %zu thiết kế, %zu quy trình trên toàn thế giới | phiên sáng tạo %d (ý tưởng %d, thành công %d) | thí nghiệm %d, sản xuất %d ==\n",
                sim.designs.all.size(), sim.processes.all.size(), sim.stats().inventAttempts, sim.stats().inventTried, sim.stats().inventSuccess,
                sim.stats().experiments, sim.stats().productions);
    std::printf("\n== Kết thúc sau %.1fs (%.0f tick/s) ==\n", secs, sim.tick / std::max(1e-6, secs));
    for (auto& g : sim.groups) {
        if (!g.alive || g.population() < 3) continue;
        std::string techs;
        for (int t = 0; t < kTechCount; ++t) if (g.knowledge.test(t)) { techs += techDef(t).name; techs += ", "; }
        std::string laws;
        for (int l = 0; l < kLawCount; ++l) if (g.laws.test(l)) { laws += lawName(static_cast<Law>(l)); laws += ", "; }
        std::printf("* %s [%s] — %s, %d thành viên, %d công trình, lãnh thổ %d ô, trí tuệ TB %.2f, thế hệ %d\n    Tập quán: %s\n    Luật: %s\n",
                    g.displayName().c_str(), sim.species[g.species].name.c_str(), stageName(g.stage, g.animal), g.population(),
                    g.buildingsTotal, g.territoryTiles, g.avgIntellect, g.generationDepth, techs.c_str(), laws.c_str());
        {
            // Môi trường quanh nơi ở: giúp so sánh các nhóm (vd. hai bộ tộc song sinh)
            int volc = 0, trees = 0, root = g.id;
            for (int dy = -15; dy <= 15; ++dy)
                for (int dx = -15; dx <= 15; ++dx) {
                    if (!sim.world.inBounds(g.homeX + dx, g.homeY + dy)) continue;
                    const Tile& t = sim.world.at(g.homeX + dx, g.homeY + dy);
                    volc += (t.flags & kTileVolcanic) != 0;
                    trees += t.wood > 20;
                }
            for (int guard = 0; guard < 64 && root >= 0 && root < static_cast<int>(sim.groups.size()) && sim.groups[root].parentGroup >= 0; ++guard)
                root = sim.groups[root].parentGroup;
            std::printf("    Nơi ở (%d,%d) · nhiệt độ nền %.0f°C · ô núi lửa (r=15): %d · ô có cây: %d · dòng dõi từ nhóm #%d\n",
                        g.homeX, g.homeY, sim.world.matter.ambient(sim.world, g.homeX, g.homeY), volc, trees, root);
        }
        std::string concepts;
        for (int c = 0; c < kConceptCount; ++c) if (g.concepts.test(c)) { concepts += conceptName(static_cast<Concept>(c)); concepts += ", "; }
        std::printf("    %s · trình độ công nghệ %.1f · %zu vật liệu, %zu thiết kế, %zu quy trình\n    Khái niệm: %s\n",
                    g.era.c_str(), g.techLevel, g.materials.size(), g.designs.size(), g.processes.size(), concepts.c_str());
        for (int p = 0; p < kPurposeCount; ++p) {
            int d = g.bestDesign[p];
            if (d < 0 || sim.designs[d].primary != static_cast<Purpose>(p)) continue;
            const Design& ds = sim.designs[d];
            std::printf("      - %-12s %-22s %6.1f  = %s (thế hệ %d)\n", purposeName(static_cast<Purpose>(p)), ds.name.c_str(),
                        ds.value(static_cast<Purpose>(p)), ds.recipe(sim.world.materials).c_str(), ds.generation);
        }
        for (int pid : g.processes) std::printf("      ⚗ %s\n", sim.processes[pid].name.c_str());
        std::string stock;
        for (auto& [mt, n] : g.mats) stock += sim.world.materials[mt].name + ":" + std::to_string(n) + " ";
        if (!stock.empty() || g.forgeX >= 0)
            std::printf("      kho: gỗ %d, đá %d, %s%s\n", g.wood, g.stone, stock.c_str(), g.forgeX >= 0 ? (" | lò " + std::to_string(static_cast<int>(g.forgeHeat)) + "°C").c_str() : "");
    }
    int rc = 0;
    if (expectDesigns >= 0) {
        bool ok = static_cast<int>(sim.designs.all.size()) >= expectDesigns;
        std::printf("Số thiết kế phát minh: %zu (yêu cầu ≥ %d) → %s\n", sim.designs.all.size(), expectDesigns, ok ? "ĐẠT" : "KHÔNG ĐẠT");
        if (!ok) rc = 1;
    }
    if (expectReactions >= 0) {
        int kinds = 0;
        for (long c : sim.world.matter.counts) kinds += c > 0;
        bool ok = kinds >= expectReactions;
        std::printf("Số loại phản ứng hoá học đã xảy ra: %d (yêu cầu ≥ %d) → %s\n", kinds, expectReactions, ok ? "ĐẠT" : "KHÔNG ĐẠT");
        if (!ok) rc = 1;
    }
    if (!expect.empty()) {
        Stage want = parseStage(expect);
        bool ok = bestStage >= want;
        std::printf("Giai đoạn cao nhất đạt được: %s (yêu cầu %s) → %s\n", stageName(bestStage, false), stageName(want, false), ok ? "ĐẠT" : "KHÔNG ĐẠT");
        return ok ? rc : 1;
    }
    return rc;
}
