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
#include "../sim/Scenarios.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

using namespace wb;

static Stage parseStage(const std::string& s) {
    if (s == "Tribe") return Stage::Tribe;
    if (s == "Village") return Stage::Village;
    if (s == "Town") return Stage::Town;
    if (s == "City") return Stage::City;
    if (s == "Nation") return Stage::Nation;
    return Stage::Pack;
}

int main(int argc, char** argv) {
    SimConfig cfg;
    std::string scenario = "sandbox";
    int years = 50;
    int statEvery = 10;
    bool quiet = false;
    std::string expect;
    int expectSpecies = -1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--scenario") scenario = next();
        else if (a == "--years") years = std::stoi(next());
        else if (a == "--seed") cfg.seed = static_cast<uint32_t>(std::stoul(next()));
        else if (a == "--size") cfg.width = cfg.height = std::stoi(next());
        else if (a == "--threads") cfg.threads = static_cast<unsigned>(std::stoi(next()));
        else if (a == "--quiet") quiet = true;
        else if (a == "--stat-every") statEvery = std::max(1, std::stoi(next()));
        else if (a == "--expect-stage") expect = next();
        else if (a == "--help") {
            std::printf("worldbox_sim [--scenario sandbox|wolves|humans] [--years N] [--seed S] [--size N] [--threads N] [--quiet] [--expect-stage Tribe|Village|Town|City|Nation]\n");
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
            std::printf("     %s| tử vong (già/đói/bị giết/thiên tai/lạnh): %d/%d/%d/%d/%d\n", pops.c_str(), s.sapientDeaths[0],
                        s.sapientDeaths[1], s.sapientDeaths[2], s.sapientDeaths[3], s.sapientDeaths[4]);
            std::fflush(stdout);
        }
        if (sim.stats().population == 0) { std::printf("Tất cả sinh vật đã chết.\n"); break; }
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("\n== Kết thúc sau %.1fs (%.0f tick/s) ==\n", secs, sim.tick / std::max(1e-6, secs));
    for (auto& g : sim.groups) {
        if (!g.alive || g.population() < 3) continue;
        std::string techs;
        for (int t = 0; t < kTechCount; ++t) if (g.knowledge.test(t)) { techs += techDef(t).name; techs += ", "; }
        std::string laws;
        for (int l = 0; l < kLawCount; ++l) if (g.laws.test(l)) { laws += lawName(static_cast<Law>(l)); laws += ", "; }
        std::printf("* %s [%s] — %s, %d thành viên, %d công trình, lãnh thổ %d ô, trí tuệ TB %.2f, thế hệ %d\n    Tri thức: %s\n    Luật: %s\n",
                    g.displayName().c_str(), sim.species[g.species].name.c_str(), stageName(g.stage, g.animal), g.population(),
                    g.buildingsTotal, g.territoryTiles, g.avgIntellect, g.generationDepth, techs.c_str(), laws.c_str());
    }
    if (!expect.empty()) {
        Stage want = parseStage(expect);
        bool ok = bestStage >= want;
        std::printf("Giai đoạn cao nhất đạt được: %s (yêu cầu %s) → %s\n", stageName(bestStage, false), stageName(want, false), ok ? "ĐẠT" : "KHÔNG ĐẠT");
        return ok ? 0 : 1;
    }
    return 0;
}
