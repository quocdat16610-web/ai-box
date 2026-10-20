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
#include "FirstPerson.h"
#include "../sim/GodPowers.h"
#include "../sim/Scenarios.h"
#include "../sim/Simulation.h"
#include "../sim/SaveLoad.h"
#include "../ai/Invention.h"
#include "Net.h"
#include <memory>
#include <vector>

namespace wb {

enum class ToolCategory : uint8_t { Inspect, Terrain, Matter, Life, Disaster, Weather, Divine, Count };
enum class SidePanel : uint8_t { None, Creature, Group, Chronicle, Species, Chemistry };

// Phòng thí nghiệm gen: ghép gen giữa các loài, chỉnh từng gen, tạo gen mới, chỉnh ngoại hình
// → tạo một LOÀI MỚI thả vào thế giới (loài này vẫn tiếp tục tự tiến hoá về sau).
struct GeneLab {
    bool open = false;
    int tab = 0;                 // 0 cấy ghép, 1 bộ gen, 2 ngoại hình, 3 gen mới
    int base = 0, donor = 4;     // loài gốc / loài cho gen
    SpeciesDef draft;            // bản nháp loài mới
    bool hasDraft = false;
    int newEffect = 0;           // gen mới: hiệu ứng
    float newStrength = 0.5f;
    std::string newGeneName;
    int focus = 0;               // ô nhập chữ đang chọn: 1 tên loài, 2 tên gen
    std::string message;
    double messageTime = -10;
};

// Bảng chế tạo & xây dựng của người chơi (khi điều khiển một sinh vật)
struct CraftUI {
    bool open = false;
    int tab = 0;                 // 0 chế tạo, 1 xây dựng
    int purpose = 0;             // mục đích đang thiết kế (Purpose)
    std::vector<Part> parts;     // bản thiết kế đang ghép
    bool started = false;        // đã điền gợi ý ban đầu chưa
    std::string msg;
    double msgTime = -10;
};

struct App {
    std::unique_ptr<Simulation> sim;
    Renderer renderer;
    Camera2D cam{};
    RenderOptions opt;
    bool inMenu = true;
    Scenario scenario = Scenario::Sandbox;
    int worldSize = 128;        // số hàng vĩ độ; số cột kinh độ = 2 × (bản đồ trụ của hành tinh)
    WorldType worldType = WorldType::Random;   // kiểu hành tinh chọn ở menu

    // Quả địa cầu: thu nhỏ bản đồ hết cỡ → chuyển sang xem hành tinh hình cầu 3D
    bool globe = false;
    GlobeView globeView;
    FirstPerson fp;            // góc nhìn thứ nhất / điều khiển sinh vật
    GeneLab lab;
    // Thế giới đang sống trên GitHub (Actions chạy liên tục, lưu ở Release "live-world")
    Download liveDl;
    bool liveKnown = false, liveAutoLoad = false;
    SaveInfo liveInfo;
    CraftUI craft;             // bảng chế tạo / xây dựng của người chơi

    // Lưu / tải thế giới: ô 0 = tự động lưu, ô 1..3 = người chơi chọn
    static constexpr int kSlots = 4;
    bool savePage = false;                 // đang mở màn hình Lưu / Tải
    bool slotKnown[kSlots] = {};
    SaveInfo slotInfo[kSlots];
    bool slotsScanned = false;
    std::string toast;                     // thông báo ngắn ("Đã lưu vào ô 1")
    double toastTime = -10;
    double lastAutosave = 0;
    int lastW = 0, lastH = 0;   // kích thước màn hình khung trước (phát hiện xoay điện thoại)

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
};

void appInit(App& app);
void appStartWorld(App& app); // tạo thế giới theo app.scenario/app.worldSize
void appFrame(App& app);
void appShutdown(App& app);
bool appLoadSlot(App& app, int slot);   // tải thế giới đã lưu (0 = tự động lưu)
void appFetchLiveWorld(App& app);         // tải thế giới đang sống từ GitHub rồi mở (chạy nền)

} // namespace wb
