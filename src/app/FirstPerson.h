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

namespace wb {

class Simulation;
class Renderer;

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
