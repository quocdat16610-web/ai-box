// =============================================================================
// Species.h — Định nghĩa loài (data-driven) + cây tiến hoá.
//
// Danh sách loài KHÔNG cố định: khi một nhóm bị cô lập tiến hoá đủ xa, hệ thống
// tạo một SpeciesDef mới (con của loài gốc, `parent`) với bộ gen trung bình mới,
// tên mới theo ngôn ngữ của nhóm. Ví dụ: "Sói" → "Sói Grakul" (biết đứng thẳng,
// dùng công cụ nếu trí tuệ vượt ngưỡng).
// =============================================================================
#pragma once
#include "../ai/Genome.h"
#include "../ai/Knowledge.h"
#include <string>
#include <vector>

namespace wb {

enum class Diet : uint8_t { Herbivore, Carnivore, Omnivore };
enum class SocialType : uint8_t { Solitary, Herd, Pack, Tribal };
// Môi trường sống: quyết định đi/bơi/bay ở đâu, ăn gì, nhìn thấy ai.
//   Land  — trên cạn          Water — dưới nước (cá, cá mập)     Air — bay khắp nơi (chim, đại bàng)
//   Cave  — trong hang động dưới lòng đất (dơi, nhện hang)       Amphibious — cả cạn lẫn nước (cá sấu)
enum class Habitat : uint8_t { Land, Water, Air, Cave, Amphibious };
// Hình dáng cơ thể (để vẽ sprite 2D và hình khối 3D ở góc nhìn thứ nhất)
enum class BodyPlan : uint8_t { Humanoid, Quadruped, Bird, Fish, Bat, Spider, Reptile, Giant, Primate };

// ---- Gen tự tạo (do Thần "cấy ghép" ở phòng thí nghiệm, hoặc đột biến tự nhiên sinh ra) ----
// Mỗi gen có một hiệu ứng sinh học + cường độ; di truyền cho mọi cá thể và mọi loài hậu duệ.
enum class GeneEffect : uint8_t {
    Vitality,       // sinh lực: máu tối đa ↑
    Muscle,         // cơ bắp: sát thương ↑
    Agility,        // nhanh nhẹn: tốc độ ↑
    Brain,          // não bộ: trí tuệ ↑
    Fertility,      // sinh sản: đẻ nhiều ↑
    Longevity,      // trường thọ
    Fur,            // lông dày: chịu lạnh
    Sweat,          // tuyến mồ hôi: chịu nóng
    Venom,          // nọc độc: sát thương ↑↑
    Armor,          // vảy giáp: máu ↑, chậm hơn
    Photosynthesis, // quang hợp: đỡ đói (da có diệp lục)
    Glow,           // phát quang sinh học (nhìn thấy trong bóng tối)
    Gills,          // mang cá: sống được cả dưới nước
    Wings,          // cánh: bay được
    Count
};
constexpr int kGeneEffectCount = static_cast<int>(GeneEffect::Count);
const char* geneEffectName(GeneEffect e);
const char* geneEffectDesc(GeneEffect e);

struct CustomGene {
    std::string name;      // tên gen (người chơi đặt hoặc ngôn ngữ của loài)
    GeneEffect effect;
    float strength = 0.5f; // 0..1
};

struct SpeciesDef {
    std::string name;
    Diet diet = Diet::Omnivore;
    SocialType social = SocialType::Tribal;
    bool dexterous = false;    // có "tay khéo" → chế tạo công cụ, xây nhà
    bool monster = false;      // quái vật (hung hãn với mọi loài)
    Habitat habitat = Habitat::Land;
    float capShare = 0.05f;    // sức chứa môi trường: tỉ lệ tối đa của trần dân số thế giới (loài tiến hoá kế thừa)
    BodyPlan plan = BodyPlan::Quadruped;
    float size = 1.0f;         // kích thước tương đối (ảnh hưởng săn mồi, sức khoẻ)
    float baseHp = 60, baseDamage = 6, baseSpeed = 0.12f; // tốc độ: ô/tick
    int adultYears = 14, maxAgeYears = 70;
    int gestationTicks = 300, litterMax = 1;
    Genome baseGenome;         // gen trung bình của loài
    uint8_t r = 255, g = 255, b = 255; // màu sprite
    int parent = -1;           // loài tổ tiên (-1 = nguyên thuỷ)
    int donor = -1;            // loài cho gen (khi được tạo bằng cấy ghép gen)
    bool engineered = false;   // do Thần tạo ra ở phòng thí nghiệm gen
    std::vector<CustomGene> customGenes;
    // Tổng cường độ các gen tự tạo có hiệu ứng e
    float geneBonus(GeneEffect e) const {
        float t = 0;
        for (const CustomGene& c : customGenes) if (c.effect == e) t += c.strength;
        return t;
    }
    long bornTick = 0;
    int population = 0;        // cập nhật mỗi tick bởi Simulation
};

// Thêm một gen tự tạo vào loài: hiệu ứng lên bộ gen gốc / môi trường sống áp dụng MỘT lần ở đây,
// hiệu ứng lên chỉ số cơ thể áp dụng mỗi khi sinh ra cá thể (Simulation::spawnCreature).
void addCustomGene(SpeciesDef& d, const CustomGene& g);

class SpeciesRegistry {
public:
    void initDefaults();
    int add(const SpeciesDef& d) { defs_.push_back(d); return static_cast<int>(defs_.size()) - 1; }
    SpeciesDef& operator[](int id) { return defs_[id]; }
    const SpeciesDef& operator[](int id) const { return defs_[id]; }
    int count() const { return static_cast<int>(defs_.size()); }
    int find(const std::string& name) const {
        for (int i = 0; i < count(); ++i) if (defs_[i].name == name) return i;
        return -1;
    }
    // Loài gốc nguyên thuỷ (đi ngược cây tiến hoá) — để xác định quan hệ họ hàng.
    int root(int id) const { while (defs_[id].parent >= 0) id = defs_[id].parent; return id; }
    // a có thể săn b không?
    bool canHunt(int a, int b) const;
    std::vector<SpeciesDef>& all() { return defs_; }

private:
    std::vector<SpeciesDef> defs_;
};

// Id cố định của các loài mặc định (thứ tự trong initDefaults).
namespace sp {
enum : int {
    Human = 0, Elf, Orc, Dwarf, Wolf, Deer, Rabbit, Bear, Troll, Goblin,
    // Đa dạng sinh học: trên cạn, dưới nước, trên trời, trong hang
    Boar, Mammoth, Fox, Monkey, Crocodile, Eagle, Sparrow, Fish, Shark, Bat, CaveSpider,
    DefaultCount
};
}

} // namespace wb
