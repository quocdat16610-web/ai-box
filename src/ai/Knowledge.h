// =============================================================================
// Knowledge.h — Cây công nghệ / tri thức có thể "tự khám phá".
//
// Không có ai "dạy sẵn" công nghệ. Mỗi Tech có:
//   * điều kiện tiên quyết (prereq)  — phải biết trước tech khác
//   * ngưỡng trí tuệ tối thiểu        — sinh vật đủ thông minh mới hiểu được
//   * "kích thích" (stimulus)        — thứ sinh vật phải QUAN SÁT hoặc TRẢI NGHIỆM
//                                      (thấy lửa, cầm đá + gỗ, săn chung với bầy...)
// Khi cá thể tích luỹ đủ "exposure" (số lần tiếp xúc kích thích) thì có xác suất
// phát minh ra tech, tỷ lệ với trí tuệ và tính tò mò. Tri thức lan truyền qua
// dạy học trong nhóm và truyền cho con cái (di truyền văn hoá).
// =============================================================================
#pragma once
#include <cstdint>
#include <bitset>

namespace wb {

enum class Tech : uint8_t {
    PackHunting,   // săn theo bầy (động vật cũng học được)
    ScentMarking,  // đánh dấu lãnh thổ
    Hierarchy,     // thứ bậc xã hội / con đầu đàn
    FireUse,       // sử dụng lửa (nấu chín, sưởi ấm)
    StoneTools,    // công cụ đá sơ khai
    Shelter,       // hang ổ / lều tạm
    Language,      // ngôn ngữ (gọi nhau, truyền tri thức nhanh hơn)
    Weapons,       // vũ khí (giáo, cung)
    WoodWorking,   // chế biến gỗ → nhà gỗ
    Farming,       // trồng trọt
    Mining,        // khai thác quặng
    Masonry,       // xây đá → tường, nhà đá
    Metallurgy,    // luyện kim → công cụ/vũ khí kim loại
    Religion,      // tôn giáo / tín ngưỡng
    Writing,       // chữ viết → lưu trữ tri thức
    Law,           // luật pháp / thể chế nhà nước
    Bridges,       // xây cầu qua nước
    Count
};
constexpr int kTechCount = static_cast<int>(Tech::Count);
using TechSet = std::bitset<kTechCount>;

// Loại kích thích (thứ cần quan sát/trải nghiệm để nảy sinh ý tưởng).
enum class Stimulus : uint8_t {
    GroupHunt,      // săn mồi cùng đồng loại
    TerritoryClash, // chạm trán nhóm khác cùng loài
    GroupLife,      // sống trong nhóm đông
    SeeFire,        // nhìn thấy đám cháy
    HandleStone,    // cầm nắm đá + gỗ
    ColdOrRain,     // chịu lạnh / mưa
    Communicate,    // giao tiếp với đồng loại
    Combat,         // chiến đấu
    ChopWood,       // đốn cây
    EatPlants,      // hái lượm cây trái lâu dài
    SeeOre,         // thấy quặng kim loại
    BuildStone,     // xây bằng đá
    SeeDeath,       // chứng kiến cái chết / thiên tai
    Settled,        // sống cố định lâu ở một nơi
    SeeWater,       // bị nước chặn đường
    FindOldTool,    // nhặt được công cụ cũ của nền văn minh khác
    Count
};
constexpr int kStimulusCount = static_cast<int>(Stimulus::Count);

struct TechDef {
    const char* name;        // tên tiếng Việt
    float minIntellect;      // ngưỡng trí tuệ
    Stimulus stimulus;       // kích thích chính
    int exposureNeeded;      // số lần tiếp xúc tối thiểu
    Tech prereq[2];          // tối đa 2 tiền đề
    int prereqCount;
    bool animalsCan;         // động vật (chưa có tay khéo) có học được không
};

const TechDef& techDef(Tech t);
inline const TechDef& techDef(int t) { return techDef(static_cast<Tech>(t)); }
inline int ti(Tech t) { return static_cast<int>(t); }

// Kiểm tra tiền đề đã đủ chưa.
bool techPrereqsMet(Tech t, const TechSet& known);

} // namespace wb
