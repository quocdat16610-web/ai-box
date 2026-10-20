// =============================================================================
// Scenarios.h — Các kịch bản khởi đầu (ví dụ cụ thể trong yêu cầu thiết kế).
//
//   Sandbox     : thế giới đầy đủ — 4 chủng tộc, động vật, quái vật.
//   WolfPack    : một đàn sói giữa đồng cỏ đầy hươu/thỏ. Chúng học săn theo bầy
//                 → trí tuệ tăng (chọn lọc + kinh nghiệm) → thứ bậc → đánh dấu
//                 lãnh thổ → Bộ tộc sói có lãnh thổ và luật lệ (→ có thể tiến
//                 hoá thành loài mới và lập "Vương quốc Sói").
//   PrimalHumans: một nhóm người nguyên thuỷ trắng tay cạnh rừng và đồi đá. Họ
//                 tự học công cụ đá → nơi trú ẩn → chế biến gỗ → làng → trồng
//                 trọt → chữ viết → luật pháp → quốc gia.
// =============================================================================
#pragma once
#include <string>

namespace wb {

class Simulation;

//   TwinTribes  : HAI nhóm người nhân bản y hệt nhau (cùng bộ gen, cùng tri thức ban
//                 đầu) đặt ở hai môi trường trái ngược — bờ núi lửa và rừng lạnh ven
//                 sông. Mọi khác biệt về sau là do môi trường + trải nghiệm + ngẫu nhiên:
//                 ví dụ trực quan về sự phát triển KHÔNG đoán trước được.
enum class Scenario { Sandbox, WolfPack, PrimalHumans, TwinTribes, Count };
const char* scenarioName(Scenario s);
Scenario scenarioFromString(const std::string& s);
// Áp dụng kịch bản lên mô phỏng đã reset (có thể chỉnh cfg tốc độ tiến hoá).
void setupScenario(Simulation& sim, Scenario s);
// Thế giới chạy lâu dài (trên GitHub): nếu các loài có trí khôn gần tuyệt chủng (< minSapients cá thể),
// "thần linh gieo mầm" một bộ lạc nhỏ mới ở nơi sống được. Trả về true nếu đã gieo.
bool reseedSapients(Simulation& sim, int minSapients);

} // namespace wb
