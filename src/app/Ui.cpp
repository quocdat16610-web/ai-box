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
#include <iterator>

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
    // Bản đồ trụ đẳng chữ nhật của một hành tinh: chiều rộng (kinh độ) = 2 × chiều cao (vĩ độ)
    cfg.width = app.worldSize * 2;
    cfg.height = app.worldSize;
    cfg.worldType = app.worldType;
    cfg.seed = static_cast<uint32_t>(GetRandomValue(1, 1000000));
#if defined(PLATFORM_ANDROID)
    cfg.maxPopulation = 3000; // điện thoại: giới hạn dân số thấp hơn để giữ 60 FPS
#endif
    if (app.sim) fpExit(app.fp, *app.sim);
    fpRelease(app.fp);
    app.sim = std::make_unique<Simulation>(cfg);
    setupScenario(*app.sim, app.scenario);
    app.renderer.init(*app.sim);
    app.cam.target = {cfg.width * kTileWorld / 2, cfg.height * kTileWorld / 2};
    app.cam.offset = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    app.cam.zoom = GetScreenHeight() / (cfg.height * kTileWorld) * 1.6f;
    app.globe = false;
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

// Mức thu nhỏ tối thiểu của bản đồ phẳng: nhìn thấy trọn từ cực bắc tới cực nam.
// Thu nhỏ hơn nữa → chuyển sang xem quả địa cầu.
float minZoom2D(const App& app) { return GetScreenHeight() / (app.sim->world.height() * kTileWorld) * 0.98f; }

void enterGlobe(App& app) {
    const World& w = app.sim->world;
    float tx = WrapSpace::wrapX(app.cam.target.x / kTileWorld), ty = app.cam.target.y / kTileWorld;
    app.globeView.lon = tx / w.width() * 6.2831853f;
    app.globeView.lat = std::clamp((0.5f - ty / w.height()) * 3.14159265f, -1.3f, 1.3f);
    app.globeView.dist = 4.2f;
    app.globe = true;
}

// Rời quả địa cầu, phóng xuống bản đồ phẳng tại (kinh độ, vĩ độ)
void exitGlobe(App& app, float lon, float lat, float zoomMul) {
    const World& w = app.sim->world;
    float x = std::fmod(lon / 6.2831853f + 10.0f, 1.0f) * w.width();
    float y = (0.5f - lat / 3.14159265f) * w.height();
    app.cam.target = {x * kTileWorld, y * kTileWorld};
    app.cam.zoom = minZoom2D(app) * zoomMul;
    app.globe = false;
}

