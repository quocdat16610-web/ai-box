// =============================================================================
// Sprites.h — "Mô hình" pixel art thủ tục cho sinh vật và dụng cụ.
//
// Mỗi chủng tộc có một hình dáng riêng (người, elf tai nhọn, orc nanh, dwarf râu
// + mũ sắt, yêu tinh tai to, quỷ khổng lồ, sói, hươu có gạc, thỏ, gấu; loài thú
// tiến hoá ra tay khéo dùng dáng "thú nhân"). Áo mang màu nhóm/quốc gia.
// Dụng cụ cầm tay vẽ theo ĐÚNG thiết kế đang mang: hình dạng theo mục đích
// (rìu, cuốc, giáo, chuỳ, cung, mồi lửa, áo choàng, giỏ), màu theo VẬT LIỆU chính
// → nhìn là biết thời đại: rìu đá xám, rìu đồng cam, rìu sắt xanh thép...
// Không dùng file ảnh nên APK vẫn nhỏ gọn.
// =============================================================================
#pragma once
#include "raylib.h"
#include "../core/Ecs.h"

namespace wb {

class Simulation;

// Vẽ một sinh vật (trong BeginMode2D). (px, py) = vị trí chân, đơn vị thế giới.
// detailed = false khi nhìn xa → vẽ khối đơn giản cho nhanh.
void drawCreatureSprite(const Simulation& sim, Entity e, float px, float py, float time, bool detailed);

// Vật phẩm trên mặt đất: xác, xương, công cụ cũ, thịt, đá.
void drawGroundItems(const Simulation& sim, float x0, float y0, float x1, float y1);

} // namespace wb
