// =============================================================================
// Drives.h — NHU CẦU và THAM VỌNG riêng của từng cá thể.
//
// Nhu cầu (tháp Maslow rút gọn): mỗi nhu cầu là một số 0..1 (0 = thoả mãn,
// 1 = thiếu thốn cùng cực), tính lại mỗi nhịp suy nghĩ từ cơ thể, môi trường
// và vị trí xã hội. Nhu cầu càng thiếu thì mục tiêu thoả mãn nó càng được ưu
// tiên trong Utility AI. Nhu cầu bậc thấp (ăn, ngủ, an toàn) lấn át bậc cao.
//
// Tham vọng: một ước mơ cả đời chọn lúc trưởng thành theo tính cách, giống loài,
// vai trò (vd. kẻ tham vọng + hung hăng → "trở thành thủ lĩnh"; người tò mò →
// "thấu hiểu tự nhiên"). Tham vọng làm lệch trọng số mục tiêu suốt đời. Đạt được
// → hạnh phúc, có thể đặt mục tiêu lớn hơn (cấp tiếp theo); mãi không đạt
// → bất mãn (dễ thách thức thủ lĩnh, bỏ nhóm…).
// =============================================================================
#pragma once
#include <array>
#include <cstdint>

namespace wb {

enum class Need : uint8_t {
    Food,       // ăn
    Rest,       // ngủ nghỉ
    Warmth,     // giữ ấm
    Safety,     // an toàn trước thú dữ / kẻ thù / lửa
    Shelter,    // có mái nhà
    Belonging,  // thuộc về một nhóm, có người thân/bạn bè
    Esteem,     // được tôn trọng (địa vị)
    Curiosity,  // khát khao hiểu biết, khám phá
    Count
};
constexpr int kNeedCount = static_cast<int>(Need::Count);
inline const char* needName(Need n) {
    static const char* s[] = {"Ăn", "Ngủ nghỉ", "Giữ ấm", "An toàn", "Chỗ ở", "Thuộc về", "Được tôn trọng", "Hiểu biết"};
    return s[static_cast<int>(n)];
}

enum class Ambition : uint8_t {
    None,
    Survive,   // chỉ mong sống sót (sinh vật bản năng)
    Family,    // con đàn cháu đống
    Wealth,    // giàu có: tích trữ kim loại quý, đá quý
    Power,     // trở thành thủ lĩnh
    Inventor,  // phát minh ra thứ mới
    Scholar,   // thấu hiểu tự nhiên (khoa học)
    Builder,   // xây dựng nhà cửa, công trình
    Warrior,   // chiến binh lừng danh
    Explorer,  // đi khắp thế giới
    Devout,    // phụng sự thần linh
    Count
};
constexpr int kAmbitionCount = static_cast<int>(Ambition::Count);
inline const char* ambitionName(Ambition a) {
    static const char* s[] = {"(chưa có)", "Sống sót", "Con đàn cháu đống", "Giàu có", "Trở thành thủ lĩnh",
                              "Phát minh vĩ đại", "Thấu hiểu tự nhiên", "Xây dựng công trình", "Chiến binh lừng danh",
                              "Khám phá thế giới", "Phụng sự thần linh"};
    return s[static_cast<int>(a)];
}

struct Drives {
    std::array<float, kNeedCount> need{};  // 0 = thoả mãn, 1 = rất thiếu
    Ambition ambition = Ambition::None;
    int ambitionLevel = 0;      // số lần đã đạt (mục tiêu kế tiếp lớn hơn)
    float progress = 0;         // tiến độ tới tham vọng hiện tại 0..1
    float happiness = 0.6f;     // 0..1 (trung bình động)
    long ambitionTick = 0;      // lúc chọn tham vọng hiện tại
    // Bộ đếm thành tựu cá nhân (dùng đo tiến độ tham vọng)
    uint16_t builds = 0, explores = 0, worships = 0;
    float need_(Need n) const { return need[static_cast<int>(n)]; }
};

} // namespace wb
