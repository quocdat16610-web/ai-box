// =============================================================================
// GodPowers.h — Quyền năng của Thần (người chơi), giống thanh công cụ WorldBox.
// =============================================================================
#pragma once
#include <cstdint>

namespace wb {

class Simulation;

enum class Power : uint8_t {
    Inspect,                                    // chạm để xem suy nghĩ sinh vật
    RaiseLand, LowerLand,                       // nâng / hạ địa hình
    PaintWater, PaintGrass, PaintForest, PaintSand, PaintMountain, PaintSnow, PaintLava,
    Fire, Meteor, Lightning, Rain, Drought, Earthquake, Plague,
    Blessing,                                   // ban phước: tăng trí tuệ + sức khoẻ
    Smite,                                      // trừng phạt: giết sinh vật trong vùng
    Spawn,                                      // thả sinh vật (loài chọn riêng)
    Count
};

const char* powerName(Power p);
// Trả về true nếu là công cụ "vẽ" (giữ ngón tay kéo liên tục).
bool powerIsBrush(Power p);
void applyPower(Simulation& sim, Power p, int x, int y, int radius, int speciesId = 0);

} // namespace wb
