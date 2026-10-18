// =============================================================================
// Ui.cpp — Vòng lặp khung hình: nhập liệu (chuột + cảm ứng), mô phỏng theo
// bước thời gian cố định, vẽ thế giới, thanh công cụ, bảng thông tin debug AI.
// =============================================================================
#include "Ui.h"
#include "Text.h"
#include "../ai/Brain.h"
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <algorithm>

namespace wb {

namespace {
constexpr float kTicksPerSecond = 20.0f;              // tốc độ 1x
const int kSpeeds[] = {0, 1, 4, 15, 50};              // hệ số tốc độ
const char* kSpeedLabels[] = {"||", "1x", "4x", "15x", "50x"};

const Color kPanel{18, 20, 28, 225};
const Color kPanelLight{40, 44, 60, 235};
const Color kAccent{90, 170, 255, 255};
const Color kText{230, 232, 240, 255};
const Color kMuted{150, 155, 170, 255};

struct ToolEntry { Power power; ToolCategory cat; };
const ToolEntry kTools[] = {
    {Power::Inspect, ToolCategory::Inspect},
    {Power::RaiseLand, ToolCategory::Terrain}, {Power::LowerLand, ToolCategory::Terrain},
    {Power::PaintWater, ToolCategory::Terrain}, {Power::PaintGrass, ToolCategory::Terrain},
    {Power::PaintForest, ToolCategory::Terrain}, {Power::PaintSand, ToolCategory::Terrain},
    {Power::PaintMountain, ToolCategory::Terrain}, {Power::PaintSnow, ToolCategory::Terrain},
    {Power::PaintLava, ToolCategory::Terrain},
    {Power::PourWater, ToolCategory::Matter}, {Power::PourLava, ToolCategory::Matter},
    {Power::PourAcid, ToolCategory::Matter}, {Power::PourOil, ToolCategory::Matter},
    {Power::Gunpowder, ToolCategory::Matter}, {Power::HeatUp, ToolCategory::Matter},
    {Power::CoolDown, ToolCategory::Matter}, {Power::Boulder, ToolCategory::Matter},
    {Power::OreVein, ToolCategory::Matter},
    {Power::Spawn, ToolCategory::Life},
    {Power::Fire, ToolCategory::Disaster}, {Power::Meteor, ToolCategory::Disaster},
    {Power::Lightning, ToolCategory::Disaster}, {Power::Earthquake, ToolCategory::Disaster},
    {Power::Plague, ToolCategory::Disaster},
    {Power::Rain, ToolCategory::Weather}, {Power::Drought, ToolCategory::Weather},
    {Power::Blessing, ToolCategory::Divine}, {Power::Smite, ToolCategory::Divine},
};
const char* kCategoryNames[] = {"Quan sát", "Địa hình", "Vật chất", "Sự sống", "Thiên tai", "Thời tiết", "Thần lực"};

// ---------------------------------------------------------------------------
// Tiện ích UI tức thời (immediate mode)
// ---------------------------------------------------------------------------
App* gApp = nullptr;

bool pointInRect(Vector2 p, Rectangle r) { return p.x >= r.x && p.y >= r.y && p.x <= r.x + r.width && p.y <= r.y + r.height; }

void panel(Rectangle r, Color c = kPanel) {
    gApp->uiRects.push_back(r);
    DrawRectangleRounded(r, 0.08f, 6, c);
}

// Nút bấm: kích hoạt khi NHẢ tay bên trong nút (tránh bấm nhầm khi vuốt).
bool button(Rectangle r, const std::string& label, bool active = false, Color tint = kPanelLight) {
    gApp->uiRects.push_back(r);
    Vector2 mp = GetMousePosition();
    bool hover = pointInRect(mp, r);
    Color bg = active ? kAccent : tint;
    if (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) bg = ColorBrightness(bg, 0.25f);
    DrawRectangleRounded(r, 0.25f, 6, bg);
    float fs = std::min(r.height * 0.42f, 18 * gApp->uiScale);
    float tw = ui::measureText(label, fs);
    if (tw > r.width - 6) { fs *= (r.width - 6) / tw; tw = ui::measureText(label, fs); }
    ui::drawText(label, r.x + (r.width - tw) / 2, r.y + (r.height - fs) / 2, fs, active ? BLACK : kText);
    bool clicked = hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && pointInRect(gApp->pressPos, r);
    return clicked;
}

void bar(float x, float y, float w, float h, float v, Color c) {
    DrawRectangleRec({x, y, w, h}, Color{0, 0, 0, 120});
    DrawRectangleRec({x, y, w * clamp01(v), h}, c);
}

std::string fmt(const char* f, ...) WB_PRINTF(1, 2);
std::string fmt(const char* f, ...) {
    char buf[256];
    va_list a;
    va_start(a, f);
    std::vsnprintf(buf, sizeof(buf), f, a);
    va_end(a);
    return buf;
}

// ---------------------------------------------------------------------------
// Khởi tạo thế giới theo kịch bản
// ---------------------------------------------------------------------------
void startWorld(App& app) {
    SimConfig cfg;
    cfg.width = cfg.height = app.worldSize;
    cfg.seed = static_cast<uint32_t>(GetRandomValue(1, 1000000));
#if defined(PLATFORM_ANDROID)
    cfg.maxPopulation = 3000; // điện thoại: giới hạn dân số thấp hơn để giữ 60 FPS
#endif
    app.sim = std::make_unique<Simulation>(cfg);
    setupScenario(*app.sim, app.scenario);
    app.renderer.init(*app.sim);
    app.cam.target = {app.worldSize * kTileWorld / 2, app.worldSize * kTileWorld / 2};
    app.cam.offset = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    app.cam.zoom = std::max(GetScreenWidth(), GetScreenHeight()) / (app.worldSize * kTileWorld) * 1.1f;
    // Kịch bản cụ thể: đưa camera tới nhóm đầu tiên
    if (app.scenario != Scenario::Sandbox && !app.sim->agents.empty()) {
        Vec2 p = app.sim->reg.get<Transform>(app.sim->agents.front()).pos;
        app.cam.target = {p.x * kTileWorld, p.y * kTileWorld};
        app.cam.zoom *= 3;
    }
    app.opt = RenderOptions{};
    app.panel = SidePanel::Chronicle;
    app.inMenu = false;
    app.speedIdx = 1;
}

void clampCamera(App& app) {
    float W = app.sim->world.width() * kTileWorld, H = app.sim->world.height() * kTileWorld;
    app.cam.zoom = std::clamp(app.cam.zoom, 0.3f, 12.0f);
    app.cam.target.x = std::clamp(app.cam.target.x, 0.0f, W);
    app.cam.target.y = std::clamp(app.cam.target.y, 0.0f, H);
}

// ---------------------------------------------------------------------------
// Nhập liệu bản đồ: chạm/chuột/pinch
// ---------------------------------------------------------------------------
bool overUi(App& app, Vector2 p) {
    for (const Rectangle& r : app.uiRectsPrev) if (pointInRect(p, r)) return true;
    return false;
}

void selectAt(App& app, Vector2 worldPx) {
    Simulation& sim = *app.sim;
    Vec2 wp{worldPx.x / kTileWorld, worldPx.y / kTileWorld};
    Entity best = NullEntity;
    float bd = (1.5f / std::min(1.0f, app.cam.zoom / 2)) * (1.5f / std::min(1.0f, app.cam.zoom / 2));
    sim.grid.query(wp, 3.0f, [&](Entity e, Vec2 p) {
        float d = dist2(p, wp);
        if (d < bd) { bd = d; best = e; }
    });
    if (best != NullEntity) {
        app.opt.selected = best;
        app.panel = SidePanel::Creature;
        return;
    }
    // Chạm vào lãnh thổ → xem thông tin nhóm
    int tx = static_cast<int>(wp.x), ty = static_cast<int>(wp.y);
    if (sim.world.inBounds(tx, ty) && sim.world.at(tx, ty).owner >= 0) {
        app.opt.selectedGroup = sim.world.at(tx, ty).owner;
        app.panel = SidePanel::Group;
        return;
    }
    app.opt.selected = NullEntity;
}

void handleWorldInput(App& app) {
    Simulation& sim = *app.sim;
    float dt = GetFrameTime();
    int touches = GetTouchPointCount();
    // ---- Hai ngón: chụm thu phóng + kéo ----
    if (touches >= 2) {
        Vector2 a = GetTouchPosition(0), b = GetTouchPosition(1);
        Vector2 mid{(a.x + b.x) / 2, (a.y + b.y) / 2};
        float d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
        if (!app.pinching) {
            app.pinching = true; app.pinchStartDist = std::max(10.0f, d); app.pinchStartZoom = app.cam.zoom; app.pinchLastMid = mid;
        } else {
            Vector2 before = GetScreenToWorld2D(mid, app.cam);
            app.cam.zoom = app.pinchStartZoom * d / app.pinchStartDist;
            clampCamera(app);
            Vector2 after = GetScreenToWorld2D(mid, app.cam);
            app.cam.target.x += before.x - after.x; app.cam.target.y += before.y - after.y;
            app.cam.target.x -= (mid.x - app.pinchLastMid.x) / app.cam.zoom;
            app.cam.target.y -= (mid.y - app.pinchLastMid.y) / app.cam.zoom;
            app.pinchLastMid = mid;
        }
        app.pointerDownOnWorld = false; // huỷ thao tác 1 ngón
        return;
    }
    if (app.pinching && touches == 0) app.pinching = false;
    if (app.pinching) return;

    Vector2 mp = GetMousePosition();
    // ---- Lăn chuột: zoom quanh con trỏ ----
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && !overUi(app, mp)) {
        Vector2 before = GetScreenToWorld2D(mp, app.cam);
        app.cam.zoom *= std::pow(1.15f, wheel);
        clampCamera(app);
        Vector2 after = GetScreenToWorld2D(mp, app.cam);
        app.cam.target.x += before.x - after.x; app.cam.target.y += before.y - after.y;
    }
    // ---- Chuột phải / giữa: kéo bản đồ ----
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 d = GetMouseDelta();
        app.cam.target.x -= d.x / app.cam.zoom; app.cam.target.y -= d.y / app.cam.zoom;
    }
    // ---- Bàn phím (PC) ----
    float pan = 400 * dt / app.cam.zoom;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) app.cam.target.x -= pan;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) app.cam.target.x += pan;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) app.cam.target.y -= pan;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) app.cam.target.y += pan;

    // ---- Một ngón / chuột trái ----
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        app.pressPos = mp;
        app.lastPointer = mp;
        app.dragMoved = false;
        app.pointerDownOnWorld = !overUi(app, mp);
    }
    if (app.pointerDownOnWorld && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 d{mp.x - app.lastPointer.x, mp.y - app.lastPointer.y};
        if (std::fabs(mp.x - app.pressPos.x) + std::fabs(mp.y - app.pressPos.y) > 12 * app.uiScale) app.dragMoved = true;
        if (app.tool == Power::Inspect || !powerIsBrush(app.tool)) {
            // Quan sát / công cụ chạm một lần: kéo = di chuyển bản đồ
            if (app.dragMoved) { app.cam.target.x -= d.x / app.cam.zoom; app.cam.target.y -= d.y / app.cam.zoom; }
        } else {
            // Công cụ vẽ: giữ & kéo để vẽ liên tục
            app.lastApply += dt;
            if (app.lastApply > 0.04f) {
                app.lastApply = 0;
                Vector2 w = GetScreenToWorld2D(mp, app.cam);
                applyPower(sim, app.tool, static_cast<int>(w.x / kTileWorld), static_cast<int>(w.y / kTileWorld), app.brush, app.spawnSpecies);
            }
        }
        app.lastPointer = mp;
    }
    if (app.pointerDownOnWorld && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app.pointerDownOnWorld = false;
        if (!app.dragMoved) {
            Vector2 w = GetScreenToWorld2D(mp, app.cam);
            if (app.tool == Power::Inspect) selectAt(app, w);
            else if (!powerIsBrush(app.tool))
                applyPower(sim, app.tool, static_cast<int>(w.x / kTileWorld), static_cast<int>(w.y / kTileWorld),
                           app.tool == Power::Spawn ? std::max(1, app.brush) : app.brush, app.spawnSpecies);
        }
    }
    clampCamera(app);
}

