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
    // ---- Vật chất (vật lý & hoá học) ----
    PourWater, PourLava, PourAcid, PourOil,     // đổ chất lỏng — chảy theo địa hình, phản ứng thật
    Gunpowder,                                  // rắc thuốc súng (châm lửa → nổ)
    HeatUp, CoolDown,                           // đốt nóng / làm lạnh vùng (°C)
    Boulder,                                    // ném tảng đá (vật thể vật lý: rơi, nảy, lăn)
    OreVein,                                    // tạo mạch quặng
    Count
};

const char* powerName(Power p);
// Trả về true nếu là công cụ "vẽ" (giữ ngón tay kéo liên tục).
bool powerIsBrush(Power p);
void applyPower(Simulation& sim, Power p, int x, int y, int radius, int speciesId = 0);

} // namespace wb
