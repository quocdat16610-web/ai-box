// Knowledge.cpp — Bảng định nghĩa cây công nghệ (dữ liệu thuần, dễ cân bằng).
#include "Knowledge.h"

namespace wb {

namespace {
constexpr Tech N = Tech::Count; // "không có tiền đề"
// Thứ tự phải khớp enum Tech.
const TechDef kTechs[kTechCount] = {
    //  tên                 trí tuệ  kích thích             exp  tiền đề                           n  động vật
    {"Săn theo bầy",        0.15f, Stimulus::GroupHunt,      6, {N, N},                            0, true},
    {"Đánh dấu lãnh thổ",   0.18f, Stimulus::TerritoryClash, 3, {N, N},                            0, true},
    {"Thứ bậc xã hội",      0.22f, Stimulus::GroupLife,     30, {N, N},                            0, true},
    {"Nơi trú ẩn",          0.28f, Stimulus::ColdOrRain,    25, {N, N},                            0, true},
    {"Ngôn ngữ",            0.42f, Stimulus::Communicate,   40, {Tech::Hierarchy, N},              1, true},
    {"Trồng trọt",          0.46f, Stimulus::EatPlants,     40, {Tech::Language, N},               1, false},
    {"Tôn giáo",            0.45f, Stimulus::SeeDeath,       6, {Tech::Language, N},               1, true},
    {"Chữ viết",            0.60f, Stimulus::Communicate,   80, {Tech::Language, Tech::Farming},   2, false},
    {"Luật pháp",           0.55f, Stimulus::Settled,      300, {Tech::Language, Tech::Hierarchy}, 2, true},
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
