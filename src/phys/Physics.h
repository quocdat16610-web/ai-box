// =============================================================================
// Physics.h — Vật lý vật thể rắn 2.5D (x, y trên bản đồ + z độ cao).
//
//   * Trọng lực g = 9.8 m/s² theo trục z → mũi tên, đá ném, đạn máy bắn đá bay
//     theo quỹ đạo parabol thật. Sinh vật bắn cung phải TỰ GIẢI phương trình
//     đạn đạo (góc bắn) — xem `ballisticAngle`.
//   * Lực cản không khí, gió đẩy vật nhẹ.
//   * Va chạm: mặt đất (nảy theo hệ số đàn hồi của vật liệu, ma sát), công trình
//     (phá huỷ theo động năng so với độ cứng), sinh vật (sát thương = động năng
//     × độ sắc), vật thể với vật thể (bảo toàn động lượng).
//   * Nổi / chìm: vật rơi xuống nước nổi nếu khối lượng riêng < nước.
//   * Nổ: xung lực hướng tâm ra ngoài, giảm theo bình phương khoảng cách → đẩy
//     văng vật thể và sinh vật.
// Đơn vị: 1 ô = 1 m, 1 tick = 0.05 s.
// =============================================================================
#pragma once
#include "../core/Math.h"
#include "../core/Ecs.h"
#include "Material.h"
#include <vector>

namespace wb {

class Simulation;

constexpr float kGravity = 9.8f;
constexpr float kPhysDt = 0.05f;

enum class BodyKind : uint8_t { Projectile, Boulder, Debris, Item };

struct PhysBody {
    Vec2 pos;          // vị trí trên mặt bản đồ (m)
    float z = 0;       // độ cao so với mặt đất (m)
    Vec2 vel;          // vận tốc ngang (m/s)
    float vz = 0;      // vận tốc thẳng đứng (m/s)
    float mass = 1;    // kg
    float radius = 0.2f;
    MatId mat = 0;
    float sharpness = 1;   // hệ số xuyên thủng khi trúng sinh vật
    float temp = 20;       // vật nóng (mảnh thiên thạch) đốt nóng nơi rơi xuống
    BodyKind kind = BodyKind::Projectile;
    Entity owner = NullEntity;
    int ownerGroup = -1;
    int design = -1;       // nếu là vật phẩm/đạn chế tạo
    int life = 400;        // tick tối đa
    bool alive = true;
    bool floating = false;
};

// Một "sự kiện vật lý" để debug & để sinh vật quan sát học khái niệm
// (thấy vật lăn → bánh xe, thấy vật nổi → sức nổi, thấy nổ → áp suất...).
enum class PhysEventKind : uint8_t { Impact, Bounce, Float, Sink, Roll, Blast, Count };
struct PhysEvent { long tick; PhysEventKind kind; Vec2 pos; MatId mat; float energy; };

class PhysicsWorld {
public:
    std::vector<PhysBody> bodies;
    std::vector<PhysEvent> events;   // sự kiện của tick hiện tại

    void clear() { bodies.clear(); events.clear(); }
    int spawn(const PhysBody& b);
    void step(Simulation& sim);
    // Áp xung lực nổ tại (x,y) với năng lượng E (J tương đối).
    void blast(Simulation& sim, Vec2 c, float energy);
};

// Góc bắn (radian) để vật bay vận tốc v tới khoảng cách d trên mặt phẳng.
// Trả về false nếu ngoài tầm (d > v²/g). Đây là phương trình sinh vật "biết".
bool ballisticAngle(float v, float d, float& angle);

} // namespace wb
