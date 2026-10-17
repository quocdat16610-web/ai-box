// Species.cpp — Thông số các loài mặc định (giống WorldBox: 4 chủng tộc + động vật + quái vật).
#include "Species.h"

namespace wb {

namespace {
// Tạo genome từ danh sách giá trị theo thứ tự enum Gene.
Genome makeGenome(std::initializer_list<float> v) {
    Genome g; int i = 0;
    for (float x : v) g.g[i++] = x;
    return g;
}
} // namespace

void SpeciesRegistry::initDefaults() {
    defs_.clear();
    //                       Int   Str   Spd   Soc   Agg   Cur   Fer   Lon   Cold  Heat
    SpeciesDef human;
    human.name = "Người"; human.diet = Diet::Omnivore; human.social = SocialType::Tribal; human.dexterous = true;
    human.baseGenome = makeGenome({0.40f, 0.50f, 0.50f, 0.65f, 0.40f, 0.60f, 0.55f, 0.50f, 0.50f, 0.50f});
    human.r = 230; human.g = 190; human.b = 150; human.adultYears = 14; human.maxAgeYears = 70;
    defs_.push_back(human);

    SpeciesDef elf = human;
    elf.name = "Elf"; elf.r = 190; elf.g = 240; elf.b = 170; elf.maxAgeYears = 300; elf.adultYears = 25;
    elf.baseGenome = makeGenome({0.48f, 0.35f, 0.60f, 0.60f, 0.25f, 0.70f, 0.30f, 0.90f, 0.50f, 0.40f});
    elf.gestationTicks = 500;
    defs_.push_back(elf);

    SpeciesDef orc = human;
    orc.name = "Orc"; orc.r = 110; orc.g = 170; orc.b = 80; orc.maxAgeYears = 50; orc.adultYears = 10;
    orc.baseGenome = makeGenome({0.32f, 0.75f, 0.50f, 0.60f, 0.80f, 0.40f, 0.70f, 0.40f, 0.55f, 0.65f});
    orc.baseHp = 80; orc.baseDamage = 9;
    defs_.push_back(orc);

    SpeciesDef dwarf = human;
    dwarf.name = "Dwarf"; dwarf.r = 200; dwarf.g = 130; dwarf.b = 90; dwarf.maxAgeYears = 150; dwarf.adultYears = 20;
    dwarf.baseGenome = makeGenome({0.42f, 0.70f, 0.35f, 0.60f, 0.45f, 0.50f, 0.40f, 0.70f, 0.80f, 0.40f});
    dwarf.baseHp = 75; dwarf.size = 0.8f;
    defs_.push_back(dwarf);

    SpeciesDef wolf;
    wolf.name = "Sói"; wolf.diet = Diet::Carnivore; wolf.social = SocialType::Pack; wolf.dexterous = false;
    wolf.baseGenome = makeGenome({0.20f, 0.55f, 0.75f, 0.70f, 0.60f, 0.40f, 0.60f, 0.40f, 0.70f, 0.40f});
    wolf.r = 150; wolf.g = 150; wolf.b = 160; wolf.adultYears = 2; wolf.maxAgeYears = 13;
    wolf.gestationTicks = 120; wolf.litterMax = 3; wolf.baseHp = 45; wolf.baseDamage = 7; wolf.baseSpeed = 0.16f; wolf.size = 0.9f;
    defs_.push_back(wolf);

    SpeciesDef deer;
    deer.name = "Hươu"; deer.diet = Diet::Herbivore; deer.social = SocialType::Herd;
    deer.baseGenome = makeGenome({0.10f, 0.40f, 0.80f, 0.60f, 0.10f, 0.20f, 0.70f, 0.40f, 0.50f, 0.50f});
    deer.r = 170; deer.g = 120; deer.b = 70; deer.adultYears = 1; deer.maxAgeYears = 12;
    deer.gestationTicks = 120; deer.litterMax = 2; deer.baseHp = 35; deer.baseDamage = 2; deer.baseSpeed = 0.17f;
    defs_.push_back(deer);

    SpeciesDef rabbit = deer;
    rabbit.name = "Thỏ"; rabbit.social = SocialType::Solitary; rabbit.size = 0.3f; rabbit.r = 235; rabbit.g = 225; rabbit.b = 210;
    rabbit.baseGenome = makeGenome({0.06f, 0.15f, 0.70f, 0.30f, 0.05f, 0.30f, 0.90f, 0.20f, 0.50f, 0.50f});
    rabbit.maxAgeYears = 5; rabbit.gestationTicks = 60; rabbit.litterMax = 4; rabbit.baseHp = 12; rabbit.baseDamage = 1;
    defs_.push_back(rabbit);

    SpeciesDef bear;
    bear.name = "Gấu"; bear.diet = Diet::Omnivore; bear.social = SocialType::Solitary; bear.size = 1.6f;
    bear.baseGenome = makeGenome({0.18f, 0.90f, 0.40f, 0.20f, 0.50f, 0.30f, 0.35f, 0.50f, 0.80f, 0.30f});
    bear.r = 110; bear.g = 75; bear.b = 50; bear.adultYears = 4; bear.maxAgeYears = 25;
    bear.baseHp = 120; bear.baseDamage = 14; bear.baseSpeed = 0.12f; bear.gestationTicks = 250;
    defs_.push_back(bear);

    SpeciesDef troll;
    troll.name = "Quỷ khổng lồ"; troll.diet = Diet::Carnivore; troll.social = SocialType::Solitary; troll.monster = true; troll.size = 2.0f;
    troll.baseGenome = makeGenome({0.22f, 0.95f, 0.30f, 0.15f, 0.95f, 0.25f, 0.20f, 0.70f, 0.70f, 0.50f});
    troll.r = 90; troll.g = 110; troll.b = 120; troll.adultYears = 10; troll.maxAgeYears = 120;
    troll.baseHp = 220; troll.baseDamage = 22; troll.baseSpeed = 0.09f; troll.gestationTicks = 900;
    defs_.push_back(troll);

    SpeciesDef goblin;
    goblin.name = "Yêu tinh"; goblin.diet = Diet::Omnivore; goblin.social = SocialType::Tribal; goblin.dexterous = true;
    goblin.monster = true; goblin.size = 0.7f;
    goblin.baseGenome = makeGenome({0.30f, 0.35f, 0.65f, 0.55f, 0.75f, 0.55f, 0.85f, 0.30f, 0.50f, 0.60f});
    goblin.r = 160; goblin.g = 200; goblin.b = 60; goblin.adultYears = 6; goblin.maxAgeYears = 35;
    goblin.baseHp = 35; goblin.baseDamage = 5; goblin.gestationTicks = 200; goblin.litterMax = 2;
    defs_.push_back(goblin);
}

bool SpeciesRegistry::canHunt(int a, int b) const {
    const SpeciesDef& A = defs_[a];
    const SpeciesDef& B = defs_[b];
    if (A.diet == Diet::Herbivore) return false;
    if (root(a) == root(b)) return false; // không ăn thịt đồng loại
    if (A.monster) return true;
    // Thú săn những loài nhỏ hơn đáng kể hoặc ăn cỏ; loài có trí tuệ không là "con mồi" của nhau
    if (B.dexterous && !A.monster && A.diet != Diet::Carnivore) return false;
    return B.size <= A.size * 1.3f && (B.diet == Diet::Herbivore || B.size < A.size * 0.6f);
}

} // namespace wb