// ---------------------------------------------------------------------------
// Các bảng thông tin
// ---------------------------------------------------------------------------
// Ngắt dòng theo khoảng trắng sao cho mỗi dòng vừa độ rộng maxW. Luôn tiến về phía
// trước (mỗi vòng lặp tiêu thụ ít nhất 1 từ) nên không thể lặp vô hạn.
std::vector<std::string> wrapText(const std::string& text, float fs, float maxW) {
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < text.size()) {
        size_t j = text.find(' ', i);
        if (j == std::string::npos) j = text.size();
        std::string word = text.substr(i, j - i);
        std::string cand = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && ui::measureText(cand, fs) > maxW) { lines.push_back(cur); cur = word; }
        else cur = cand;
        i = j + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

struct Writer {
    float x, y, w, fs;
    void wrapped(const std::string& s, Color c = kText, float maxY = 1e9f) {
        for (const std::string& l : wrapText(s, fs * 0.9f, w)) {
            if (y > maxY) break;
            ui::drawText(l, x, y, fs * 0.9f, c);
            y += fs * 1.15f;
        }
    }
    void line(const std::string& s, Color c = kText) { ui::drawText(s, x, y, fs, c); y += fs * 1.25f; }
    void title(const std::string& s, Color c = kAccent) { ui::drawText(s, x, y, fs * 1.2f, c); y += fs * 1.5f; }
    void gap() { y += fs * 0.4f; }
    void meter(const std::string& label, float v, Color c) {
        ui::drawText(label, x, y, fs * 0.9f, kMuted);
        bar(x + w * 0.42f, y + fs * 0.25f, w * 0.55f, fs * 0.55f, v, c);
        y += fs * 1.15f;
    }
};

