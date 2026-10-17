// =============================================================================
// Group.h — Tổ chức xã hội: Bầy → Bộ lạc → Làng → Thị trấn → Thành phố → Quốc gia.
//
// Một Group là một thực thể xã hội tự hình thành từ các cá thể (không do người
// chơi tạo). Nó có văn hoá, ngôn ngữ, tôn giáo, luật lệ, lãnh thổ, kho chung,
// quan hệ ngoại giao... Tất cả đều nổi lên (emergent) từ tính cách và tri thức
// của các thành viên: ví dụ tính hiếu chiến của văn hoá = trung bình tính hung
// hăng của thành viên; luật lệ được ban hành dựa trên tri thức + giá trị văn hoá.
// =============================================================================
#pragma once
#include "Language.h"
#include "../ai/Knowledge.h"
#include "../core/Ecs.h"
#include <string>
#include <vector>
#include <map>
#include <bitset>

namespace wb {

enum class Stage : uint8_t { Pack, Tribe, Village, Town, City, Nation, Count };
inline const char* stageName(Stage s, bool animal) {
    static const char* human[] = {"Nhóm du mục", "Bộ lạc", "Làng", "Thị trấn", "Thành phố", "Quốc gia"};
    static const char* beast[] = {"Bầy", "Bộ tộc", "Làng hang ổ", "Lãnh địa", "Đại lãnh địa", "Vương quốc"};
    return (animal ? beast : human)[static_cast<int>(s)];
}

enum class Government : uint8_t { AlphaRule, Chiefdom, ElderCouncil, Monarchy, Republic, Theocracy, MilitaryJunta, Count };
inline const char* governmentName(Government g) {
    static const char* n[] = {"Đầu đàn cai trị", "Tù trưởng", "Hội đồng già làng", "Quân chủ",
                              "Cộng hoà", "Thần quyền", "Quân phiệt"};
    return n[static_cast<int>(g)];
}

// Luật lệ — được ban hành dần khi nhóm có đủ tri thức/giá trị.
enum class Law : uint8_t {
    AlphaEatsFirst,   // con đầu đàn ăn trước
    ShareKills,       // chia sẻ con mồi / lương thực chung
    DefendTerritory,  // bảo vệ lãnh thổ
    NoKillingKin,     // cấm giết đồng loại trong nhóm
    TeachTheYoung,    // người già dạy trẻ (truyền tri thức nhanh hơn)
    Conscription,     // nghĩa vụ quân sự
    Taxation,         // thuế (nộp một phần thu hoạch)
    WorshipDuty,      // nghĩa vụ thờ cúng
    OpenBorders,      // mở cửa biên giới cho người ngoài
    Xenophobia,       // bài ngoại
    Count
};
constexpr int kLawCount = static_cast<int>(Law::Count);
inline const char* lawName(Law l) {
    static const char* n[] = {"Đầu đàn ăn trước", "Chia sẻ con mồi", "Bảo vệ lãnh thổ", "Cấm giết đồng loại",
                              "Người già dạy trẻ", "Nghĩa vụ quân sự", "Đóng thuế", "Nghĩa vụ thờ cúng",
                              "Mở cửa biên giới", "Bài ngoại"};
    return n[static_cast<int>(l)];
}

// Giá trị văn hoá (0..1) — trung bình động của tính cách thành viên.
struct Culture {
    float militarism = 0.5f, piety = 0.3f, curiosity = 0.5f, collectivism = 0.5f, xenophobia = 0.5f, tradition = 0.5f;
};

struct Relation {
    float opinion = 0;     // -100 .. +100
    bool atWar = false;
    long warStart = 0;
    int warCasualties = 0;
};

struct Group {
    int id = -1;
    bool alive = true;
    int species = 0;
    bool animal = false;     // loài không có tay khéo
    Stage stage = Stage::Pack;
    Government gov = Government::AlphaRule;
    std::string name;        // tên nhóm, sinh từ ngôn ngữ riêng
    std::string nationName;  // tên quốc gia (khi đạt Stage::Nation)
    std::string deity;       // thần linh (khi có tôn giáo)
    Language language;
    Culture culture;
    std::bitset<kLawCount> laws;
    TechSet knowledge;       // tri thức tập thể (văn hoá)
    int parentGroup = -1;    // nhóm mẹ (nếu tách ra)

    // Biểu tượng / quốc kỳ: màu + hình (0..7) sinh ngẫu nhiên
    uint8_t colR = 200, colG = 200, colB = 200, col2R = 50, col2G = 50, col2B = 50, symbol = 0;

    std::vector<Entity> members;
    Entity leader = NullEntity;
    int homeX = 0, homeY = 0;
    int territoryRadius = 0;
    int claimX = -1, claimY = -1;  // tâm lãnh thổ lần claim trước (để trả lại đất khi di cư)
    int territoryTiles = 0;

    // Kho chung
    int food = 0, wood = 0, stone = 0, ore = 0;

    // Công trình (đếm lại định kỳ)
    int dwellings = 0, housing = 0, buildingsTotal = 0;
    bool hasGranary = false, hasWorkshop = false, hasForge = false, hasTemple = false, hasTownHall = false;

    std::map<int, Relation> relations;
    int warTarget = -1;      // nhóm đang bị tấn công (mục tiêu hành quân)
    long foundedTick = 0, stageTick = 0, settledTicks = 0, lastSplitCheck = 0;
    float avgIntellect = 0;
    int peakPopulation = 0;
    int generationDepth = 0;  // số thế hệ đã trôi qua
    int kills = 0, losses = 0;

    int population() const { return static_cast<int>(members.size()); }
    bool atWarWith(int other) const {
        auto it = relations.find(other);
        return it != relations.end() && it->second.atWar;
    }
    bool atWar() const {
        for (auto& [k, r] : relations) if (r.atWar) return true;
        return false;
    }
    std::string displayName() const { return stage == Stage::Nation && !nationName.empty() ? nationName : name; }
};

} // namespace wb
