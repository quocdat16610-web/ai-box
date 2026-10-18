// =============================================================================
// Goap.cpp — Bảng hành động + bộ lập kế hoạch A*.
// =============================================================================
#include "Goap.h"
#include <vector>
#include <bitset>

namespace wb {

namespace {
using namespace ws;
constexpr int NT = -1; // không cần tech
constexpr int SK_FOR = 0, SK_HUN = 1, SK_CRA = 2, SK_BUI = 3, SK_COM = 4, SK_SOC = 5, SK_FAR = 6, SK_LEA = 7;

// Thứ tự phải khớp enum ActionId.
const ActionDef kActions[kActionCount] = {
    // tên                        pre                     effect          cost  tech                  tay   nhóm   ăn               kỹ năng
    {"Ăn đồ mang theo",           HasFood,                Fed,            1.0f, NT,                   false, false, DietReq::Any,    -1},
    {"Ăn tại chỗ",                NearFood,               Fed,            1.5f, NT,                   false, false, DietReq::Plants, SK_FOR},
    {"Đi tới nguồn thức ăn",      KnowsFood,              NearFood,       3.0f, NT,                   false, false, DietReq::Plants, -1},
    {"Tìm kiếm thức ăn",          0,                      KnowsFood,      6.0f, NT,                   false, false, DietReq::Plants, SK_FOR},
    {"Hái lượm",                  NearFood,               HasFood,        2.0f, NT,                   false, false, DietReq::Plants, SK_FOR},
    {"Lùng sục con mồi",          0,                      SeesPrey,       5.0f, NT,                   false, false, DietReq::Meat,   SK_HUN},
    {"Săn mồi",                   SeesPrey,               HasFood,        3.0f, NT,                   false, false, DietReq::Meat,   SK_HUN},
    {"Ăn xác chết",               SeesCarrion,            HasFood,        2.0f, NT,                   false, false, DietReq::Meat,   -1},
    {"Lấy lương thực trong kho",  GroupFood | AtHome,     Fed,            1.0f, NT,                   false, true,  DietReq::Any,    -1},
    {"Về nhà",                    0,                      AtHome,         3.0f, NT,                   false, true,  DietReq::Any,    -1},
    {"Ngủ ở nhà",                 AtHome,                 Rested,         1.0f, NT,                   false, true,  DietReq::Any,    -1},
    {"Ngủ ngoài trời",            0,                      Rested,         5.0f, NT,                   false, false, DietReq::Any,    -1},
    {"Bỏ chạy",                   0,                      Safe,           1.0f, NT,                   false, false, DietReq::Any,    -1},
    {"Chiến đấu",                 SeesEnemy,              EnemyDefeated,  3.0f, NT,                   false, false, DietReq::Any,    SK_COM},
    {"Hành quân tìm địch",        0,                      SeesEnemy,      5.0f, NT,                   false, true,  DietReq::Any,    SK_COM},
    {"Tìm bạn đời",               0,                      MateNearby,     4.0f, NT,                   false, false, DietReq::Any,    -1},
    {"Giao phối",                 MateNearby,             Reproduced,     1.0f, NT,                   false, false, DietReq::Any,    -1},
    {"Tìm đồng loại",             0,                      AllyNearby,     3.0f, NT,                   false, false, DietReq::Any,    -1},
    {"Giao tiếp / dạy học",       AllyNearby,             Socialized,     1.0f, NT,                   false, false, DietReq::Any,    SK_SOC},
    {"Khám phá",                  0,                      Explored,       2.0f, NT,                   false, false, DietReq::Any,    -1},
    {"Đốn gỗ",                    KnowsTree,              HasWood,        3.0f, NT,                   true,  false, DietReq::Any,    SK_FOR},
    {"Nhặt đá",                   KnowsStone,             HasStone,       3.0f, NT,                   true,  false, DietReq::Any,    SK_FOR},
    {"Đào quặng",                 KnowsOre,               HasOre,         4.0f, NT,                   true,  false, DietReq::Any,    SK_CRA},
    {"Chế tạo theo thiết kế",     0,                      HasTool,        2.0f, NT,                   true,  false, DietReq::Any,    SK_CRA},
    {"Sáng tạo thiết kế mới",     0,                      Invented,       3.0f, NT,                   true,  false, DietReq::Any,    SK_CRA},
    {"Gom vật liệu xây dựng",     0,                      HasMaterials,   4.0f, NT,                   false, true,  DietReq::Any,    SK_BUI},
    {"Xây công trình",            HasMaterials,           Built,          3.0f, NT,                   false, true,  DietReq::Any,    SK_BUI},
    {"Nộp gỗ vào kho",            HasWood,                Stored,         1.0f, NT,                   true,  true,  DietReq::Any,    -1},
    {"Nộp đá vào kho",            HasStone,               Stored,         1.0f, NT,                   true,  true,  DietReq::Any,    -1},
    {"Nộp thức ăn vào kho",       HasFood,                Stored,         1.0f, NT,                   false, true,  DietReq::Any,    -1},
    {"Làm ruộng",                 AtHome,                 Farmed,         2.0f, ti(Tech::Farming),    true,  true,  DietReq::Any,    SK_FAR},
    {"Đánh dấu lãnh thổ",         0,                      Marked,         2.0f, ti(Tech::ScentMarking), false, true, DietReq::Any,   -1},
    {"Thờ cúng",                  0,                      Worshipped,     2.0f, ti(Tech::Religion),   false, true,  DietReq::Any,    -1},
    {"Thách thức thủ lĩnh",       0,                      Dominant,       4.0f, ti(Tech::Hierarchy),  false, true,  DietReq::Any,    SK_LEA},
    {"Thí nghiệm vật liệu ở lò",  0,                      Experimented,   3.0f, NT,                   true,  true,  DietReq::Any,    SK_CRA},
    {"Chế biến theo quy trình",   0,                      Produced,       3.0f, NT,                   true,  true,  DietReq::Any,    SK_CRA},
    {"Nộp quặng/vật liệu vào kho", HasOre,                Stored,         1.0f, NT,                   true,  true,  DietReq::Any,    -1},
    {"Nghiên cứu khoa học",       0,                      Studied,        3.0f, NT,                   true,  false, DietReq::Any,    SK_CRA},
};

const char* kGoalNames[kGoalCount] = {
    "Thư thả", "Ăn", "Nghỉ ngơi", "Chạy trốn", "Tự vệ", "Sinh sản", "Giao tiếp", "Khám phá",
    "Lao động cho nhóm", "Chế tạo", "Sáng tạo", "Xây dựng", "Trồng trọt",
    "Giữ lãnh thổ", "Thờ cúng", "Chiến tranh", "Giành quyền lực", "Thí nghiệm", "Sản xuất", "Nghiên cứu",
};

const WorldState kGoalStates[kGoalCount] = {
    Explored, Fed, Rested, Safe, EnemyDefeated, Reproduced, Socialized, Explored,
    Stored, HasTool, Invented, Built, Farmed, Marked, Worshipped, EnemyDefeated, Dominant,
    Experimented, Produced, Studied,
};

const char* kSkillNames[kSkillCount] = {"Hái lượm", "Săn bắn", "Chế tạo", "Xây dựng", "Chiến đấu", "Xã giao", "Trồng trọt", "Lãnh đạo"};
} // namespace

const ActionDef& actionDef(ActionId a) { return kActions[static_cast<int>(a)]; }
const char* goalName(GoalId g) { return kGoalNames[static_cast<int>(g)]; }
WorldState goalState(GoalId g) { return kGoalStates[static_cast<int>(g)]; }
const char* skillName(Skill s) { return kSkillNames[static_cast<int>(s)]; }

bool planActions(WorldState start, WorldState goal, const PlannerAgentView& view, Plan& out) {
    out.clear();
    if ((start & goal) == goal) return true; // đã đạt
    struct Node { WorldState s; float g, f; int parent; ActionId act; int depth; bool closed; };
    // Bộ đệm cục bộ theo luồng: tránh cấp phát lại mỗi lần lập kế hoạch.
    thread_local std::vector<Node> nodes;
    nodes.clear();
    auto h = [&](WorldState s) { return static_cast<float>(std::bitset<64>(goal & ~s).count()); };
    nodes.push_back({start, 0, h(start), -1, ActionId::Count, 0, false});
    const int kMaxNodes = 300;
    // Lọc hành động LIÊN QUAN (lan ngược từ mục tiêu qua tiền điều kiện): hành động không góp
    // phần tạo ra bit nào cần thiết thì không bao giờ nằm trong kế hoạch tối ưu. Không lọc thì
    // A* tốn hết ngân sách nút cho các tổ hợp hành động rẻ vô ích (ăn, giao tiếp…) và bỏ lỡ
    // những kế hoạch dài hơn như "gom vật liệu → xây nhà".
    bool relevant[kActionCount] = {};
    WorldState needBits = goal & ~start;
    for (bool grew = true; grew;) {
        grew = false;
        for (int a = 0; a < kActionCount; ++a) {
            if (relevant[a] || !view.available[a] || !(kActions[a].effect & needBits)) continue;
            relevant[a] = true;
            WorldState more = kActions[a].pre & ~start & ~needBits;
            if (more) { needBits |= more; grew = true; }
        }
    }
    while (true) {
        // Chọn nút mở có f nhỏ nhất (số nút nhỏ → quét tuyến tính là đủ nhanh).
        int best = -1;
        for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
            if (!nodes[i].closed && (best < 0 || nodes[i].f < nodes[best].f)) best = i;
        if (best < 0) return false;
        Node cur = nodes[best];
        nodes[best].closed = true;
        if ((cur.s & goal) == goal) {
            // Truy vết ngược để dựng kế hoạch.
            std::array<ActionId, Plan::kMax> rev{};
            int n = 0;
            for (int i = best; nodes[i].parent >= 0 && n < Plan::kMax; i = nodes[i].parent) rev[n++] = nodes[i].act;
            out.len = static_cast<uint8_t>(n);
            for (int k = 0; k < n; ++k) out.steps[k] = rev[n - 1 - k];
            return true;
        }
        if (cur.depth >= Plan::kMax || static_cast<int>(nodes.size()) > kMaxNodes) continue;
        for (int a = 0; a < kActionCount; ++a) {
            if (!relevant[a]) continue;
            const ActionDef& d = kActions[a];
            if ((cur.s & d.pre) != d.pre) continue;
            WorldState ns = cur.s | d.effect;
            if (ns == cur.s) continue; // hành động vô ích
            float g = cur.g + d.cost * view.costMul[a];
            // Đã có nút cùng trạng thái với chi phí thấp hơn? bỏ qua.
            bool dominated = false;
            for (auto& n : nodes) if (n.s == ns && n.g <= g) { dominated = true; break; }
            if (dominated) continue;
            nodes.push_back({ns, g, g + h(ns), best, static_cast<ActionId>(a), cur.depth + 1, false});
        }
    }
}

} // namespace wb