void drawCreaturePanel(App& app, Rectangle r) {
    Simulation& sim = *app.sim;
    Entity e = app.opt.selected;
    if (!sim.reg.valid(e)) { app.panel = SidePanel::Chronicle; return; }
    const Body& b = sim.reg.get<Body>(e);
    const Mind& m = sim.reg.get<Mind>(e);
    const Social& so = sim.reg.get<Social>(e);
    const Inventory& inv = sim.reg.get<Inventory>(e);
    const Genome& g = sim.reg.get<GenomeC>(e).genome;
    const SpeciesDef& sd = sim.species[b.species];
    Writer w{r.x + 10 * app.uiScale, r.y + 8 * app.uiScale, r.width - 20 * app.uiScale, 13 * app.uiScale};
    w.title(so.name + " — " + sd.name + (b.female ? " (cái)" : " (đực)"));
    const Group* grp = so.group >= 0 ? &sim.groups[so.group] : nullptr;
    w.line(fmt("Tuổi %.1f · %s · Thế hệ %d", sim.yearsOf(b.ageTicks), roleName(so.role), g.generation), kMuted);
    if (grp) w.line("Thuộc: " + grp->displayName() + " (" + stageName(grp->stage, grp->animal) + ")", groupColor(sim, so.group));
    w.gap();
    w.line(std::string("Suy nghĩ: \"") + m.thought + "\"", Color{255, 230, 150, 255});
    w.line(fmt("Trí tuệ %.2f — %s · Tính cách: %s", m.intellect, sapienceName(m.sapience()), m.pers.archetype()));
    w.meter("Máu", b.hp / b.maxHp, Color{220, 70, 70, 255});
    w.meter("Đói", b.hunger, Color{230, 160, 60, 255});
    w.meter("Mệt", b.fatigue, Color{150, 150, 230, 255});
    w.meter("Lạnh", b.cold, Color{120, 200, 255, 255});
    w.meter("Trí tuệ", m.intellect, SKYBLUE);
    w.gap();
    // Kế hoạch GOAP hiện tại
    w.line(std::string("Mục tiêu: ") + goalName(m.goal), kAccent);
    std::string plan;
    for (int i = 0; i < m.plan.len; ++i) {
        if (i) plan += " → ";
        if (i == m.plan.idx) plan += "[";
        plan += actionDef(m.plan.steps[i]).name;
        if (i == m.plan.idx) plan += "]";
    }
    if (!plan.empty()) w.wrapped(plan);
    // Top điểm Utility
    std::array<int, kGoalCount> idx;
    for (int i = 0; i < kGoalCount; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](int a, int c) { return m.scores[a] > m.scores[c]; });
    for (int k = 0; k < 4; ++k)
        if (m.scores[idx[k]] > 0) w.meter(fmt("%s %.2f", goalName(static_cast<GoalId>(idx[k])), m.scores[idx[k]]), m.scores[idx[k]] / 1.6f, Color{120, 220, 140, 255});
    w.gap();
    // Tri thức
    std::string techs;
    for (int t = 0; t < kTechCount; ++t) if (m.known.test(t)) { if (!techs.empty()) techs += ", "; techs += techDef(t).name; }
    w.line("Tập quán: " + (techs.empty() ? std::string("(chưa có)") : ""), kMuted);
    if (!techs.empty()) w.wrapped(techs, kText, r.y + r.height - w.fs * 6);
    // Tri thức vật chất MỞ: khái niệm, thiết kế tốt nhất, ý tưởng đang theo đuổi
    {
        const Knowhow& K = m.know;
        std::string cs;
        for (int k = 0; k < kConceptCount; ++k) if (K.concepts.test(k)) { if (!cs.empty()) cs += ", "; cs += conceptName(static_cast<Concept>(k)); }
        if (!cs.empty()) w.wrapped("Hiểu: " + cs, Color{170, 220, 255, 255}, r.y + r.height - w.fs * 7);
        for (int p = 0; p < kPurposeCount && w.y < r.y + r.height - w.fs * 6; ++p) {
            int d = K.best[p];
            if (d < 0 || sim.designs[d].primary != static_cast<Purpose>(p)) continue;
            const Design& ds = sim.designs[d];
            bool held = inv.item[p] >= 0;
            w.line(fmt("%s %s: %s %.1f", held ? "■" : "□", purposeName(static_cast<Purpose>(p)), ds.name.c_str(), ds.value(static_cast<Purpose>(p))),
                   held ? kText : kMuted);
        }
        if (m.hasIdea) {
            std::string idea;
            for (const Part& pt : m.idea) { if (!idea.empty()) idea += " + "; idea += sim.world.materials[pt.mat].name + " " + shapeName(pt.shape); }
            w.wrapped(std::string("Ý tưởng (") + purposeName(m.ideaPurpose) + "): " + idea, Color{255, 200, 255, 255}, r.y + r.height - w.fs * 5);
        }
        w.line(fmt("Biết %zu vật liệu · %zu thiết kế · %zu quy trình · %d phát minh", K.materials.size(), K.designs.size(), K.processes.size(), K.inventions), kMuted);
    }
    // Kỹ năng nổi bật + trí nhớ
    std::string skills;
    for (int s = 0; s < kSkillCount; ++s)
        if (m.skill[s] > 0.05f) skills += fmt("%s %.0f%%  ", skillName(static_cast<Skill>(s)), m.skill[s] * 100);
    if (!skills.empty()) w.line(skills, kMuted);
    w.line(fmt("Trí nhớ: %d/%d sự kiện · Đồ: %d ăn, %d gỗ, %d đá, %d khác", static_cast<int>(m.ltm.facts().size()), m.ltm.capacity,
               inv.food, inv.wood, inv.stone, inv.matTotal()), kMuted);
    if (m.lastDiscovery >= 0 && sim.tick - m.lastDiscoveryTick < kTicksPerYear * 3)
        w.line(std::string("Vừa học được: ") + techDef(m.lastDiscovery).name, Color{255, 220, 90, 255});
    // Nút theo dõi
    float bh = 34 * app.uiScale;
    if (button({r.x + 10 * app.uiScale, r.y + r.height - bh - 8 * app.uiScale, (r.width - 30 * app.uiScale) / 2, bh}, "Theo dõi")) {
        const Transform& tr = sim.reg.get<Transform>(e);
        app.cam.target = {tr.pos.x * kTileWorld, tr.pos.y * kTileWorld};
        app.cam.zoom = std::max(app.cam.zoom, 4.0f);
    }
    if (grp && button({r.x + r.width / 2 + 5 * app.uiScale, r.y + r.height - bh - 8 * app.uiScale, (r.width - 30 * app.uiScale) / 2, bh}, "Xem nhóm")) {
        app.opt.selectedGroup = so.group;
        app.panel = SidePanel::Group;
    }
    // Camera bám theo sinh vật được chọn khi ở tốc độ cao
    (void)inv;
}

