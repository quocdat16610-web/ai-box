// =============================================================================
// main.cpp — Điểm vào game (dùng chung cho PC và Android).
//
// Trên Android, raylib cung cấp sẵn android_main() (native_app_glue) và gọi
// hàm main() này → cùng một mã nguồn chạy trên cả hai nền tảng.
// =============================================================================
#include "raylib.h"
#include "Ui.h"
#include <cstdlib>
#include <string>

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
#if defined(PLATFORM_ANDROID)
    InitWindow(0, 0, "WorldBox AI"); // toàn màn hình theo kích thước thiết bị
#else
    InitWindow(1280, 760, "WorldBox AI — Sandbox văn minh tự tiến hoá");
#endif
    SetExitKey(KEY_NULL); // ESC/Back mở menu thay vì thoát
    SetTargetFPS(60);
    wb::App app;
    wb::appInit(app);
#if !defined(PLATFORM_ANDROID)
    // Hỗ trợ kiểm thử tự động: WB_AUTOSTART=sandbox|wolves|humans, WB_SPEED=0..4,
    // WB_ZOOM=hệ số, WB_SELECT=1 (chọn sinh vật đầu tiên), WB_SHOT=đường_dẫn.png, WB_FRAMES=số khung
    const char* autostart = std::getenv("WB_AUTOSTART");
    const char* shot = std::getenv("WB_SHOT");
    int frames = std::getenv("WB_FRAMES") ? std::atoi(std::getenv("WB_FRAMES")) : 300;
    if (autostart && std::string(autostart) == "menu") { /* chỉ hiện menu */ } else if (autostart) {
        app.scenario = wb::scenarioFromString(autostart);
        if (const char* sz = std::getenv("WB_SIZE")) app.worldSize = std::atoi(sz);
        if (const char* wt = std::getenv("WB_WORLD")) app.worldType = static_cast<wb::WorldType>(std::atoi(wt) % static_cast<int>(wb::WorldType::Count));
        wb::appStartWorld(app);
        if (const char* sp = std::getenv("WB_SPEED")) app.speedIdx = std::atoi(sp);
        if (const char* z = std::getenv("WB_ZOOM")) app.cam.zoom *= static_cast<float>(std::atof(z));
        if (const char* pre = std::getenv("WB_PRESIM")) for (int i = 0, n = std::atoi(pre); i < n; ++i) app.sim->step();
        if (std::getenv("WB_DEBUG")) app.opt.debugThoughts = true;
        if (const char* gl = std::getenv("WB_GLOBE")) { app.globe = true; app.globeView.dist = static_cast<float>(std::atof(gl)); }
        if (const char* l = std::getenv("WB_LAYER")) app.opt.layer = static_cast<wb::Layer>(std::atoi(l) % static_cast<int>(wb::Layer::Count));
        if (const char* pn = std::getenv("WB_PANEL")) {
            std::string v = pn;
            if (v == "chem") app.panel = wb::SidePanel::Chemistry;
            if (v == "none") app.panel = wb::SidePanel::None;
        }
        if (const char* gx = std::getenv("WB_GOTO_FORGE")) {
            (void)gx;   // bay camera tới lò luyện đầu tiên (minh hoạ hoá học trong lò)
            for (auto& g : app.sim->groups)
                if (g.alive && g.forgeX >= 0) { app.cam.target = {(g.forgeX + 0.5f) * wb::kTileWorld, (g.forgeY + 0.5f) * wb::kTileWorld}; app.cam.zoom = 5; break; }
        }
    }
    int frame = 0;
    while (!WindowShouldClose()) {
        if (autostart && std::getenv("WB_SELECT") && !std::getenv("WB_GOTO_FORGE") && frame == 2 && !app.sim->agents.empty()) {
            // chọn sinh vật thông minh nhất để minh hoạ bảng "suy nghĩ"
            wb::Entity best = app.sim->agents.front();
            for (wb::Entity e : app.sim->agents)
                if (app.sim->reg.get<wb::Mind>(e).intellect > app.sim->reg.get<wb::Mind>(best).intellect) best = e;
            app.opt.selected = best;
            app.panel = wb::SidePanel::Creature;
            const wb::Transform& tr = app.sim->reg.get<wb::Transform>(best);
            app.cam.target = {tr.pos.x * wb::kTileWorld, tr.pos.y * wb::kTileWorld};
        }
        wb::appFrame(app);
        if (shot && ++frame == frames) { TakeScreenshot(shot); break; }
    }
#else
    while (!WindowShouldClose()) wb::appFrame(app);
#endif
    wb::appShutdown(app);
    CloseWindow();
    return 0;
}
