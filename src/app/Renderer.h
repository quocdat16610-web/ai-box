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
    Entity selected = NullEntity;
    int selectedGroup = -1;
};

// Góc nhìn quả địa cầu (khi thu nhỏ hết cỡ): kinh độ / vĩ độ điểm đối diện camera + khoảng cách.
struct GlobeView {
    float lon = 0, lat = 0.3f;   // radian
    float dist = 4.2f;            // bán kính hành tinh = 1
};

class Renderer {
public:
    void init(Simulation& sim);
    void shutdown();
    // Cập nhật texture từ các ô thay đổi (gọi mỗi khung hình).
    void sync(Simulation& sim);
    // Vẽ trong BeginMode2D: địa hình, lửa, mưa, sinh vật. Hành tinh quấn vòng đông–tây
    // nên texture được vẽ lặp hai bên và sinh vật được dời tới bản sao gần camera nhất.
    void drawWorld(Simulation& sim, const Camera2D& cam, const RenderOptions& opt, float time);
    // Vẽ trong toạ độ màn hình: nhãn nhóm/quốc gia, bong bóng suy nghĩ debug.
    void drawOverlay(Simulation& sim, const Camera2D& cam, const RenderOptions& opt, float uiScale);
    // Vẽ cả hành tinh dạng quả cầu 3D (texture địa hình dán lên mặt cầu, cập nhật trực tiếp).
    void drawGlobe(Simulation& sim, const GlobeView& gv, const RenderOptions& opt, float time, float uiScale);
    // Texture địa hình (dùng chung cho quả địa cầu và góc nhìn thứ nhất)
    const Texture2D& terrainTexture() const { return tex_; }
    const Texture2D& undergroundTexture(Simulation& sim) { syncUnder(sim); return underTex_; }

private:
    void paintTile(Simulation& sim, int x, int y);
    void paintUnder(Simulation& sim, int x, int y);
    void syncUnder(Simulation& sim);
    void drawCreatures(Simulation& sim, const Camera2D& cam, const RenderOptions& opt, float time, bool underground);
    void buildGlobeMesh();
    Image underImage_{};
    Texture2D underTex_{};
    bool underReady_ = false;
    int underVersion_ = -1;
    double underSyncTime_ = -10;
    Image image_{};
    Texture2D tex_{};
    Model globe_{};
    bool globeReady_ = false;
    int w_ = 0, h_ = 0;
    bool ready_ = false;
};

// Toạ độ x (đơn vị thế giới camera) của bản sao gần camera nhất trên hành tinh quấn vòng
float nearestCopyX(float px, float camX, float worldW);
// Điểm (x, y) ô → vector 3D trên quả cầu bán kính 1 (bắc ở trên)
Vector3 globePoint(float tileX, float tileY, int w, int h);
// Camera 3D nhìn quả địa cầu từ phía (kinh độ, vĩ độ) của GlobeView
Camera3D globeCamera(const GlobeView& gv);

Color speciesColor(const Simulation& sim, int species);
Color groupColor(const Simulation& sim, int gid);

} // namespace wb