void drawGroupPanel(App& app, Rectangle r) {
    Simulation& sim = *app.sim;
    Group* g = sim.group(app.opt.selectedGroup);
    if (!g) { app.panel = SidePanel::Chronicle; return; }
    Writer w{r.x + 10 * app.uiScale, r.y + 8 * app.uiScale, r.width - 20 * app.uiScale, 13 * app.uiScale};
    w.title(g->displayName(), groupColor(sim, g->id));
    w.line(std::string(stageName(g->stage, g->animal)) + " · " + sim.species[g->species].name + " · " + governmentName(g->gov), kMuted);
    w.line(fmt("Dân số %d (đỉnh %d) · Trí tuệ TB %.2f · Thế hệ %d", g->population(), g->peakPopulation, g->avgIntellect, g->generationDepth));
    w.line(fmt("Lãnh thổ %d ô · %d công trình · %d chỗ ở", g->territoryTiles, g->buildingsTotal, g->housing));
    w.line(fmt("Kho: %d lương thực, %d gỗ, %d đá, %d kim loại", g->food, g->wood, g->stone, g->metalStock(sim.world.materials)));
    w.line(fmt("%s · Trình độ công nghệ %.1f", g->era.c_str(), g->techLevel), Color{255, 210, 120, 255});
    for (int p = 0; p < kPurposeCount && w.y < r.y + r.height * 0.55f; ++p) {
        int d = g->bestDesign[p];
        if (d < 0 || sim.designs[d].primary != static_cast<Purpose>(p)) continue;
        const Design& ds = sim.designs[d];
        w.line(fmt("  %s: %s %.1f", purposeName(static_cast<Purpose>(p)), ds.name.c_str(), ds.value(static_cast<Purpose>(p))), kText);
    }
    if (g->forgeX >= 0) w.line(fmt("  Lò luyện: %.0f°C · %zu quy trình hoá học", g->forgeHeat, g->processes.size()), kMuted);
    if (sim.reg.valid(g->leader)) w.line("Thủ lĩnh: " + sim.reg.get<Social>(g->leader).name, GOLD);
    w.line("Ngôn ngữ: " + g->language.familyName + (g->deity.empty() ? "" : " · Tín ngưỡng: " + g->deity), kMuted);
    w.gap();
    w.line("Văn hoá:", kAccent);
    w.meter("Thượng võ", g->culture.militarism, Color{230, 90, 80, 255});
    w.meter("Tín ngưỡng", g->culture.piety, Color{240, 210, 90, 255});
    w.meter("Ham hiểu biết", g->culture.curiosity, Color{120, 200, 255, 255});
    w.meter("Cộng đồng", g->culture.collectivism, Color{140, 220, 140, 255});
    w.meter("Bài ngoại", g->culture.xenophobia, Color{180, 120, 220, 255});
    w.gap();
    std::string laws;
    for (int l = 0; l < kLawCount; ++l) if (g->laws.test(l)) { if (!laws.empty()) laws += ", "; laws += lawName(static_cast<Law>(l)); }
    w.line("Luật lệ:", kAccent);
    w.wrapped(laws.empty() ? "(chưa có)" : laws);
    std::string techs;
    for (int t = 0; t < kTechCount; ++t) if (g->knowledge.test(t)) { if (!techs.empty()) techs += ", "; techs += techDef(t).name; }
    w.line("Tập quán văn hoá:", kAccent);
    w.wrapped(techs.empty() ? "(chưa có)" : techs, kText, r.y + r.height - w.fs * 8);
    w.line("Ngoại giao:", kAccent);
    int shown = 0;
    for (auto& [id, rel] : g->relations) {
        const Group* o = sim.group(id);
        if (!o || shown > 5) continue;
        if (std::fabs(rel.opinion) < 5 && !rel.atWar) continue;
        w.line(fmt("%s: %s %+.0f", o->displayName().c_str(), rel.atWar ? "CHIẾN TRANH" : "thiện cảm", rel.opinion), rel.atWar ? RED : kText);
        shown++;
    }
    float bh = 34 * app.uiScale;
    if (button({r.x + 10 * app.uiScale, r.y + r.height - bh - 8 * app.uiScale, r.width - 20 * app.uiScale, bh}, "Tới thủ phủ")) {
        app.cam.target = {(g->homeX + 0.5f) * kTileWorld, (g->homeY + 0.5f) * kTileWorld};
        app.cam.zoom = std::max(app.cam.zoom, 3.0f);
    }
}

