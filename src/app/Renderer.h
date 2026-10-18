// =============================================================================
// Renderer.h — Vẽ thế giới pixel art 2D top-down bằng raylib.
//
// Kỹ thuật:
//   * Toàn bộ địa hình + công trình + lãnh thổ được "nướng" vào MỘT texture
//     (mỗi ô = 4×4 pixel). Chỉ những ô bẩn (dirty) mới được vẽ lại → rất rẻ,
//     phù hợp GPU điện thoại. Lọc NEAREST giữ nét pixel art khi phóng to.
//   * Sinh vật, lửa, mưa, nhãn, debug suy nghĩ vẽ mỗi khung hình, có cắt xén
//     (culling) theo khung nhìn camera.
// =============================================================================
#pragma once
#include "raylib.h"
#include "../core/Ecs.h"
#include <vector>

namespace wb {

class Simulation;

constexpr int kTilePx = 4;        // pixel trên texture cho mỗi ô
constexpr float kTileWorld = 8.f; // đơn vị thế giới (camera) cho mỗi ô

// Lớp hiển thị debug vật lý/hoá học
enum class Layer : uint8_t { Normal, Heat, Gas, Reactions, Underground, Count };
inline const char* layerName(Layer l) {
    static const char* n[] = {"Thường", "Nhiệt độ", "Khí & áp suất", "Phản ứng", "Lòng đất"};
    return n[static_cast<int>(l)];
}

struct RenderOptions {
    bool debugThoughts = false;   // hiện "suy nghĩ" của mọi sinh vật trên màn hình
    Layer layer = Layer::Normal;  // lớp debug vật lý/hoá học
    bool showTerritory = true;
    bool showLabels = true;
    bool translate = true;        // trình thông dịch: hiện nghĩa tiếng Việt dưới lời nói
    Entity selected = NullEntity;
    int selectedGroup = -1;
};

class Renderer {
public:
    void init(Simulation& sim);
    void shutdown();
    // Cập nhật texture từ các ô thay đổi (gọi mỗi khung hình).
    void sync(Simulation& sim);
    // Vẽ trong BeginMode2D: địa hình, lửa, mưa, sinh vật.
    void drawWorld(Simulation& sim, const Camera2D& cam, const RenderOptions& opt, float time);
    // Vẽ trong toạ độ màn hình: nhãn nhóm/quốc gia, bong bóng suy nghĩ debug.
    void drawOverlay(Simulation& sim, const Camera2D& cam, const RenderOptions& opt, float uiScale);

private:
    void paintTile(Simulation& sim, int x, int y);
    Image image_{};
    Texture2D tex_{};
    int w_ = 0, h_ = 0;
    bool ready_ = false;
};

Color speciesColor(const Simulation& sim, int species);
Color groupColor(const Simulation& sim, int gid);

} // namespace wb