void clampCamera(App& app) {
    float W = app.sim->world.width() * kTileWorld, H = app.sim->world.height() * kTileWorld;
    app.cam.zoom = std::clamp(app.cam.zoom, minZoom2D(app), 12.0f);
    app.cam.target.x = std::fmod(std::fmod(app.cam.target.x, W) + W, W);   // quấn vòng đông–tây
    // không cho cuộn quá hai cực (giữ khung nhìn trong bản đồ theo chiều dọc)
    float halfH = GetScreenHeight() * 0.5f / app.cam.zoom;
    app.cam.target.y = halfH * 2 >= H ? H * 0.5f : std::clamp(app.cam.target.y, halfH, H - halfH);
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
    Vec2 wp = wrapPos({worldPx.x / kTileWorld, worldPx.y / kTileWorld});
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

// Nhập liệu khi xem quả địa cầu: kéo = xoay hành tinh, lăn/chụm = phóng to/thu nhỏ,
// chạm = bay xuống bản đồ tại điểm đó.
void handleGlobeInput(App& app) {
    GlobeView& gv = app.globeView;
    Vector2 mp = GetMousePosition();
    int touches = GetTouchPointCount();
    float rot = 1.6f / (GetScreenHeight() * 0.5f) * (gv.dist - 1.0f) / 1.6f;   // kéo hết nửa màn hình ≈ xoay 1 radian khi ở gần
    if (touches >= 2) {
        Vector2 a = GetTouchPosition(0), b = GetTouchPosition(1);
        float d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
        if (!app.pinching) { app.pinching = true; app.pinchStartDist = std::max(10.0f, d); app.pinchStartZoom = gv.dist; }
        else gv.dist = std::clamp(app.pinchStartZoom * app.pinchStartDist / d, 1.25f, 7.0f);
        app.pointerDownOnWorld = false;
    } else if (app.pinching && touches == 0) app.pinching = false;
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && !overUi(app, mp)) gv.dist = std::clamp(gv.dist * std::pow(0.88f, wheel), 1.25f, 7.0f);
    if (!app.pinching) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { app.pressPos = mp; app.lastPointer = mp; app.dragMoved = false; app.pointerDownOnWorld = !overUi(app, mp); }
        if (app.pointerDownOnWorld && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d{mp.x - app.lastPointer.x, mp.y - app.lastPointer.y};
            if (std::fabs(mp.x - app.pressPos.x) + std::fabs(mp.y - app.pressPos.y) > 12 * app.uiScale) app.dragMoved = true;
            if (app.dragMoved) { gv.lon -= d.x * rot; gv.lat = std::clamp(gv.lat + d.y * rot, -1.45f, 1.45f); }
            app.lastPointer = mp;
        }
        if (app.pointerDownOnWorld && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            app.pointerDownOnWorld = false;
            if (!app.dragMoved) {
                // Chạm vào mặt cầu → bay xuống đúng điểm đó
                RayCollision hit = GetRayCollisionSphere(GetMouseRay(mp, globeCamera(gv)), {0, 0, 0}, 1.0f);
                if (hit.hit) { exitGlobe(app, std::atan2(-hit.point.z, hit.point.x), std::asin(std::clamp(hit.point.y, -1.0f, 1.0f)), 2.5f); return; }
            }
        }
    }
    float dt = GetFrameTime();
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) gv.lon -= dt * 1.2f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) gv.lon += dt * 1.2f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) gv.lat = std::min(1.45f, gv.lat + dt);
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) gv.lat = std::max(-1.45f, gv.lat - dt);
    // Phóng đủ gần → trở lại bản đồ phẳng tại điểm đối diện camera
    if (gv.dist <= 1.3f) exitGlobe(app, gv.lon, gv.lat, 1.05f);
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
            if (app.cam.zoom < minZoom2D(app) * 0.8f) { enterGlobe(app); app.pinching = false; app.pointerDownOnWorld = false; return; }
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
        if (wheel < 0 && app.cam.zoom <= minZoom2D(app) * 1.001f) { enterGlobe(app); return; }   // thu nhỏ hết → quả địa cầu
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
    if (!sd.customGenes.empty()) {
        std::string cg;
        for (const CustomGene& c : sd.customGenes) { if (!cg.empty()) cg += ", "; cg += c.name + " (" + geneEffectName(c.effect) + ")"; }
        w.wrapped(std::string(sd.engineered ? "Loài nhân tạo · gen: " : "Gen đột biến: ") + cg, Color{255, 200, 255, 255});
    }
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
    // Nhìn qua mắt sinh vật / nhập vai điều khiển nó
    float bh = 34 * app.uiScale;
    {
        float by = r.y + r.height - 2 * bh - 14 * app.uiScale, bw = (r.width - 30 * app.uiScale) / 2;
        if (button({r.x + 10 * app.uiScale, by, bw, bh}, "Nhìn qua mắt")) fpEnter(app.fp, sim, e);
        if (button({r.x + r.width / 2 + 5 * app.uiScale, by, bw, bh}, "Điều khiển")) {
            fpEnter(app.fp, sim, e);
            app.fp.control = true;
            sim.reg.get<Mind>(e).possessed = true;
        }
    }
    // Nút theo dõi
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
    // Phân huỷ xác chết xảy ra liên tục → chỉ đếm, không liệt kê (tránh che các phản ứng thú vị)
    int decays = 0;
    for (const auto& ev : rec) decays += ev.kind == ReactionKind::Decay;
    if (decays > 0) w.line(fmt("  (+%d lần phân huỷ xác/chất hữu cơ)", decays), Color{170, 170, 170, 255});
    auto sameReaction = [](const ReactionEvent& a, const ReactionEvent& b) {
        return a.kind == b.kind && std::equal(a.in, a.in + 3, b.in) && std::equal(a.out, a.out + 2, b.out);
    };
    for (auto it = rec.rbegin(); it != rec.rend() && w.y < r.y + r.height - w.fs * 2; ++it) {
        if (it->kind == ReactionKind::Decay) continue;
        // Gộp các phản ứng giống hệt nhau liền kề thành một dòng "×N"
        int same = 1;
        for (auto nx = std::next(it); nx != rec.rend(); ++nx) {
            if (nx->kind == ReactionKind::Decay) continue;
            if (!sameReaction(*it, *nx)) break;
            it = nx;
            ++same;
        }
        std::string t = fmt("[%ld] ", it->tick / kTicksPerYear) + reactionName(it->kind) + ": " + it->describe(R);
        if (same > 1) t = fmt("×%d ", same) + t;
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
    if (app.category == ToolCategory::Life) {
        if (button({tx, rowY2, toolW * 1.6f, bh}, "Phòng thí nghiệm gen", app.lab.open, Color{120, 70, 140, 255})) { app.lab.open = true; app.lab.hasDraft = false; }
        tx += toolW * 1.6f + 4 * s;
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
            if (button({tx, rowY2, toolW, bh}, "Lòng đất", app.opt.layer == Layer::Underground))
                app.opt.layer = app.opt.layer == Layer::Underground ? Layer::Normal : Layer::Underground;
            tx += toolW + 4 * s;
        } else {
            if (button({tx, rowY2, toolW, bh}, "Hoá học", app.panel == SidePanel::Chemistry))
                app.panel = app.panel == SidePanel::Chemistry ? SidePanel::None : SidePanel::Chemistry;
            tx += toolW + 4 * s;
            if (button({tx, rowY2, toolW, bh}, "Lòng đất", app.opt.layer == Layer::Underground))
                app.opt.layer = app.opt.layer == Layer::Underground ? Layer::Normal : Layer::Underground;
            tx += toolW + 4 * s;
            ui::drawText(app.globe ? "Kéo: xoay hành tinh · chạm: hạ xuống điểm đó · chụm/lăn chuột: phóng to (đủ gần sẽ về bản đồ)"
                                   : "Chạm sinh vật: xem suy nghĩ · chạm lãnh thổ: nền văn minh · kéo: di chuyển · thu nhỏ hết: quả địa cầu",
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

// ---------------------------------------------------------------------------
// Góc nhìn thứ nhất: nhập liệu + HUD
// ---------------------------------------------------------------------------
void fpAction(App& app, Brain::PlayerAct act) {
    FirstPerson& fp = app.fp;
    fp.msg = Brain::playerAction(*app.sim, fp.who, act, fpFacing(fp));
    fp.msgTime = static_cast<float>(GetTime());
}

void handleFpInput(App& app) {
    Simulation& sim = *app.sim;
    FirstPerson& fp = app.fp;
    if (!sim.reg.valid(fp.who) || sim.reg.get<Body>(fp.who).dead) {   // sinh vật đã chết → về bản đồ
        fpExit(fp, sim);
        app.panel = SidePanel::Chronicle;
        return;
    }
    float dt = GetFrameTime();
    int W = GetScreenWidth(), H = GetScreenHeight();
    auto inStickZone = [&](Vector2 p) { return p.x < W * 0.38f && p.y > H * 0.45f; };
    Vector2 move{0, 0};
    // ---- Cảm ứng / chuột: cần điều khiển ảo (trái dưới) + vuốt để nhìn ----
    int touches = std::max(GetTouchPointCount(), IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? 1 : 0);
    bool stickSeen = false, lookSeen = false;
    for (int i = 0; i < touches; ++i) {
        Vector2 p = GetTouchPointCount() > 0 ? GetTouchPosition(i) : GetMousePosition();
        if (overUi(app, p) && !fp.stick && !fp.looking) continue;
        if (fp.control && (fp.stick ? (std::fabs(p.x - fp.stickPos.x) + std::fabs(p.y - fp.stickPos.y) < 200 * app.uiScale) : inStickZone(p)) && !stickSeen) {
            if (!fp.stick) { fp.stick = true; fp.stickOrigin = p; }
            fp.stickPos = p;
            stickSeen = true;
        } else if (!lookSeen) {
            if (!fp.looking) { fp.looking = true; fp.lookLast = p; }
            fp.yaw += (p.x - fp.lookLast.x) * 0.006f / app.uiScale;
            fp.pitch = std::clamp(fp.pitch - (p.y - fp.lookLast.y) * 0.005f / app.uiScale, -1.2f, 1.0f);
            fp.lookLast = p;
            lookSeen = true;
        }
    }
    if (!stickSeen) fp.stick = false;
    if (!lookSeen) fp.looking = false;
    if (fp.stick) {
        float rad = 60 * app.uiScale;
        move.x = std::clamp((fp.stickPos.x - fp.stickOrigin.x) / rad, -1.0f, 1.0f);
        move.y = std::clamp((fp.stickPos.y - fp.stickOrigin.y) / rad, -1.0f, 1.0f);
    }
    // ---- Bàn phím (PC) ----
    if (IsKeyDown(KEY_Q)) fp.yaw -= dt * 2.0f;
    if (IsKeyDown(KEY_E) && !fp.control) fp.yaw += dt * 2.0f;
    if (IsKeyDown(KEY_LEFT)) fp.yaw -= dt * 2.0f;
    if (IsKeyDown(KEY_RIGHT)) fp.yaw += dt * 2.0f;
    if (fp.control) {
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) move.y -= 1;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) move.y += 1;
        if (IsKeyDown(KEY_A)) move.x -= 1;
        if (IsKeyDown(KEY_D)) move.x += 1;
        if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_J)) fpAction(app, Brain::PlayerAct::Attack);
        if (IsKeyPressed(KEY_E)) fpAction(app, Brain::PlayerAct::Eat);
        if (IsKeyPressed(KEY_G)) fpAction(app, Brain::PlayerAct::Gather);
        if (IsKeyPressed(KEY_B)) fpAction(app, Brain::PlayerAct::BuildHut);
        float L = std::sqrt(move.x * move.x + move.y * move.y);
        if (L > 0.05f) {
            if (L > 1) { move.x /= L; move.y /= L; }
            Vec2 fwd = fpFacing(fp), right{-fwd.y, fwd.x};
            Body& b = sim.reg.get<Body>(fp.who);
            float run = IsKeyDown(KEY_LEFT_SHIFT) ? 1.6f : 1.0f;
            float step = b.speed * kTicksPerSecond * dt * run;   // tốc độ thật của sinh vật (ô/giây)
            Vec2 dir = fwd * (-move.y * step) + right * (move.x * step);
            Brain::playerMove(sim, fp.who, dir);
            b.fatigue = std::min(1.0f, b.fatigue + dt * 0.004f * run);
        } else sim.reg.get<Transform>(fp.who).moving = false;
    }
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK)) fpExit(fp, sim);
}

