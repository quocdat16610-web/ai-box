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
    long bornTick = 0;
    int population = 0;        // cập nhật mỗi tick bởi Simulation
};

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
