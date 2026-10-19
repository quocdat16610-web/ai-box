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
constexpr uint8_t kTileVolcanic = 1;     // hồ dung nham núi lửa (nguồn nhiệt vĩnh viễn)
constexpr uint8_t kTileCaveEntrance = 2; // cửa hang tự nhiên: mặt đất thông xuống hang động
// (1 năm trong game = 600 tick ≈ 30 giây ở tốc độ 1x)

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
};

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

// Kiểu hành tinh khi sinh thế giới. "Ngẫu nhiên" tự chọn một kiểu + tham số ngẫu nhiên,
// nên có lần ra siêu lục địa, có lần ra nhiều châu lục, có lần chỉ toàn đảo hoặc
// không có châu lục nào (hành tinh đại dương / hành tinh khô toàn đất liền).
enum class WorldType : uint8_t { Random, Pangaea, Continents, Archipelago, OceanWorld, DryWorld, Count };
const char* worldTypeName(WorldType t);

class World {
public:
    // Thế giới là một HÀNH TINH HÌNH CẦU trải phẳng theo phép chiếu trụ đẳng chữ nhật:
    // x = kinh độ (quấn vòng đông–tây), y = vĩ độ (mép trên = cực bắc, mép dưới = cực nam).
    void generate(int width, int height, uint32_t seed, WorldType type = WorldType::Random);

    int width() const { return w_; }
    int height() const { return h_; }
    // Mọi x đều hợp lệ (quấn vòng), chỉ y bị giới hạn bởi hai cực.
    bool inBounds(int x, int y) const { (void)x; return y >= 0 && y < h_; }
    int wrapX(int x) const { return x < 0 ? (x % w_ + w_) % w_ : (x >= w_ ? x % w_ : x); }
    Tile& at(int x, int y) { return tiles_[y * w_ + wrapX(x)]; }
    const Tile& at(int x, int y) const { return tiles_[y * w_ + wrapX(x)]; }
    WorldType type() const { return type_; }     // kiểu hành tinh thực tế (sau khi bốc ngẫu nhiên)
    int continents() const { return continents_; } // số "lõi lục địa" đã sinh (0 = không có)

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

    // Thống kê ô đang cháy để AI quan sát "lửa".
    int burningCount() const { return matter.burningCount(); }

    // ---- Lòng đất: hang động, hồ ngầm, mạch quặng, đường hầm mỏ ----
    UnderTile& under(int x, int y) { return under_[y * w_ + wrapX(x)]; }
    const UnderTile& under(int x, int y) const { return under_[y * w_ + wrapX(x)]; }
    // Ô ngầm có không khí để sinh vật hang (dơi, nhện) di chuyển
    bool caveOpen(int x, int y) const {
        if (!inBounds(x, y)) return false;
        Under k = under(x, y).kind;
        return k == Under::Cave || k == Under::Tunnel;
    }
    // Sinh vật đào hầm mỏ tại (x, y): biến đá thành đường hầm và đào tiếp theo mạch quặng ngầm
    // gần nhất. Trả về loại & lượng quặng khai thác thêm được từ lòng đất.
    int digMine(int x, int y, Rng& rng, MatId& oreOut);
    int underVersion = 0;   // tăng mỗi khi lòng đất thay đổi (renderer vẽ lại lớp "Lòng đất")
    int caveTiles = 0, veinTiles = 0;

    // ---- Vật lý & hoá học ----
    MaterialRegistry materials;   // bảng vật liệu (tăng dần khi có hợp kim mới)
    MatterGrid matter;            // nhiệt độ, chất lỏng, khí, phản ứng

private:
    void classify(int x, int y, float h, float moist, float temp);
    void generateUnderground(uint32_t seed, Rng& rng);
    int w_ = 0, h_ = 0;
    WorldType type_ = WorldType::Continents;
    int continents_ = 0;
    std::vector<Tile> tiles_;
    std::vector<UnderTile> under_;
    std::vector<GroundItem> items_;
    std::vector<uint8_t> dirtyFlag_;
    std::vector<int> dirty_;
    Weather weather_;
};

} // namespace wb
