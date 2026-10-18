// =============================================================================
// Knowledge.h — TẬP QUÁN VĂN HOÁ / XÃ HỘI (meme) có thể tự phát sinh.
//
// LƯU Ý THIẾT KẾ: đây KHÔNG phải cây công nghệ. Công nghệ vật chất (công cụ, vũ
// khí, máy móc, luyện kim, hoá chất…) là hệ thống MỞ, không giới hạn — xem
// ai/Invention.h (khái niệm vật lý, thiết kế, quy trình hoá học).
//
// Ở đây chỉ còn các hành vi xã hội rời rạc mà một nhóm "có" hoặc "chưa có":
// săn theo bầy, thứ bậc, ngôn ngữ, tôn giáo, luật pháp... Mỗi tập quán có:
//   * ngưỡng trí tuệ tối thiểu, tiền đề (tập quán khác)
//   * "kích thích" phải trải nghiệm đủ nhiều lần
// Khi đủ điều kiện, cá thể có xác suất nảy ra ý tưởng (tỷ lệ trí tuệ × tò mò),
// rồi lan truyền qua dạy học, thừa hưởng, giao lưu văn hoá.
// =============================================================================
#pragma once
#include <cstdint>
#include <bitset>

namespace wb {

enum class Tech : uint8_t {
    PackHunting,   // săn theo bầy (động vật cũng học được)
    ScentMarking,  // đánh dấu lãnh thổ
    Hierarchy,     // thứ bậc xã hội / con đầu đàn
    Shelter,       // hang ổ / lều tạm
    Language,      // ngôn ngữ (gọi nhau, truyền tri thức nhanh hơn)
    Farming,       // trồng trọt
    Religion,      // tôn giáo / tín ngưỡng
    Writing,       // chữ viết → lưu trữ tri thức (kể cả thiết kế & quy trình)
    Law,           // luật pháp / thể chế nhà nước
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