void drawChroniclePanel(App& app, Rectangle r) {
    Simulation& sim = *app.sim;
    float fs = 12 * app.uiScale;
    ui::drawText("Biên niên sử", r.x + 10 * app.uiScale, r.y + 8 * app.uiScale, fs * 1.25f, kAccent);
    float y = r.y + 8 * app.uiScale + fs * 1.8f;
    const auto& E = sim.chronicle.entries();
    int lines = static_cast<int>((r.height - (y - r.y) - 10) / (fs * 1.3f));
    int start = std::max(0, static_cast<int>(E.size()) - lines - app.chronicleScroll);
    for (int i = static_cast<int>(E.size()) - 1 - app.chronicleScroll; i >= start && i >= 0; --i) {
        const ChronicleEntry& e = E[i];
        Color c = kText;
        if (e.kind == EventKind::Discovery) c = Color{255, 220, 110, 255};
        if (e.kind == EventKind::War) c = Color{255, 110, 100, 255};
        if (e.kind == EventKind::Evolution) c = Color{150, 255, 170, 255};
        if (e.kind == EventKind::Disaster) c = Color{255, 160, 60, 255};
        std::string t = fmt("[%ld] ", e.tick / kTicksPerYear) + e.text;
        // cắt cho vừa khung
        while (t.size() > 4 && ui::measureText(t, fs) > r.width - 20 * app.uiScale) {
            t.pop_back();
            while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0x80) t.pop_back(); // không cắt giữa ký tự UTF-8
        }
        Rectangle lr{r.x + 6, y - 2, r.width - 12, fs * 1.3f};
        if (e.x >= 0 && pointInRect(GetMousePosition(), lr) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && pointInRect(app.pressPos, lr)) {
            app.cam.target = {(e.x + 0.5f) * kTileWorld, (e.y + 0.5f) * kTileWorld};
            app.cam.zoom = std::max(app.cam.zoom, 3.0f);
        }
        ui::drawText(t, r.x + 10 * app.uiScale, y, fs, c);
        y += fs * 1.3f;
    }
    if (pointInRect(GetMousePosition(), r)) {
        app.chronicleScroll = std::clamp(app.chronicleScroll + static_cast<int>(GetMouseWheelMove() * 3), 0, std::max(0, static_cast<int>(E.size()) - 3));
    }
}

void drawSpeciesPanel(App& app, Rectangle r) {
    Simulation& sim = *app.sim;
    float fs = 13 * app.uiScale;
    ui::drawText("Chọn loài để thả", r.x + 10 * app.uiScale, r.y + 8 * app.uiScale, fs * 1.2f, kAccent);
    float bh = 36 * app.uiScale, y = r.y + 12 * app.uiScale + fs * 1.6f;
    float bw = (r.width - 30 * app.uiScale) / 2;
    for (int i = 0; i < sim.species.count(); ++i) {
        float x = r.x + 10 * app.uiScale + (i % 2) * (bw + 10 * app.uiScale);
        if (y > r.y + r.height - bh) break;
        const SpeciesDef& s = sim.species[i];
        std::string label = s.name + (s.population > 0 ? fmt(" (%d)", s.population) : "");
        if (button({x, y, bw, bh}, label, app.spawnSpecies == i)) { app.spawnSpecies = i; app.tool = Power::Spawn; }
        DrawRectangle(static_cast<int>(x + 4), static_cast<int>(y + 4), static_cast<int>(6 * app.uiScale), static_cast<int>(6 * app.uiScale), speciesColor(sim, i));
        if (i % 2 == 1) y += bh + 6 * app.uiScale;
    }
}