void drawFpHud(App& app) {
    Simulation& sim = *app.sim;
    FirstPerson& fp = app.fp;
    if (!sim.reg.valid(fp.who)) return;
    float s = app.uiScale;
    int W = GetScreenWidth(), H = GetScreenHeight();
    const Body& b = sim.reg.get<Body>(fp.who);
    const Mind& m = sim.reg.get<Mind>(fp.who);
    const Social& so = sim.reg.get<Social>(fp.who);
    const Inventory& inv = sim.reg.get<Inventory>(fp.who);
    const SpeciesDef& sd = sim.species[b.species];
    // ---- Bảng trạng thái ----
    float pw = std::min(420 * s, W * 0.55f);
    Rectangle pr{8 * s, 8 * s, pw, 118 * s};
    panel(pr, Color{10, 12, 18, 190});
    Writer w{pr.x + 10 * s, pr.y + 6 * s, pw - 20 * s, 13 * s};
    std::string head = so.name + " — " + sd.name;
    if (so.group >= 0 && sim.group(so.group)) head += " · " + sim.groups[so.group].displayName();
    w.line(head, kAccent);
    w.meter("Máu", b.hp / b.maxHp, Color{220, 70, 70, 255});
    w.meter("Đói", b.hunger, Color{230, 160, 60, 255});
    w.meter("Mệt", b.fatigue, Color{150, 150, 230, 255});
    if (fp.control) w.line(fmt("ĐANG ĐIỀU KHIỂN · mang: %d ăn, %d gỗ, %d đá, %d khác", inv.food, inv.wood, inv.stone, inv.matTotal()), Color{255, 220, 110, 255});
    else w.line(std::string("Đang nghĩ: \"") + m.thought + "\"", Color{255, 230, 150, 255});
    // ---- Bản đồ nhỏ ----
    float ms = 120 * s;
    Rectangle mr{W - ms - 10 * s, 10 * s, ms, ms};
    panel(mr, Color{0, 0, 0, 160});
    const Transform& tr = sim.reg.get<Transform>(fp.who);
    const Texture2D& tex = sd.habitat == Habitat::Cave ? app.renderer.undergroundTexture(sim) : app.renderer.terrainTexture();
    float span = 40;   // ô
    float sx0 = std::clamp(tr.pos.x - span / 2, 0.0f, sim.world.width() - span), sy0 = std::clamp(tr.pos.y - span / 2, 0.0f, sim.world.height() - span);
    DrawTexturePro(tex, {sx0 * kTilePx, sy0 * kTilePx, span * kTilePx, span * kTilePx}, mr, {0, 0}, 0, WHITE);
    Vector2 dot{mr.x + (tr.pos.x - sx0) / span * ms, mr.y + (tr.pos.y - sy0) / span * ms};
    Vec2 f = fpFacing(fp);
    DrawLineEx(dot, {dot.x + f.x * 10 * s, dot.y + f.y * 10 * s}, 2 * s, YELLOW);
    DrawCircleV(dot, 3 * s, RED);
    // ---- Tâm ngắm ----
    if (fp.control) {
        DrawLineEx({W / 2.0f - 8 * s, H / 2.0f}, {W / 2.0f + 8 * s, H / 2.0f}, 2, Color{255, 255, 255, 180});
        DrawLineEx({W / 2.0f, H / 2.0f - 8 * s}, {W / 2.0f, H / 2.0f + 8 * s}, 2, Color{255, 255, 255, 180});
    }
    // ---- Thông báo hành động ----
    if (!fp.msg.empty() && GetTime() - fp.msgTime < 3.0) {
        float fs = 16 * s, tw = ui::measureText(fp.msg, fs);
        DrawRectangleRounded({(W - tw) / 2 - 8 * s, H * 0.62f, tw + 16 * s, fs + 10 * s}, 0.3f, 4, Color{0, 0, 0, 170});
        ui::drawText(fp.msg, (W - tw) / 2, H * 0.62f + 5 * s, fs, Color{255, 235, 170, 255});
    }
    // ---- Nút ----
    float bh = 44 * s, bw = 118 * s, gap = 6 * s;
    float by = H - bh - 10 * s;
    if (button({W - bw - 10 * s, by, bw, bh}, "Thoát")) { fpExit(fp, sim); return; }
    if (button({W - 2 * bw - 10 * s - gap, by, bw, bh}, fp.control ? "Để AI tự quyết" : "Điều khiển", fp.control)) {
        fp.control = !fp.control;
        Mind& mm = sim.reg.get<Mind>(fp.who);
        mm.possessed = fp.control;
        if (!fp.control) { mm.needReplan = true; mm.nextThink = sim.tick; }
    }
    // tốc độ thời gian
    for (int i = 0; i < 3; ++i)
        if (button({W - ms - 10 * s + i * (ms / 3), ms + 16 * s, ms / 3 - 3 * s, 30 * s}, kSpeedLabels[i], app.speedIdx == i)) app.speedIdx = i;
    if (fp.control) {
        const char* names[] = {"Tấn công", "Ăn", "Thu thập", "Dựng lều"};
        const Brain::PlayerAct acts[] = {Brain::PlayerAct::Attack, Brain::PlayerAct::Eat, Brain::PlayerAct::Gather, Brain::PlayerAct::BuildHut};
        int n = sd.dexterous ? 4 : 2;
        for (int i = 0; i < n; ++i)
            if (button({W - bw - 10 * s - (i % 2) * (bw + gap), by - (1 + i / 2) * (bh + gap), bw, bh}, names[i], false, Color{90, 60, 50, 235})) fpAction(app, acts[i]);
        // Cần điều khiển ảo
        float rad = 60 * s;
        Vector2 o = fp.stick ? fp.stickOrigin : Vector2{20 * s + rad, H - 20 * s - rad};
        DrawCircleV(o, rad, Color{255, 255, 255, 40});
        DrawCircleLinesV(o, rad, Color{255, 255, 255, 120});
        Vector2 k = fp.stick ? fp.stickPos : o;
        float dx = k.x - o.x, dy = k.y - o.y, L = std::sqrt(dx * dx + dy * dy);
        if (L > rad) { k.x = o.x + dx / L * rad; k.y = o.y + dy / L * rad; }
        DrawCircleV(k, rad * 0.42f, Color{255, 255, 255, 120});
        ui::drawText("WASD/cần ảo: đi · kéo: nhìn · F tấn công · E ăn · G thu thập · B dựng lều · Tab thoát", 10 * s, pr.y + pr.height + 6 * s, 11 * s, Color{230, 230, 240, 200});
    } else {
        ui::drawText("Kéo màn hình để nhìn quanh · bấm \"Điều khiển\" để nhập vai sinh vật này", 10 * s, pr.y + pr.height + 6 * s, 12 * s, Color{230, 230, 240, 210});
    }
}

