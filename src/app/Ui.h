// =============================================================================
// Ui.h — Trạng thái ứng dụng + giao diện người dùng thân thiện cảm ứng.
//
//   * Nút bấm cỡ lớn (≥ 44dp), tự co giãn theo DPI/kích thước màn hình.
//   * 1 ngón: dùng công cụ (vẽ địa hình, thả sinh vật...) hoặc kéo để di chuyển
//     bản đồ khi đang ở chế độ "Quan sát"; chạm để chọn sinh vật/nhóm.
//   * 2 ngón: chụm để thu phóng, kéo để di chuyển (pinch & pan).
//   * PC: chuột trái = công cụ, chuột phải/giữa = kéo bản đồ, lăn = zoom,
//     phím Space tạm dừng, 1–5 tốc độ, F1 debug suy nghĩ, Tab biên niên sử.
// =============================================================================
#pragma once
#include "raylib.h"
#include "Renderer.h"
#include "Voice.h"
#include "../sim/GodPowers.h"
#include "../sim/Scenarios.h"
#include "../sim/Simulation.h"
#include <memory>
#include <vector>

namespace wb {

enum class ToolCategory : uint8_t { Inspect, Terrain, Matter, Life, Disaster, Weather, Divine, Count };
enum class SidePanel : uint8_t { None, Creature, Group, Chronicle, Species, Chemistry };

struct App {
    std::unique_ptr<Simulation> sim;
    Renderer renderer;
    Camera2D cam{};
    RenderOptions opt;
    bool inMenu = true;
    Scenario scenario = Scenario::Sandbox;
    int worldSize = 192;

    ToolCategory category = ToolCategory::Inspect;
    Power tool = Power::Inspect;
    int brush = 2;
    int spawnSpecies = 0;
    int speedIdx = 1;          // 0 = tạm dừng
    SidePanel panel = SidePanel::None;
    int chronicleScroll = 0;

    float uiScale = 1.0f;
    float tickAccum = 0;
    int frame = 0;

    // Trạng thái nhập liệu
    std::vector<Rectangle> uiRects, uiRectsPrev; // vùng UI (khung trước) để chặn thao tác bản đồ
    bool pointerDownOnWorld = false;
    bool dragMoved = false;
    Vector2 pressPos{}, lastPointer{};
    float pinchStartDist = 0, pinchStartZoom = 1;
    Vector2 pinchLastMid{};
    bool pinching = false;
    float lastApply = 0;

    // Giọng nói của sinh vật (âm thanh tổng hợp) + trình thông dịch
    VoiceSystem voice;
    bool hearVoices = true;       // phát âm thanh lời nói
    long lastHeardTick = -1;      // câu cuối của sinh vật đang chọn đã phát
    Entity lastHeardEntity = NullEntity;
    double lastAmbientVoice = 0;
    long lastAmbientTick = -1;
};

void appInit(App& app);
void appStartWorld(App& app); // tạo thế giới theo app.scenario/app.worldSize
void appFrame(App& app);
void appShutdown(App& app);

} // namespace wb