// Bảng "Hoá học & vật lý": phản ứng đang diễn ra, thống kê, khí quyển, vật liệu mới, phát minh
void drawChemistryPanel(App& app, Rectangle r) {
    Simulation& sim = *app.sim;
    const MatterGrid& G = sim.world.matter;
    const MaterialRegistry& R = sim.world.materials;
    Writer w{r.x + 10 * app.uiScale, r.y + 8 * app.uiScale, r.width - 20 * app.uiScale, 12 * app.uiScale};
    w.title("Vật lý & Hoá học");
    w.line(fmt("Khí quyển: CO₂ %.0f ppm · độ ẩm %.0f%% · gió (%.2f, %.2f)", G.atmosCO2, G.humidity * 100, G.wind.x, G.wind.y));
    w.line(fmt("Ô hoạt động %d · đang cháy %d · vật thể bay/lăn %zu", G.activeCount(), G.burningCount(), sim.physics.bodies.size()), kMuted);
    // Vật liệu sinh ra trong lúc chạy (hợp kim mới)
    std::string born;
    for (int i = mat::DefaultCount; i < R.count(); ++i) if (!R[static_cast<MatId>(i)].natural) { if (!born.empty()) born += ", "; born += R[static_cast<MatId>(i)].name; }
    w.wrapped(fmt("Vật liệu: %d", R.count()) + (born.empty() ? std::string("") : " — mới: " + born), kText, r.y + r.height * 0.3f);
    w.line(fmt("Phát minh: %zu thiết kế, %zu quy trình trên thế giới", sim.designs.all.size(), sim.processes.all.size()), Color{255, 210, 120, 255});
    for (int p = 0; p < kPurposeCount && w.y < r.y + r.height * 0.5f; ++p) {
        int d = sim.designs.worldBest[p];
        if (d < 0) continue;
        const Design& ds = sim.designs[d];
        w.line(fmt("  Kỷ lục %s: %s %.1f", purposeName(static_cast<Purpose>(p)), ds.name.c_str(), ds.value(static_cast<Purpose>(p))), kMuted);
    }
    w.gap();
    w.line("Phản ứng gần đây:", kAccent);
    const auto& rec = G.recent();
    for (auto it = rec.rbegin(); it != rec.rend() && w.y < r.y + r.height - w.fs * 2; ++it) {
        std::string t = fmt("[%ld] ", it->tick / kTicksPerYear) + reactionName(it->kind) + ": " + it->describe(R);
        while (t.size() > 4 && ui::measureText(t, w.fs * 0.9f) > w.w) {
            t.pop_back();
            while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0x80) t.pop_back();
        }
        Rectangle lr{r.x + 4, w.y - 1, r.width - 8, w.fs * 1.2f};
        if (pointInRect(GetMousePosition(), lr) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && pointInRect(app.pressPos, lr)) {
            app.cam.target = {(it->x + 0.5f) * kTileWorld, (it->y + 0.5f) * kTileWorld};
            app.cam.zoom = std::max(app.cam.zoom, 4.0f);
        }
        ui::drawText(t, w.x, w.y, w.fs * 0.9f, Color{255, 220, 150, 255});
        w.y += w.fs * 1.2f;
    }
}