// ---------------------------------------------------------------------------
// Phòng thí nghiệm gen
// ---------------------------------------------------------------------------
// Thanh trượt: kéo để chỉnh giá trị trong [lo, hi]
bool slider(Rectangle r, float& v, float lo, float hi, const std::string& label, Color c) {
    gApp->uiRects.push_back(r);
    float s = gApp->uiScale;
    float lw = r.width * 0.38f;
    ui::drawText(label, r.x, r.y + (r.height - 12 * s) / 2, 12 * s, kText);
    Rectangle tr{r.x + lw, r.y + r.height * 0.35f, r.width - lw - 44 * s, r.height * 0.3f};
    DrawRectangleRounded(tr, 0.5f, 4, Color{0, 0, 0, 140});
    float t = (v - lo) / (hi - lo);
    DrawRectangleRounded({tr.x, tr.y, tr.width * clamp01(t), tr.height}, 0.5f, 4, c);
    DrawCircleV({tr.x + tr.width * clamp01(t), tr.y + tr.height / 2}, r.height * 0.32f, RAYWHITE);
    ui::drawText(fmt("%.2f", v), tr.x + tr.width + 6 * s, r.y + (r.height - 12 * s) / 2, 12 * s, kMuted);
    Rectangle hit{tr.x - 8 * s, r.y, tr.width + 16 * s, r.height};
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && pointInRect(gApp->pressPos, hit)) {
        float nt = clamp01((GetMousePosition().x - tr.x) / tr.width);
        v = lo + nt * (hi - lo);
        return true;
    }
    return false;
}

// Bộ chọn vòng: [<] nhãn [>]
int cycler(Rectangle r, const std::string& label, int idx, int count) {
    float bw = r.height;
    if (button({r.x, r.y, bw, r.height}, "<")) idx = (idx + count - 1) % count;
    if (button({r.x + r.width - bw, r.y, bw, r.height}, ">")) idx = (idx + 1) % count;
    float fs = std::min(14 * gApp->uiScale, r.height * 0.45f);
    std::string t = label;
    while (t.size() > 4 && ui::measureText(t, fs) > r.width - 2 * bw - 8) { t.pop_back(); while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0x80) t.pop_back(); }
    ui::drawText(t, r.x + (r.width - ui::measureText(t, fs)) / 2, r.y + (r.height - fs) / 2, fs, kText);
    return idx;
}

// Ô nhập chữ (bàn phím PC; điện thoại dùng nút "tên ngẫu nhiên")
void textField(Rectangle r, std::string& text, int id) {
    GeneLab& L = gApp->lab;
    gApp->uiRects.push_back(r);
    bool focused = L.focus == id;
    DrawRectangleRounded(r, 0.25f, 6, focused ? Color{60, 70, 100, 255} : kPanelLight);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && pointInRect(GetMousePosition(), r) && pointInRect(gApp->pressPos, r)) L.focus = id;
    if (focused) {
        for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed()) {
            if (text.size() > 40) continue;
            // mã hoá UTF-8 (gõ được tiếng Việt có dấu nếu bộ gõ hệ điều hành gửi ký tự Unicode)
            if (ch < 0x80) text += static_cast<char>(ch);
            else if (ch < 0x800) { text += static_cast<char>(0xC0 | (ch >> 6)); text += static_cast<char>(0x80 | (ch & 0x3F)); }
            else { text += static_cast<char>(0xE0 | (ch >> 12)); text += static_cast<char>(0x80 | ((ch >> 6) & 0x3F)); text += static_cast<char>(0x80 | (ch & 0x3F)); }
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) {
            text.pop_back();
            while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) text.pop_back();
        }
        if (IsKeyPressed(KEY_ENTER)) L.focus = 0;
    }
    float fs = std::min(15 * gApp->uiScale, r.height * 0.5f);
    std::string shown = text + (focused && static_cast<int>(GetTime() * 2) % 2 ? "|" : "");
    ui::drawText(shown, r.x + 8, r.y + (r.height - fs) / 2, fs, kText);
}

std::string randomSpeciesName(Rng& rng) {
    static const char* a[] = {"Kra", "Vel", "Zor", "Mi", "Tha", "Ul", "Gro", "Sy", "Ae", "Bru", "Qua", "Lo", "Dre", "Nym", "Ox"};
    static const char* b[] = {"nax", "ri", "thul", "ven", "mo", "rak", "lith", "pha", "dor", "sa", "gul", "ix", "ra", "ne", "bor"};
    static const char* c[] = {"", "", "ia", "on", "us", "a", "ek", "oth"};
    std::string n = std::string(a[rng.irange(0, 14)]) + b[rng.irange(0, 14)] + c[rng.irange(0, 7)];
    return n;
}

const char* planName(BodyPlan p) {
    static const char* n[] = {"Dáng người", "Bốn chân", "Chim", "Cá", "Dơi", "Nhện", "Bò sát", "Khổng lồ", "Linh trưởng"};
    return n[static_cast<int>(p)];
}
const char* habitatName(Habitat h) {
    static const char* n[] = {"Trên cạn", "Dưới nước", "Trên trời", "Trong hang", "Lưỡng cư"};
    return n[static_cast<int>(h)];
}
const char* dietName(Diet d) {
    static const char* n[] = {"Ăn cỏ", "Ăn thịt", "Ăn tạp"};
    return n[static_cast<int>(d)];
}
const char* socialName(SocialType t) {
    static const char* n[] = {"Sống đơn độc", "Sống theo đàn", "Săn theo bầy", "Bộ tộc"};
    return n[static_cast<int>(t)];
}

