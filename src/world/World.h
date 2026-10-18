// =============================================================================
// World.h — Bản đồ thế giới dạng lưới ô (tile map) + môi trường.
//
// Mỗi ô chứa: địa hình, độ cao, tài nguyên (gỗ, đá, quặng, thức ăn thực vật),
// lửa, công trình và quốc gia/bộ tộc sở hữu. Thế giới tự vận hành: cây mọc lại,
// lửa lan, mưa dập lửa, mùa thay đổi nhiệt độ. Thần (người chơi) can thiệp
// thông qua GodPowers.
// =============================================================================
#pragma once
#include "Terrain.h"
#include "../core/Math.h"
#include "../phys/Material.h"
#include "../phys/MatterGrid.h"
#include <vector>
#include <functional>
#include <string>

namespace wb {

constexpr int kTicksPerYear = 600;
constexpr uint8_t kTileVolcanic = 1; // hồ dung nham núi lửa (nguồn nhiệt vĩnh viễn) // 1 năm trong game = 600 tick (≈30 giây ở tốc độ 1x)

struct Tile {
    Terrain terrain = Terrain::Grass;
    Building building = Building::None;
    uint8_t buildingHp = 0;
    uint8_t wood = 0;     // lượng gỗ (cây) 0..255
    uint8_t stone = 0;    // đá lộ thiên
    uint8_t ore = 0;      // trữ lượng quặng
    MatId oreMat = kNoMat;     // loại quặng (đồng, thiếc, sắt, vàng, lưu huỳnh, khoáng lạ…)
    MatId stoneMat = mat::Stone; // loại đá lộ thiên (đá, đá lửa, đá vôi, đất sét, đá vỏ chai…)
    uint8_t food = 0;     // quả mọng / cây ăn được
    uint8_t fire = 0;     // >0: đang cháy (cường độ, do MatterGrid cập nhật)
    uint8_t flags = 0;    // kTileVolcanic...
    uint8_t itemFlag = 0; // có vật phẩm trên ô (giúp quan sát nhanh, không phải duyệt danh sách)
    int16_t owner = -1;   // id nhóm (bộ tộc / quốc gia) sở hữu lãnh thổ, -1 = vô chủ
    float height = 0.5f;  // độ cao 0..1
    // ---- Lòng đất: mạch khoáng ẩn (không nhìn thấy từ mặt đất) ----
    // Chỉ biết được bằng hiểu biết địa chất (khái niệm Địa tầng), đào thăm dò, hoặc tình cờ khi đào hầm.
    MatId deepMat = kNoMat;  // loại khoáng sản: than đá, đồng, thiếc, sắt, bạc, vàng, đá quý, khoáng lạ…
    uint8_t deep = 0;        // trữ lượng còn lại
    uint8_t deepLevel = 0;   // độ sâu mạch: 1 nông · 2 vừa · 3 sâu (càng sâu càng cần dụng cụ đào tốt)
    uint8_t mineDepth = 0;   // độ sâu hầm đã đào được (0..deepLevel*kMineStep)
};
constexpr int kMineStep = 60;   // "khối lượng đào" để xuống thêm một tầng địa chất

struct GroundItem {
    int x, y;
    ItemKind kind;
    int amount;     // lượng thịt / số món
    int age = 0;    // tick tồn tại, mục rữa dần
    int techHint = -1; // công cụ cũ "chứa" kiến thức công nghệ nào (để học ngược)
};

// Thời tiết toàn cục đơn giản (mưa theo vùng + hạn hán).
struct Weather {
    float rainX = 0, rainY = 0, rainRadius = 0; // vùng mưa di chuyển
    float rainVX = 0, rainVY = 0;
    int rainTicks = 0;
    int droughtTicks = 0;
    float seasonTemp = 0; // -1 (đông) .. +1 (hè)
};

// Vật liệu nền của một loại địa hình (đất, đá, nước, cát…).
MatId groundMaterial(Terrain t);

class World {
public:
    void generate(int width, int height, uint32_t seed);

    int width() const { return w_; }
    int height() const { return h_; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    Tile& at(int x, int y) { return tiles_[y * w_ + x]; }
    const Tile& at(int x, int y) const { return tiles_[y * w_ + x]; }

    // Có đi bộ được qua ô không (địa hình + công trình + cầu trên nước).
    bool walkable(int x, int y) const;
    bool isWater(int x, int y) const;
    float moveCost(int x, int y) const;
    float temperatureAt(int x, int y) const; // °C thực tế (từ lớp vật chất)
    bool raining(int x, int y) const;

    // Tìm ô gần nhất thoả điều kiện trong bán kính r (quét theo vòng tròn đồng tâm).
    // Trả về true nếu tìm thấy, ghi toạ độ vào (ox, oy).
    bool findNearestTile(int cx, int cy, int r, const std::function<bool(const Tile&, int, int)>& pred,
                         int& ox, int& oy) const;

    // ---- Vật phẩm trên mặt đất ----
    std::vector<GroundItem>& items() { return items_; }
    const std::vector<GroundItem>& items() const { return items_; }
    void dropItem(int x, int y, ItemKind k, int amount, int techHint = -1);

    // ---- Chỉnh sửa (thần linh hoặc sinh vật) ----
    void setTerrain(int x, int y, Terrain t);
    void setBuilding(int x, int y, Building b);
    void ignite(int x, int y);
    void markDirty(int x, int y);

    // Mô phỏng môi trường một tick: cây mọc lại, lửa lan, thời tiết, mục rữa.
    void update(long tick, Rng& rng);

    Weather& weather() { return weather_; }
    const Weather& weather() const { return weather_; }

    // Danh sách ô vừa thay đổi (renderer dùng để cập nhật texture từng phần).
    const std::vector<int>& dirtyTiles() const { return dirty_; }
    void clearDirty();
    bool fullRedraw = true;

    // Tên độ sâu tầng địa chất.
    static const char* depthName(int level) {
        static const char* n[] = {"mặt đất", "tầng nông", "tầng giữa", "tầng sâu"};
        return n[std::clamp(level, 0, 3)];
    }
    int deepDeposits() const { return deepDeposits_; }

    // Thống kê ô đang cháy để AI quan sát "lửa".
    int burningCount() const { return matter.burningCount(); }

    // ---- Vật lý & hoá học ----
    MaterialRegistry materials;   // bảng vật liệu (tăng dần khi có hợp kim mới)
    MatterGrid matter;            // nhiệt độ, chất lỏng, khí, phản ứng

private:
    void classify(int x, int y, float h, float moist, float temp);
    int w_ = 0, h_ = 0;
    std::vector<Tile> tiles_;
    std::vector<GroundItem> items_;
    std::vector<uint8_t> dirtyFlag_;
    std::vector<int> dirty_;
    Weather weather_;
    int deepDeposits_ = 0;
};

} // namespace wb