// ---------------------------------------------------------------------------
// Thanh trên + thanh công cụ
// ---------------------------------------------------------------------------
void drawHud(App& app) {
    Simulation& sim = *app.sim;
    float s = app.uiScale;
    int W = GetScreenWidth(), H = GetScreenHeight();
    bool portrait = H > W;
    // ---- Thanh trên ----
    float th = 40 * s;
    panel({0, 0, static_cast<float>(W), th}, Color{10, 12, 18, 210});
    const SimStats& st = sim.stats();
    std::string info = sim.dateString() + fmt(" · Dân %d · Nhóm %d · Quốc gia %d · Loài %d", st.population, st.groups, st.nations, st.species);
    float fs = 14 * s;
    float speedW = 44 * s;
    float rightBlock = speedW * 5 + 4 * 90 * s;
    if (portrait) info = sim.dateString() + fmt(" · Dân %d · QG %d", st.population, st.nations);
    ui::drawText(info, 10 * s, (th - fs) / 2, fs, kText);
    float x = W - rightBlock;
    if (portrait) x = W - speedW * 5 - 8 * s;
    for (int i = 0; i < 5; ++i) {
        if (button({x + i * speedW, 4 * s, speedW - 4 * s, th - 8 * s}, kSpeedLabels[i], app.speedIdx == i)) app.speedIdx = i;
    }
    if (!portrait) {
        float bx = x + 5 * speedW + 6 * s;
        if (button({bx, 4 * s, 86 * s, th - 8 * s}, "Biên niên", app.panel == SidePanel::Chronicle))
            app.panel = app.panel == SidePanel::Chronicle ? SidePanel::None : SidePanel::Chronicle;
        if (button({bx + 90 * s, 4 * s, 86 * s, th - 8 * s}, "Suy nghĩ", app.opt.debugThoughts)) app.opt.debugThoughts = !app.opt.debugThoughts;
        if (button({bx + 180 * s, 4 * s, 86 * s, th - 8 * s}, std::string("Lớp: ") + layerName(app.opt.layer), app.opt.layer != Layer::Normal))
            app.opt.layer = static_cast<Layer>((static_cast<int>(app.opt.layer) + 1) % static_cast<int>(Layer::Count));
        if (button({bx + 270 * s, 4 * s, 80 * s, th - 8 * s}, "Menu")) app.inMenu = true;
    }

    // ---- Thanh công cụ dưới ----
    float bh = 46 * s;
    int ncat = static_cast<int>(ToolCategory::Count);
    float rowY2 = H - bh - 6 * s;
    float rowY1 = rowY2 - bh - 6 * s;
    float catW = std::min(130 * s, (W - 12 * s) / ncat - 4 * s);
    panel({0, rowY1 - 6 * s, static_cast<float>(W), H - rowY1 + 6 * s}, Color{10, 12, 18, 200});
    for (int c = 0; c < ncat; ++c) {
        if (button({6 * s + c * (catW + 4 * s), rowY1, catW, bh}, kCategoryNames[c], app.category == static_cast<ToolCategory>(c))) {
            app.category = static_cast<ToolCategory>(c);
            if (app.category == ToolCategory::Inspect) app.tool = Power::Inspect;
            if (app.category == ToolCategory::Life) { app.tool = Power::Spawn; app.panel = SidePanel::Species; }
        }
    }
    // Hàng 2: công cụ trong nhóm + cỡ cọ
    float tx = 6 * s;
    float toolW = std::min(110 * s, std::max(70 * s, (W - 200 * s) / 9));
    for (const ToolEntry& t : kTools) {
        if (t.cat != app.category) continue;
        std::string label = powerName(t.power);
        if (t.power == Power::Spawn) label = "Thả: " + sim.species[app.spawnSpecies].name;
        if (button({tx, rowY2, toolW, bh}, label, app.tool == t.power)) {
            app.tool = t.power;
            if (t.power == Power::Spawn) app.panel = SidePanel::Species;
        }
        tx += toolW + 4 * s;
    }
    if (app.category == ToolCategory::Inspect) {
        if (portrait) {
            if (button({tx, rowY2, toolW, bh}, "Biên niên", app.panel == SidePanel::Chronicle))
                app.panel = app.panel == SidePanel::Chronicle ? SidePanel::None : SidePanel::Chronicle;
            tx += toolW + 4 * s;
            if (button({tx, rowY2, toolW, bh}, "Suy nghĩ", app.opt.debugThoughts)) app.opt.debugThoughts = !app.opt.debugThoughts;
            tx += toolW + 4 * s;
            if (button({tx, rowY2, toolW, bh}, "Menu")) app.inMenu = true;
            tx += toolW + 4 * s;
            if (button({tx, rowY2, toolW, bh}, "Hoá học", app.panel == SidePanel::Chemistry))
                app.panel = app.panel == SidePanel::Chemistry ? SidePanel::None : SidePanel::Chemistry;
            tx += toolW + 4 * s;
            if (button({tx, rowY2, toolW, bh}, std::string("Lớp: ") + layerName(app.opt.layer), app.opt.layer != Layer::Normal))
                app.opt.layer = static_cast<Layer>((static_cast<int>(app.opt.layer) + 1) % static_cast<int>(Layer::Count));
            tx += toolW + 4 * s;
        } else {
            if (button({tx, rowY2, toolW, bh}, "Hoá học", app.panel == SidePanel::Chemistry))
                app.panel = app.panel == SidePanel::Chemistry ? SidePanel::None : SidePanel::Chemistry;
            tx += toolW + 4 * s;
            ui::drawText("Chạm sinh vật: xem suy nghĩ · chạm lãnh thổ: nền văn minh · kéo: di chuyển · chụm 2 ngón: phóng to",
                         tx + 6 * s, rowY2 + (bh - 13 * s) / 2, 13 * s, kMuted);
        }
    } else {
        // Cỡ cọ
        float bx = W - 3 * (bh + 4 * s) - 6 * s;
        if (button({bx, rowY2, bh, bh}, "-")) app.brush = std::max(app.tool == Power::Spawn ? 1 : 0, app.brush - 1);
        Rectangle mid{bx + bh + 4 * s, rowY2, bh, bh};
        panel(mid, kPanelLight);
        std::string bs = std::to_string(app.brush);
        ui::drawText(bs, mid.x + (bh - ui::measureText(bs, 18 * s)) / 2, mid.y + (bh - 18 * s) / 2, 18 * s, kText);
        if (button({bx + 2 * (bh + 4 * s), rowY2, bh, bh}, "+")) app.brush = std::min(12, app.brush + 1);
    }

    // ---- Bảng bên ----
    if (app.panel != SidePanel::None) {
        float pw = portrait ? W - 12 * s : std::min(380 * s, W * 0.42f);
        float ph = portrait ? (rowY1 - th - 18 * s) * 0.55f : rowY1 - th - 18 * s;
        Rectangle r = portrait ? Rectangle{6 * s, rowY1 - 12 * s - ph, pw, ph} : Rectangle{W - pw - 6 * s, th + 6 * s, pw, ph};
        panel(r);
        if (button({r.x + r.width - 34 * s, r.y + 4 * s, 30 * s, 30 * s}, "x")) { app.panel = SidePanel::None; app.opt.selected = NullEntity; }
        switch (app.panel) {
        case SidePanel::Creature: drawCreaturePanel(app, r); break;
        case SidePanel::Group: drawGroupPanel(app, r); break;
        case SidePanel::Chronicle: drawChroniclePanel(app, r); break;
        case SidePanel::Species: drawSpeciesPanel(app, r); break;
        case SidePanel::Chemistry: drawChemistryPanel(app, r); break;
        default: break;
        }
    }
}

