// Knowledge.cpp — Bảng định nghĩa cây công nghệ (dữ liệu thuần, dễ cân bằng).
#include "Knowledge.h"

namespace wb {

namespace {
constexpr Tech N = Tech::Count; // "không có tiền đề"
// Thứ tự phải khớp enum Tech.
const TechDef kTechs[kTechCount] = {
    //  tên                 trí tuệ  kích thích             exp  tiền đề                         n  động vật
    {"Săn theo bầy",        0.15f, Stimulus::GroupHunt,      6, {N, N},                          0, true},
    {"Đánh dấu lãnh thổ",   0.18f, Stimulus::TerritoryClash, 3, {N, N},                          0, true},
    {"Thứ bậc xã hội",      0.22f, Stimulus::GroupLife,     30, {N, N},                          0, true},
    {"Dùng lửa",            0.35f, Stimulus::SeeFire,        3, {N, N},                          0, false},
    {"Công cụ đá",          0.33f, Stimulus::HandleStone,   10, {N, N},                          0, false},
    {"Nơi trú ẩn",          0.28f, Stimulus::ColdOrRain,    25, {N, N},                          0, true},
    {"Ngôn ngữ",            0.42f, Stimulus::Communicate,   40, {Tech::Hierarchy, N},            1, true},
    {"Vũ khí",              0.40f, Stimulus::Combat,         5, {Tech::StoneTools, N},           1, false},
    {"Chế biến gỗ",         0.42f, Stimulus::ChopWood,      12, {Tech::StoneTools, Tech::Shelter}, 2, false},
    {"Trồng trọt",          0.48f, Stimulus::EatPlants,     40, {Tech::StoneTools, Tech::Language}, 2, false},
    {"Khai khoáng",         0.47f, Stimulus::SeeOre,         8, {Tech::StoneTools, N},           1, false},
    {"Xây đá",              0.50f, Stimulus::BuildStone,    10, {Tech::WoodWorking, Tech::Mining}, 2, false},
    {"Luyện kim",           0.56f, Stimulus::SeeFire,        6, {Tech::Mining, Tech::FireUse},   2, false},
    {"Tôn giáo",            0.45f, Stimulus::SeeDeath,       6, {Tech::Language, N},             1, true},
    {"Chữ viết",            0.60f, Stimulus::Communicate,   80, {Tech::Language, Tech::Farming}, 2, false},
    {"Luật pháp",           0.55f, Stimulus::Settled,      300, {Tech::Language, Tech::Hierarchy}, 2, true},
    {"Xây cầu",             0.50f, Stimulus::SeeWater,      10, {Tech::WoodWorking, N},          1, false},
};
} // namespace

const TechDef& techDef(Tech t) { return kTechs[static_cast<int>(t)]; }

bool techPrereqsMet(Tech t, const TechSet& known) {
    const TechDef& d = techDef(t);
    for (int i = 0; i < d.prereqCount; ++i)
        if (!known.test(static_cast<int>(d.prereq[i]))) return false;
    return true;
}

} // namespace wb
