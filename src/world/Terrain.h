// =============================================================================
// Terrain.h — Các loại địa hình, công trình và vật phẩm trên bản đồ.
// Toàn bộ là bảng dữ liệu (data-driven) để dễ thêm địa hình mới.
// =============================================================================
#pragma once
#include <cstdint>

namespace wb {

enum class Terrain : uint8_t {
    DeepWater, ShallowWater, Sand, Grass, Forest, Jungle, Desert, Savanna,
    Hill, Mountain, Snow, Tundra, Swamp, Lava, Ash, Farmland, Road,
    Ice, Basalt, Glass,   // sinh ra từ vật lý: nước đóng băng, dung nham nguội, cát nung chảy
    Count
};

struct TerrainInfo {
    const char* name;     // tên tiếng Việt hiển thị
    bool walkable;        // sinh vật trên cạn có đi được không
    float moveCost;       // hệ số chậm khi di chuyển (1 = bình thường)
    float fertility;      // khả năng mọc cây/quả (0..1)
    float temperature;    // nhiệt độ tương đối (-1 lạnh .. +1 nóng)
    uint8_t r, g, b;      // màu pixel art cơ bản
};

inline const TerrainInfo& terrainInfo(Terrain t) {
    static const TerrainInfo table[] = {
        {"Biển sâu",     false, 9.f, 0.0f,  0.0f,  32,  64, 140},
        {"Nước nông",    false, 9.f, 0.0f,  0.0f,  58, 110, 190},
        {"Bãi cát",      true,  1.2f, 0.1f, 0.3f, 222, 206, 140},
        {"Đồng cỏ",      true,  1.0f, 0.6f, 0.1f,  98, 168,  62},
        {"Rừng",         true,  1.5f, 0.9f, 0.0f,  50, 120,  45},
        {"Rừng rậm",     true,  1.9f, 1.0f, 0.6f,  30, 100,  40},
        {"Sa mạc",       true,  1.3f, 0.02f, 0.9f, 230, 190, 110},
        {"Thảo nguyên",  true,  1.0f, 0.4f, 0.6f, 180, 175,  80},
        {"Đồi",          true,  1.8f, 0.3f, -0.2f, 125, 130,  80},
        {"Núi",          false, 9.f, 0.0f, -0.5f, 120, 110, 105},
        {"Tuyết",        true,  2.0f, 0.0f, -1.0f, 235, 240, 250},
        {"Lãnh nguyên",  true,  1.4f, 0.15f, -0.8f, 160, 175, 165},
        {"Đầm lầy",      true,  2.2f, 0.7f, 0.3f,  70, 100,  70},
        {"Dung nham",    false, 9.f, 0.0f,  1.0f, 230,  70,  20},
        {"Tro tàn",      true,  1.2f, 0.05f, 0.2f,  80,  75,  72},
        {"Ruộng",        true,  1.1f, 1.0f,  0.1f, 150, 120,  60},
        {"Đường",        true,  0.6f, 0.0f,  0.1f, 165, 140, 100},
        {"Băng",         true,  1.6f, 0.0f, -1.0f, 200, 230, 245},
        {"Đá bazan",     true,  1.3f, 0.05f, 0.2f,  70,  70,  78},
        {"Thuỷ tinh",    true,  1.1f, 0.0f,  0.3f, 170, 220, 225},
    };
    return table[static_cast<int>(t)];
}

// Công trình do sinh vật xây (một ô một công trình).
enum class Building : uint8_t {
    None, Den, Nest, Hut, House, StoneHouse, Wall, Tower, Bridge,
    Granary, Workshop, Forge, Temple, TownHall, Castle, ScentMark, Totem,
    Hearth,   // bếp lửa: nguồn nhiệt ~700°C (nấu ăn, sưởi, nung thử nghiệm)
    Count
};

struct BuildingInfo {
    const char* name;
    int wood, stone, metal;  // chi phí vật liệu (metal = bất kỳ kim loại nào trong kho)
    int requiredTech;        // tập quán văn hoá cần biết (-1 = không cần)
    int housing;             // số dân có thể ở
    bool blocksMove;
    float minCut;            // năng lực cắt tối thiểu của công cụ (gia công gỗ/đá) — quy luật vật lý thay cho tech tree
    bool flammable;          // làm bằng vật liệu cháy được (gỗ) → lửa thiêu rụi được
};

const BuildingInfo& buildingInfo(Building b); // định nghĩa trong World.cpp (cần Tech ids)

// Vật phẩm nằm trên mặt đất: xác chết, công cụ bị bỏ lại, đá rơi...
// Đây là "tài liệu học tập" cho các sinh vật tò mò quan sát.
enum class ItemKind : uint8_t { Corpse, Bones, OldTool, OldWeapon, Meat, Rock, Count };

} // namespace wb
