// =============================================================================
// FirstPerson.h — Góc nhìn thứ nhất: nhìn thế giới qua mắt một sinh vật bất kỳ,
// và (tuỳ chọn) điều khiển nó.
//
// Cách dựng cảnh 3D (nhẹ, chạy được trên điện thoại):
//   * Địa hình quanh sinh vật (bán kính ~28 ô) → MỘT lưới tam giác: mỗi ô là một tứ
//     giác, độ cao 4 góc lấy trung bình độ cao các ô lân cận, toạ độ texture lấy thẳng
//     từ texture pixel art của bản đồ 2D (nên nhà cửa, ruộng, lãnh thổ hiện đúng chỗ).
//     Lưới chỉ dựng lại khi sinh vật sang ô khác (hoặc mỗi 2 giây để thấy thế giới đổi).
//   * Cây, công trình, lửa, sinh vật khác, mũi tên/đá bay: các khối hộp đơn giản vẽ mỗi khung.
//   * Ba "thế giới" theo môi trường sống: mặt đất, dưới nước (cá: đáy biển + màu nước),
//     trong hang (dơi, nhện: vách đá, nấm phát quang, ánh sáng mờ dần theo khoảng cách).
// Hệ toạ độ cục bộ: gốc ở góc ô (cx, cy); trục x = kinh độ, trục z = vĩ độ, trục y = độ cao.
// =============================================================================
#pragma once
#include "raylib.h"
#include "../core/Ecs.h"
#include "../core/Math.h"
#include <climits>
#include <string>
#include <unordered_map>

namespace wb {

class Simulation;
class Renderer;

// Trạng thái hiển thị mượt của một sinh vật khác
struct SmoothState {
    Vec2 pos;          // vị trí hiển thị (đuổi theo vị trí mô phỏng)
    float yaw = 0;     // hướng mặt hiển thị
    float phase = 0;   // pha bước chân (tăng theo quãng đường đi được → chân không "trượt")
    long seen = 0;     // khung hình gần nhất được cập nhật (dọn rác)
};

struct FirstPerson {
    bool active = false;      // đang ở góc nhìn thứ nhất
    bool control = false;     // người chơi điều khiển (AI của sinh vật tạm nghỉ)
    Entity who = NullEntity;
    float yaw = 0, pitch = -0.12f;   // hướng nhìn (radian); yaw = 0 → hướng đông (+x)
    float bobT = 0;                  // nhịp bước (camera nhấp nhô khi đi)
    std::string msg;                 // thông báo kết quả hành động gần nhất
    float msgTime = 0;
    // Cần điều khiển ảo (màn hình cảm ứng)
    bool stick = false;
    Vector2 stickOrigin{}, stickPos{};
    bool looking = false;
    Vector2 lookLast{};
    // Hoạt ảnh của chính mình (tay cầm công cụ / mõm, chân trước) khi hành động
    float actAnim = 0;               // 1 → 0: đang vung tay / lao tới
    int actKind = 0;                 // 0 tấn công, 1 ăn, 2 thu thập, 3 dựng lều
    // Nội suy vị trí cho chuyển động mượt (mô phỏng chạy 20 tick/giây, màn hình 60 khung/giây)
    std::unordered_map<Entity, SmoothState> smooth;
    Vec2 camPos{};
    bool camInit = false;
    float walkPhase = 0;
    // Shader ánh sáng + sương mù (GLSL 330 trên PC, GLSL 100 trên điện thoại)
    Shader shader{};
    bool shaderReady = false, shaderTried = false;
    int locSun = -1, locFog = -1, locFogRange = -1, locLight = -1, locAmbient = -1;
    // Lưới địa hình đã dựng
    Model model{};
    bool meshReady = false;
    int cx = INT_MIN, cy = INT_MIN;
    int mode = -1;                   // 0 mặt đất, 1 dưới nước, 2 trong hang
    double builtAt = -10;
};

void fpEnter(FirstPerson& fp, Simulation& sim, Entity e);
void fpExit(FirstPerson& fp, Simulation& sim);
void fpRelease(FirstPerson& fp);     // giải phóng GPU (khi đổi thế giới / thoát game)
// Vector hướng nhìn trên mặt phẳng bản đồ (đơn vị ô)
Vec2 fpFacing(const FirstPerson& fp);
// Vẽ toàn bộ cảnh 3D (gọi giữa BeginDrawing/EndDrawing, trước HUD)
void fpRender(FirstPerson& fp, Simulation& sim, Renderer& r, float time);

} // namespace wb