void drawMenu(App& app) {
    float s = app.uiScale;
    int W = GetScreenWidth(), H = GetScreenHeight();
    ClearBackground(Color{14, 18, 30, 255});
    // nền pixel trang trí
    for (int i = 0; i < 300; ++i) {
        uint32_t h = hash32(i * 977);
        DrawRectangle(static_cast<int>(h % W), static_cast<int>((h >> 12) % H), 3, 3, Color{60, 90, 140, static_cast<unsigned char>(40 + (h >> 24) % 80)});
    }
    std::string title = "WorldBox AI";
    float tfs = 54 * s;
    ui::drawText(title, (W - ui::measureText(title, tfs)) / 2, H * 0.1f, tfs, Color{255, 220, 120, 255});
    std::string sub = "Sandbox thần thánh — mỗi sinh vật có một bộ óc riêng, tự tiến hoá thành nền văn minh";
    float sfs = 15 * s;
    if (ui::measureText(sub, sfs) > W - 20) sfs *= (W - 20) / ui::measureText(sub, sfs);
    ui::drawText(sub, (W - ui::measureText(sub, sfs)) / 2, H * 0.1f + tfs * 1.2f, sfs, kMuted);

    float bw = std::min(420 * s, W - 40.0f), bh = 54 * s;
    float x = (W - bw) / 2, y = H * 0.28f;
    const char* desc[] = {"4 chủng tộc, động vật, quái vật", "Bầy sói học săn theo bầy → bộ tộc sói", "Người nguyên thuỷ → công cụ → làng → quốc gia",
                          "Hai nhóm nhân bản, hai môi trường → hai con đường"};
    for (int i = 0; i < static_cast<int>(Scenario::Count); ++i) {
        if (button({x, y, bw, bh}, std::string(scenarioName(static_cast<Scenario>(i))) + " — " + desc[i], app.scenario == static_cast<Scenario>(i)))
            app.scenario = static_cast<Scenario>(i);
        y += bh + 10 * s;
    }
    y += 6 * s;
    const int sizes[] = {128, 192, 256};
    const char* sizeNames[] = {"Nhỏ", "Vừa", "Lớn"};
    float sw = (bw - 20 * s) / 3;
    for (int i = 0; i < 3; ++i)
        if (button({x + i * (sw + 10 * s), y, sw, bh * 0.8f}, std::string("Bản đồ ") + sizeNames[i], app.worldSize == sizes[i])) app.worldSize = sizes[i];
    y += bh * 0.8f + 20 * s;
    if (button({x, y, bw, bh * 1.2f}, "BẮT ĐẦU", false, Color{60, 140, 80, 255})) startWorld(app);
    y += bh * 1.2f + 10 * s;
    if (app.sim && button({x, y, bw, bh * 0.9f}, "Tiếp tục thế giới hiện tại")) app.inMenu = false;
    if (!ui::hasUnicodeFont())
        ui::drawText("(Không tìm thấy font Unicode — chữ hiển thị không dấu)", 10, H - 24 * s, 12 * s, kMuted);
}
} // namespace

// ===========================================================================
void appInit(App& app) {
    gApp = &app;
    float minDim = static_cast<float>(std::min(GetScreenWidth(), GetScreenHeight()));
    app.uiScale = std::clamp(minDim / 620.0f, 0.9f, 3.0f);
#if defined(PLATFORM_ANDROID)
    app.uiScale = std::clamp(minDim / 500.0f, 1.2f, 3.5f); // ngón tay to hơn con trỏ chuột
    app.worldSize = 128;
#endif
    ui::initText(16 * app.uiScale);
}

void appStartWorld(App& app) { startWorld(app); }

void appShutdown(App& app) {
    app.renderer.shutdown();
    ui::shutdownText();
}

void appFrame(App& app) {
    gApp = &app;
    app.uiRectsPrev.swap(app.uiRects);
    app.uiRects.clear();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) app.pressPos = GetMousePosition();
    app.frame++;

    if (app.inMenu || !app.sim) {
        BeginDrawing();
        drawMenu(app);
        EndDrawing();
        return;
    }
    Simulation& sim = *app.sim;
    // ---- Phím tắt ----
    if (IsKeyPressed(KEY_SPACE)) app.speedIdx = app.speedIdx == 0 ? 1 : 0;
    for (int i = 0; i < 5; ++i) if (IsKeyPressed(KEY_ONE + i)) app.speedIdx = i;
    if (IsKeyPressed(KEY_F1)) app.opt.debugThoughts = !app.opt.debugThoughts;
    if (IsKeyPressed(KEY_TAB)) app.panel = app.panel == SidePanel::Chronicle ? SidePanel::None : SidePanel::Chronicle;
    if (IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE)) app.inMenu = true; // nút Back Android

    handleWorldInput(app);

    // ---- Mô phỏng: bước thời gian cố định, giới hạn ngân sách thời gian mỗi khung ----
    app.tickAccum += GetFrameTime() * kTicksPerSecond * kSpeeds[app.speedIdx];
    app.tickAccum = std::min(app.tickAccum, 200.0f);
    double start = GetTime();
    while (app.tickAccum >= 1.0f) {
        sim.step();
        app.tickAccum -= 1.0f;
        if (GetTime() - start > 0.020) { app.tickAccum = std::min(app.tickAccum, 2.0f); break; } // giữ FPS
    }
    // LOD: báo cho mô phỏng vùng đang nhìn
    Vector2 tl = GetScreenToWorld2D({0, 0}, app.cam);
    Vector2 br = GetScreenToWorld2D({static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, app.cam);
    sim.setFocusRect(tl.x / kTileWorld, tl.y / kTileWorld, br.x / kTileWorld, br.y / kTileWorld);
    // Texture địa hình: cập nhật thưa (mỗi 4 khung) để tiết kiệm băng thông GPU
    if (app.frame % 4 == 0 || sim.world.fullRedraw) app.renderer.sync(sim);

    BeginDrawing();
    ClearBackground(Color{20, 40, 80, 255});
    BeginMode2D(app.cam);
    app.renderer.drawWorld(sim, app.cam, app.opt, static_cast<float>(GetTime()));
    // Con trỏ cọ
    if (app.tool != Power::Inspect) {
        Vector2 w = GetScreenToWorld2D(GetMousePosition(), app.cam);
        float r = (app.brush + 0.5f) * kTileWorld;
        DrawCircleLinesV({std::floor(w.x / kTileWorld) * kTileWorld + kTileWorld / 2, std::floor(w.y / kTileWorld) * kTileWorld + kTileWorld / 2}, r,
                         Color{255, 255, 255, 140});
    }
    EndMode2D();
    app.renderer.drawOverlay(sim, app.cam, app.opt, app.uiScale);
    drawHud(app);
    EndDrawing();
}

} // namespace wb