// Vẽ bản xem trước con vật theo hình dáng + màu + kích thước (to gấp nhiều lần sprite thật)
void drawSpeciesPreview(const SpeciesDef& d, Rectangle r, float time) {
    DrawRectangleRounded(r, 0.06f, 6, Color{30, 60, 40, 255});
    DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y + r.height * 0.7f), static_cast<int>(r.width), static_cast<int>(r.height * 0.3f), Color{70, 120, 60, 255});
    float k = std::min(r.width, r.height) / 18.0f * std::clamp(d.size, 0.3f, 2.2f) / 1.3f;
    float cx = r.x + r.width / 2, cy = r.y + r.height * 0.68f;
    Color c{d.r, d.g, d.b, 255};
    auto sh = [](Color c, int v) { return Color{static_cast<unsigned char>(std::clamp(c.r + v, 0, 255)), static_cast<unsigned char>(std::clamp(c.g + v, 0, 255)), static_cast<unsigned char>(std::clamp(c.b + v, 0, 255)), 255}; };
    float bob = std::sin(time * 4) * k * 0.15f;
    if (d.geneBonus(GeneEffect::Glow) > 0) DrawCircleGradient(static_cast<int>(cx), static_cast<int>(cy - 2 * k), 6 * k, Color{d.r, d.g, d.b, 120}, Color{d.r, d.g, d.b, 0});
    switch (d.plan) {
    case BodyPlan::Humanoid: case BodyPlan::Primate:
        DrawRectangleV({cx - 1.6f * k, cy - 3.2f * k + bob}, {3.2f * k, 3.2f * k}, d.plan == BodyPlan::Humanoid ? Color{90, 110, 170, 255} : sh(c, -20));
        DrawRectangleV({cx - 1.1f * k, cy - 5.4f * k + bob}, {2.2f * k, 2.2f * k}, c);
        DrawRectangleV({cx - 1.3f * k, cy}, {1.0f * k, 1.6f * k}, sh(c, -40));
        DrawRectangleV({cx + 0.3f * k, cy}, {1.0f * k, 1.6f * k}, sh(c, -40));
        break;
    case BodyPlan::Bird: case BodyPlan::Bat: {
        float flap = std::sin(time * 9) * 2.5f * k;
        DrawTriangle({cx, cy - 4 * k}, {cx - 6 * k, cy - 4 * k - flap}, {cx - 1 * k, cy - 3 * k}, sh(c, -25));
        DrawTriangle({cx, cy - 4 * k}, {cx + 1 * k, cy - 3 * k}, {cx + 6 * k, cy - 4 * k - flap}, sh(c, -25));
        DrawCircleV({cx, cy - 4 * k}, 1.5f * k, c);
        break;
    }
    case BodyPlan::Fish:
        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 3 * k), 4 * k, 1.8f * k, c);
        DrawTriangle({cx - 3.6f * k, cy - 3 * k}, {cx - 6.5f * k, cy - 5 * k}, {cx - 6.5f * k, cy - 1 * k}, sh(c, -30));
        break;
    case BodyPlan::Spider:
        for (int i = 0; i < 4; ++i) {
            float a = 0.4f + i * 0.5f;
            DrawLineEx({cx, cy - 1.5f * k}, {cx - std::cos(a) * 5 * k, cy - 1.5f * k + std::sin(a - 1.0f) * 3 * k}, k * 0.4f, sh(c, -30));
            DrawLineEx({cx, cy - 1.5f * k}, {cx + std::cos(a) * 5 * k, cy - 1.5f * k + std::sin(a - 1.0f) * 3 * k}, k * 0.4f, sh(c, -30));
        }
        DrawCircleV({cx, cy - 1.5f * k}, 2 * k, c);
        break;
    case BodyPlan::Reptile:
        DrawRectangleV({cx - 5 * k, cy - 1.6f * k}, {10 * k, 1.8f * k}, c);
        DrawTriangle({cx - 5 * k, cy - 1.6f * k}, {cx - 8 * k, cy - 0.7f * k}, {cx - 5 * k, cy + 0.2f * k}, sh(c, -25));
        DrawRectangleV({cx + 5 * k, cy - 1.3f * k}, {2.4f * k, 1.2f * k}, sh(c, 15));
        break;
    default: {   // bốn chân / khổng lồ
        float g = d.plan == BodyPlan::Giant ? 1.3f : 1.0f;
        DrawRectangleV({cx - 4 * k * g, cy - 3.4f * k * g + bob}, {8 * k * g, 3.4f * k * g}, sh(c, -15));
        DrawRectangleV({cx + 3 * k * g, cy - 5 * k * g + bob}, {2.8f * k * g, 2.6f * k * g}, c);
        for (int i = 0; i < 4; ++i) DrawRectangleV({cx - 3.6f * k * g + i * 2.2f * k * g, cy + bob}, {0.9f * k, 1.4f * k}, sh(c, -40));
        if (d.plan == BodyPlan::Giant) DrawLineEx({cx + 5.5f * k * g, cy - 2.5f * k * g}, {cx + 7 * k * g, cy - 4 * k * g}, k * 0.5f, Color{245, 240, 225, 255});
        break;
    }
    }
    if (d.geneBonus(GeneEffect::Wings) > 0 && d.plan != BodyPlan::Bird && d.plan != BodyPlan::Bat) {
        float flap = std::sin(time * 9) * 2 * k;
        DrawTriangle({cx, cy - 4 * k}, {cx - 5 * k, cy - 7 * k - flap}, {cx + 1 * k, cy - 4.5f * k}, Color{240, 240, 255, 200});
    }
    if (d.geneBonus(GeneEffect::Armor) > 0) DrawRectangleLinesEx({cx - 4 * k, cy - 5 * k, 8 * k, 5 * k}, k * 0.25f, Color{200, 200, 210, 180});
    if (d.geneBonus(GeneEffect::Venom) > 0) DrawCircleV({cx + 4.5f * k, cy - 3 * k}, k * 0.6f, Color{150, 255, 60, 255});
}

void labResetDraft(GeneLab& L, Simulation& sim) {
    L.draft = sim.species[L.base];
    L.draft.name = sim.species[sim.species.root(L.base)].name + " " + randomSpeciesName(sim.rng);
    L.draft.population = 0;
    L.hasDraft = true;
}

void drawGeneLab(App& app) {
    Simulation& sim = *app.sim;
    GeneLab& L = app.lab;
    float s = app.uiScale;
    int W = GetScreenWidth(), H = GetScreenHeight();
    if (!L.hasDraft) labResetDraft(L, sim);
    SpeciesDef& d = L.draft;
    int nsp = sim.species.count();
    L.base = std::min(L.base, nsp - 1); L.donor = std::min(L.donor, nsp - 1);
    Rectangle full{8 * s, 8 * s, W - 16 * s, H - 16 * s};
    panel(full, Color{14, 16, 24, 245});
    ui::drawText("Phòng thí nghiệm gen — cấy ghép, tạo gen mới, chỉnh ngoại hình → loài mới", full.x + 12 * s, full.y + 10 * s, 17 * s, kAccent);
    if (button({full.x + full.width - 44 * s, full.y + 6 * s, 38 * s, 32 * s}, "x")) { L.open = false; return; }
    bool wide = W > H;
    float top = full.y + 44 * s;
    // ---- Cột xem trước ----
    float leftW = wide ? std::min(330 * s, full.width * 0.36f) : full.width - 24 * s;
    float prevH = wide ? std::min(220 * s, (full.height - 60 * s) * 0.45f) : std::min(150 * s, H * 0.18f);
    Rectangle pv{full.x + 12 * s, top, leftW, prevH};
    drawSpeciesPreview(d, pv, static_cast<float>(GetTime()));
    float y = pv.y + pv.height + 8 * s;
    float bh = 36 * s;
    textField({pv.x, y, leftW - bh * 2.6f - 6 * s, bh}, d.name, 1);
    if (button({pv.x + leftW - bh * 2.6f, y, bh * 2.6f, bh}, "Tên ngẫu nhiên")) d.name = sim.species[sim.species.root(L.base)].name + " " + randomSpeciesName(sim.rng);
    y += bh + 6 * s;
    Writer w{pv.x, y, leftW, 12 * s};
    w.line(fmt("%s · %s · %s · cỡ %.2f", planName(d.plan), habitatName(d.habitat), dietName(d.diet), d.size), kMuted);
    w.line(std::string(d.dexterous ? "Có đôi tay khéo léo" : "Không có tay khéo") + (d.monster ? " · Quái vật" : "") + " · " + socialName(d.social), kMuted);
    std::string cg;
    for (const CustomGene& g : d.customGenes) { if (!cg.empty()) cg += ", "; cg += g.name + " (" + geneEffectName(g.effect) + fmt(" %.1f)", g.strength); }
    if (!cg.empty()) w.wrapped("Gen tự tạo: " + cg, Color{255, 200, 255, 255}, pv.y + (wide ? full.height - 150 * s : prevH + 140 * s));
    float createY = wide ? full.y + full.height - bh * 1.3f - 12 * s : y + 3 * bh;
    if (button({pv.x, createY, leftW, bh * 1.3f}, "TẠO LOÀI MỚI & THẢ VÀO THẾ GIỚI", false, Color{60, 140, 80, 255})) {
        SpeciesDef nd = d;
        nd.parent = L.base;
        nd.donor = L.donor;
        nd.engineered = true;
        nd.bornTick = sim.tick;
        nd.population = 0;
        nd.capShare = nd.dexterous && !nd.monster ? 0.3f : std::clamp(0.03f / std::max(0.4f, nd.size), 0.006f, 0.06f);
        if (nd.plan == BodyPlan::Humanoid && !nd.dexterous) nd.plan = BodyPlan::Primate;
        int id = sim.species.add(nd);
        app.spawnSpecies = id;
        app.tool = Power::Spawn;
        app.category = ToolCategory::Life;
        sim.chronicle.add(sim.tick, EventKind::Evolution,
                          "Thần linh tạo ra loài mới \"" + nd.name + "\" từ " + sim.species[L.base].name + (L.donor != L.base ? " × " + sim.species[L.donor].name : "") +
                              (nd.customGenes.empty() ? "" : " mang " + std::to_string(nd.customGenes.size()) + " gen tự tạo"));
        L.open = false;
        L.hasDraft = false;
        app.panel = SidePanel::Species;
        return;
    }
    // ---- Cột chỉnh sửa (thẻ) ----
    Rectangle ed = wide ? Rectangle{pv.x + leftW + 14 * s, top, full.x + full.width - (pv.x + leftW + 14 * s) - 12 * s, full.height - 56 * s}
                        : Rectangle{full.x + 12 * s, createY + bh * 1.3f + 10 * s, full.width - 24 * s, full.y + full.height - (createY + bh * 1.3f + 10 * s) - 10 * s};
    const char* tabs[] = {"Cấy ghép", "Bộ gen", "Ngoại hình", "Gen mới"};
    float tw = (ed.width - 3 * 6 * s) / 4;
    for (int i = 0; i < 4; ++i)
        if (button({ed.x + i * (tw + 6 * s), ed.y, tw, bh}, tabs[i], L.tab == i)) L.tab = i;
    float ey = ed.y + bh + 10 * s, rowH = std::min(34 * s, (ed.height - bh - 20 * s) / 14.0f);
    float ew = ed.width;
    switch (L.tab) {
    case 0: {   // ---- Cấy ghép gen giữa hai loài ----
        ui::drawText("Loài gốc (bản nháp bắt đầu từ loài này):", ed.x, ey, 12 * s, kMuted); ey += 16 * s;
        int nb = cycler({ed.x, ey, ew, rowH}, sim.species[L.base].name, L.base, nsp);
        if (nb != L.base) { L.base = nb; labResetDraft(L, sim); }
        ey += rowH + 10 * s;
        ui::drawText("Loài cho gen:", ed.x, ey, 12 * s, kMuted); ey += 16 * s;
        L.donor = cycler({ed.x, ey, ew, rowH}, sim.species[L.donor].name, L.donor, nsp);
        ey += rowH + 12 * s;
        const SpeciesDef& D = sim.species[L.donor];
        float hw = (ew - 6 * s) / 2;
        auto act = [&](int i, const char* label) { return button({ed.x + (i % 2) * (hw + 6 * s), ey + (i / 2) * (rowH + 6 * s), hw, rowH}, label); };
        if (act(0, "Lai 50/50 toàn bộ gen")) for (int g = 0; g < kGeneCount; ++g) d.baseGenome.g[g] = (d.baseGenome.g[g] + D.baseGenome.g[g]) * 0.5f;
        if (act(1, "Lấy gen trí tuệ + khéo tay")) { d.baseGenome[Gene::Intelligence] = D.baseGenome[Gene::Intelligence]; d.baseGenome[Gene::Dexterity] = D.baseGenome[Gene::Dexterity]; d.dexterous = d.dexterous || D.dexterous; }
        if (act(2, "Lấy hình dáng cơ thể")) d.plan = D.plan;
        if (act(3, "Lấy môi trường sống")) d.habitat = D.habitat;
        if (act(4, "Lấy chế độ ăn")) d.diet = D.diet;
        if (act(5, "Lấy màu & kích thước")) { d.r = D.r; d.g = D.g; d.b = D.b; d.size = D.size; }
        if (act(6, "Lấy sức mạnh, tốc độ")) { d.baseHp = D.baseHp; d.baseDamage = D.baseDamage; d.baseSpeed = D.baseSpeed; d.baseGenome[Gene::Strength] = D.baseGenome[Gene::Strength]; d.baseGenome[Gene::Speed] = D.baseGenome[Gene::Speed]; }
        if (act(7, "Lấy các gen tự tạo")) for (const CustomGene& g : D.customGenes) addCustomGene(d, g);
        if (act(8, "Lấy khả năng chịu nhiệt")) { d.baseGenome[Gene::ColdResist] = D.baseGenome[Gene::ColdResist]; d.baseGenome[Gene::HeatResist] = D.baseGenome[Gene::HeatResist]; }
        if (act(9, "Đột biến ngẫu nhiên")) {
            for (float& v : d.baseGenome.g) v = clamp01(v + sim.rng.gauss(0.15f));
            d.r = static_cast<uint8_t>(std::clamp(d.r + sim.rng.irange(-60, 60), 20, 255));
            d.g = static_cast<uint8_t>(std::clamp(d.g + sim.rng.irange(-60, 60), 20, 255));
            d.b = static_cast<uint8_t>(std::clamp(d.b + sim.rng.irange(-60, 60), 20, 255));
            d.size = std::clamp(d.size * sim.rng.range(0.7f, 1.4f), 0.15f, 3.0f);
        }
        ey += 5 * (rowH + 6 * s) + 8 * s;
        w = Writer{ed.x, ey, ew, 12 * s};
        w.wrapped(std::string("Khoảng cách di truyền với loài gốc: ") + fmt("%.2f", Genome::distance(d.baseGenome, sim.species[L.base].baseGenome)) +
                  ". Loài mới sẽ tiếp tục tự tiến hoá, có thể đột biến ra gen mới và phân nhánh thành các loài khác.", kMuted);
        break;
    }
    case 1: {   // ---- Từng gen ----
        for (int g = 0; g < kGeneCount; ++g) {
            slider({ed.x, ey, ew, rowH}, d.baseGenome.g[g], 0.0f, 1.0f, geneName(static_cast<Gene>(g)), Color{120, 200, 255, 255});
            ey += rowH + 2 * s;
        }
        break;
    }
    case 2: {   // ---- Ngoại hình & sinh thái ----
        float rr = d.r, gg = d.g, bb = d.b;
        slider({ed.x, ey, ew, rowH}, rr, 0, 255, "Màu đỏ", Color{230, 80, 80, 255}); ey += rowH + 2 * s;
        slider({ed.x, ey, ew, rowH}, gg, 0, 255, "Màu lục", Color{80, 220, 100, 255}); ey += rowH + 2 * s;
        slider({ed.x, ey, ew, rowH}, bb, 0, 255, "Màu lam", Color{90, 140, 255, 255}); ey += rowH + 2 * s;
        d.r = static_cast<uint8_t>(rr); d.g = static_cast<uint8_t>(gg); d.b = static_cast<uint8_t>(bb);
        slider({ed.x, ey, ew, rowH}, d.size, 0.15f, 3.0f, "Kích thước", Color{230, 200, 90, 255}); ey += rowH + 2 * s;
        slider({ed.x, ey, ew, rowH}, d.baseSpeed, 0.04f, 0.3f, "Tốc độ gốc", Color{230, 200, 90, 255}); ey += rowH + 2 * s;
        slider({ed.x, ey, ew, rowH}, d.baseHp, 5, 300, "Máu gốc", Color{230, 90, 90, 255}); ey += rowH + 2 * s;
        slider({ed.x, ey, ew, rowH}, d.baseDamage, 0.5f, 30, "Sát thương gốc", Color{230, 90, 90, 255}); ey += rowH + 6 * s;
        d.plan = static_cast<BodyPlan>(cycler({ed.x, ey, ew, rowH}, std::string("Hình dáng: ") + planName(d.plan), static_cast<int>(d.plan), 9)); ey += rowH + 4 * s;
        d.habitat = static_cast<Habitat>(cycler({ed.x, ey, ew, rowH}, std::string("Môi trường: ") + habitatName(d.habitat), static_cast<int>(d.habitat), 5)); ey += rowH + 4 * s;
        d.diet = static_cast<Diet>(cycler({ed.x, ey, ew, rowH}, std::string("Thức ăn: ") + dietName(d.diet), static_cast<int>(d.diet), 3)); ey += rowH + 4 * s;
        d.social = static_cast<SocialType>(cycler({ed.x, ey, ew, rowH}, std::string("Xã hội: ") + socialName(d.social), static_cast<int>(d.social), 4)); ey += rowH + 6 * s;
        float hw = (ew - 6 * s) / 2;
        if (button({ed.x, ey, hw, rowH}, d.dexterous ? "Tay khéo: CÓ" : "Tay khéo: không", d.dexterous)) {
            d.dexterous = !d.dexterous;
            d.baseGenome[Gene::Dexterity] = d.dexterous ? std::max(0.7f, d.baseGenome[Gene::Dexterity]) : std::min(0.3f, d.baseGenome[Gene::Dexterity]);
            if (d.dexterous && d.social != SocialType::Tribal) d.social = SocialType::Tribal;
        }
        if (button({ed.x + hw + 6 * s, ey, hw, rowH}, d.monster ? "Quái vật: CÓ" : "Quái vật: không", d.monster)) d.monster = !d.monster;
        break;
    }
    case 3: {   // ---- Tạo gen mới tuỳ ý ----
        ui::drawText("Tên gen:", ed.x, ey, 12 * s, kMuted); ey += 16 * s;
        if (L.newGeneName.empty()) L.newGeneName = "Gen " + randomSpeciesName(sim.rng);
        textField({ed.x, ey, ew - 130 * s, rowH}, L.newGeneName, 2);
        if (button({ed.x + ew - 124 * s, ey, 124 * s, rowH}, "Tên ngẫu nhiên")) L.newGeneName = "Gen " + randomSpeciesName(sim.rng);
        ey += rowH + 10 * s;
        L.newEffect = cycler({ed.x, ey, ew, rowH}, std::string("Hiệu ứng: ") + geneEffectName(static_cast<GeneEffect>(L.newEffect)), L.newEffect, kGeneEffectCount);
        ey += rowH + 4 * s;
        ui::drawText(geneEffectDesc(static_cast<GeneEffect>(L.newEffect)), ed.x, ey, 12 * s, Color{255, 220, 150, 255});
        ey += 20 * s;
        slider({ed.x, ey, ew, rowH}, L.newStrength, 0.05f, 1.0f, "Cường độ", Color{255, 150, 255, 255});
        ey += rowH + 8 * s;
        if (button({ed.x, ey, ew, rowH * 1.2f}, "Cấy gen này vào loài mới", false, Color{120, 70, 140, 255})) {
            addCustomGene(d, {L.newGeneName, static_cast<GeneEffect>(L.newEffect), L.newStrength});
            L.message = "Đã cấy gen \"" + L.newGeneName + "\"";
            L.messageTime = GetTime();
            L.newGeneName.clear();
        }
        ey += rowH * 1.2f + 10 * s;
        ui::drawText("Các gen tự tạo trong bản nháp:", ed.x, ey, 12 * s, kMuted); ey += 18 * s;
        for (size_t i = 0; i < d.customGenes.size() && ey < ed.y + ed.height - rowH; ++i) {
            const CustomGene& g = d.customGenes[i];
            ui::drawText(g.name + " — " + geneEffectName(g.effect) + fmt(" (%.2f)", g.strength), ed.x + 6 * s, ey + (rowH - 13 * s) / 2, 13 * s, kText);
            if (button({ed.x + ew - rowH, ey, rowH, rowH}, "x")) { d.customGenes.erase(d.customGenes.begin() + static_cast<long>(i)); break; }
            ey += rowH + 2 * s;
        }
        if (!L.message.empty() && GetTime() - L.messageTime < 2.5) ui::drawText(L.message, ed.x, ed.y + ed.height - 18 * s, 13 * s, Color{150, 255, 170, 255});
        break;
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
    // Bố cục co giãn: màn hình ngang thấp (điện thoại xoay ngang) → chia 2 cột để vừa chiều cao
    const int nsc = static_cast<int>(Scenario::Count), nwt = static_cast<int>(WorldType::Count);
    float bh = 50 * s, gap = 8 * s;
    float tfs = 46 * s, sfs = 14 * s;
    float headerH = tfs * 1.2f + sfs * 1.8f;
    float colH = nsc * (bh + gap) + bh * 0.8f + gap + 3 * (bh * 0.8f + gap) + bh * 1.2f + bh;   // chiều cao bố cục 1 cột
    bool twoCols = H < headerH + colH + 40 * s && W > H;
    if (twoCols) { float k = std::clamp((H - 20 * s) / (headerH + colH * 0.55f), 0.6f, 1.0f); bh *= k; gap *= k; tfs *= k; sfs *= k; headerH = tfs * 1.2f + sfs * 1.8f; }
    float top = twoCols ? 8 * s : H * 0.06f;
    std::string title = "WorldBox AI";
    ui::drawText(title, (W - ui::measureText(title, tfs)) / 2, top, tfs, Color{255, 220, 120, 255});
    std::string sub = "Sandbox thần thánh — mỗi sinh vật có một bộ óc riêng, tự tiến hoá thành nền văn minh";
    if (ui::measureText(sub, sfs) > W - 20) sfs *= (W - 20) / ui::measureText(sub, sfs);
    ui::drawText(sub, (W - ui::measureText(sub, sfs)) / 2, top + tfs * 1.15f, sfs, kMuted);

    float y0 = top + headerH + gap;
    float colW = twoCols ? std::min(460 * s, (W - 30 * s) / 2) : std::min(440 * s, W - 30.0f);
    float xL = twoCols ? W / 2.0f - colW - 6 * s : (W - colW) / 2;
    float xR = twoCols ? W / 2.0f + 6 * s : xL;
    // ---- Cột trái: kịch bản ----
    const char* desc[] = {"4 chủng tộc + muông thú trên cạn, biển, trời, hang", "Bầy sói học săn theo bầy → bộ tộc sói",
                          "Người nguyên thuỷ → công cụ → làng → quốc gia", "Hai nhóm nhân bản, hai môi trường → hai con đường"};
    float y = y0;
    for (int i = 0; i < nsc; ++i) {
        if (button({xL, y, colW, bh}, std::string(scenarioName(static_cast<Scenario>(i))) + " — " + desc[i], app.scenario == static_cast<Scenario>(i)))
            app.scenario = static_cast<Scenario>(i);
        y += bh + gap;
    }
    if (twoCols) y = y0;
    // ---- Cột phải (hoặc tiếp theo): kích thước + kiểu hành tinh + bắt đầu ----
    const int sizes[] = {96, 128, 160};
    const char* sizeNames[] = {"Nhỏ", "Vừa", "Lớn"};
    float sw = (colW - 2 * gap) / 3;
    for (int i = 0; i < 3; ++i)
        if (button({xR + i * (sw + gap), y, sw, bh * 0.8f}, std::string("Hành tinh ") + sizeNames[i], app.worldSize == sizes[i])) app.worldSize = sizes[i];
    y += bh * 0.8f + gap;
    // Kiểu hành tinh: lưới 3 × 2
    float tw = (colW - 2 * gap) / 3;
    for (int i = 0; i < nwt; ++i) {
        Rectangle r{xR + (i % 3) * (tw + gap), y + (i / 3) * (bh * 0.8f + gap), tw, bh * 0.8f};
        if (button(r, worldTypeName(static_cast<WorldType>(i)), app.worldType == static_cast<WorldType>(i))) app.worldType = static_cast<WorldType>(i);
    }
    y += 2 * (bh * 0.8f + gap) + gap;
    if (button({xR, y, colW, bh * 1.2f}, "BẮT ĐẦU", false, Color{60, 140, 80, 255})) startWorld(app);
    y += bh * 1.2f + gap;
    if (app.sim && button({xR, y, colW, bh * 0.85f}, "Tiếp tục thế giới hiện tại")) app.inMenu = false;
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
    app.worldSize = 96;
#endif
    ui::initText(16 * app.uiScale);
}

void appStartWorld(App& app) { startWorld(app); }

void appShutdown(App& app) {
    fpRelease(app.fp);
    app.renderer.shutdown();
    ui::shutdownText();
}

// Màn hình đổi kích thước (xoay điện thoại dọc ↔ ngang, kéo cửa sổ PC): tính lại tỉ lệ
// giao diện và tâm camera, nếu không mọi thứ bị lệch / kéo giãn.
void handleResize(App& app) {
    int W = GetScreenWidth(), H = GetScreenHeight();
    if (W == app.lastW && H == app.lastH) return;
    app.lastW = W; app.lastH = H;
    float minDim = static_cast<float>(std::min(W, H));
#if defined(PLATFORM_ANDROID)
    app.uiScale = std::clamp(minDim / 500.0f, 1.2f, 3.5f);
#else
    app.uiScale = std::clamp(minDim / 620.0f, 0.9f, 3.0f);
#endif
    app.cam.offset = {W / 2.0f, H / 2.0f};
    if (app.sim) clampCamera(app);
}

void appFrame(App& app) {
    gApp = &app;
    handleResize(app);
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
    if (IsKeyPressed(KEY_TAB) && !app.fp.active) app.panel = app.panel == SidePanel::Chronicle ? SidePanel::None : SidePanel::Chronicle;
    if (!app.fp.active && (IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE))) app.inMenu = true; // nút Back Android

    if (app.lab.open) { /* phòng thí nghiệm gen chiếm toàn màn hình */ }
    else if (app.fp.active) handleFpInput(app);
    else if (app.globe) handleGlobeInput(app);
    else handleWorldInput(app);

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
    if (app.globe) sim.setFocusRect(-1, -1, -1, -1);   // nhìn cả hành tinh: không vùng nào được ưu tiên
    else sim.setFocusRect(tl.x / kTileWorld, tl.y / kTileWorld, br.x / kTileWorld, br.y / kTileWorld);
    // Texture địa hình: cập nhật thưa (mỗi 4 khung) để tiết kiệm băng thông GPU
    if (app.frame % 4 == 0 || sim.world.fullRedraw) app.renderer.sync(sim);

    BeginDrawing();
    if (app.fp.active) {
        fpRender(app.fp, sim, app.renderer, static_cast<float>(GetTime()));
        drawFpHud(app);
        EndDrawing();
        return;
    }
    if (app.globe) {
        app.renderer.drawGlobe(sim, app.globeView, app.opt, static_cast<float>(GetTime()), app.uiScale);
        drawHud(app);
        EndDrawing();
        return;
    }
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
    if (app.lab.open) drawGeneLab(app);   // phòng thí nghiệm gen phủ toàn màn hình (không vẽ HUD bên dưới để tránh bấm xuyên)
    else drawHud(app);
    EndDrawing();
}

} // namespace wb
