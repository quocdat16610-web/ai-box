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

    // ---------------- Đa dạng sinh học ----------------
    //                       Int   Str   Spd   Soc   Agg   Cur   Fer   Lon   Cold  Heat
    SpeciesDef boar;
    boar.name = "Lợn rừng"; boar.diet = Diet::Omnivore; boar.social = SocialType::Herd; boar.size = 0.8f;
    boar.baseGenome = makeGenome({0.12f, 0.65f, 0.55f, 0.55f, 0.55f, 0.25f, 0.70f, 0.35f, 0.55f, 0.50f});
    boar.r = 95; boar.g = 70; boar.b = 55; boar.adultYears = 1; boar.maxAgeYears = 14; boar.gestationTicks = 140; boar.litterMax = 4;
    boar.baseHp = 50; boar.baseDamage = 7; boar.baseSpeed = 0.14f;
    defs_.push_back(boar);

    SpeciesDef mammoth;
    mammoth.name = "Voi ma mút"; mammoth.diet = Diet::Herbivore; mammoth.social = SocialType::Herd; mammoth.size = 2.2f;
    mammoth.plan = BodyPlan::Giant;
    mammoth.baseGenome = makeGenome({0.20f, 0.95f, 0.35f, 0.75f, 0.30f, 0.25f, 0.25f, 0.75f, 0.95f, 0.15f});
    mammoth.r = 120; mammoth.g = 85; mammoth.b = 60; mammoth.adultYears = 8; mammoth.maxAgeYears = 60; mammoth.gestationTicks = 600;
    mammoth.baseHp = 260; mammoth.baseDamage = 18; mammoth.baseSpeed = 0.10f;
    defs_.push_back(mammoth);

    SpeciesDef fox;
    fox.name = "Cáo"; fox.diet = Diet::Carnivore; fox.social = SocialType::Solitary; fox.size = 0.5f;
    fox.baseGenome = makeGenome({0.22f, 0.30f, 0.80f, 0.25f, 0.40f, 0.55f, 0.60f, 0.30f, 0.65f, 0.45f});
    fox.r = 215; fox.g = 120; fox.b = 50; fox.adultYears = 1; fox.maxAgeYears = 10; fox.gestationTicks = 120; fox.litterMax = 3;
    fox.baseHp = 22; fox.baseDamage = 5; fox.baseSpeed = 0.18f;
    defs_.push_back(fox);

    // Khỉ: có chi trước khá khéo (gen 0.45) nhưng chưa "biết dùng tay" — có thể TIẾN HOÁ thành loài dùng công cụ
    SpeciesDef monkey;
    monkey.name = "Khỉ"; monkey.diet = Diet::Omnivore; monkey.social = SocialType::Pack; monkey.size = 0.6f;
    monkey.plan = BodyPlan::Primate;
    monkey.baseGenome = makeGenome({0.27f, 0.35f, 0.60f, 0.80f, 0.35f, 0.75f, 0.60f, 0.40f, 0.35f, 0.70f});
    monkey.r = 150; monkey.g = 105; monkey.b = 70; monkey.adultYears = 4; monkey.maxAgeYears = 25; monkey.gestationTicks = 200;
    monkey.baseHp = 30; monkey.baseDamage = 4; monkey.baseSpeed = 0.15f;
    defs_.push_back(monkey);

    SpeciesDef croc;
    croc.name = "Cá sấu"; croc.diet = Diet::Carnivore; croc.social = SocialType::Solitary; croc.size = 1.4f;
    croc.habitat = Habitat::Amphibious; croc.plan = BodyPlan::Reptile;
    croc.baseGenome = makeGenome({0.12f, 0.80f, 0.35f, 0.15f, 0.75f, 0.15f, 0.40f, 0.85f, 0.20f, 0.85f});
    croc.r = 70; croc.g = 105; croc.b = 60; croc.adultYears = 6; croc.maxAgeYears = 70; croc.gestationTicks = 300; croc.litterMax = 4;
    croc.baseHp = 110; croc.baseDamage = 15; croc.baseSpeed = 0.09f;
    defs_.push_back(croc);

    SpeciesDef eagle;
    eagle.name = "Đại bàng"; eagle.diet = Diet::Carnivore; eagle.social = SocialType::Solitary; eagle.size = 0.6f;
    eagle.habitat = Habitat::Air; eagle.plan = BodyPlan::Bird;
    eagle.baseGenome = makeGenome({0.20f, 0.45f, 0.90f, 0.15f, 0.50f, 0.40f, 0.35f, 0.50f, 0.60f, 0.55f});
    eagle.r = 120; eagle.g = 90; eagle.b = 60; eagle.adultYears = 2; eagle.maxAgeYears = 25; eagle.gestationTicks = 200; eagle.litterMax = 2;
    eagle.baseHp = 20; eagle.baseDamage = 6; eagle.baseSpeed = 0.24f;
    defs_.push_back(eagle);

    SpeciesDef sparrow;
    sparrow.name = "Chim sẻ"; sparrow.diet = Diet::Herbivore; sparrow.social = SocialType::Herd; sparrow.size = 0.15f;
    sparrow.habitat = Habitat::Air; sparrow.plan = BodyPlan::Bird;
    sparrow.baseGenome = makeGenome({0.08f, 0.10f, 0.85f, 0.70f, 0.05f, 0.40f, 0.90f, 0.20f, 0.45f, 0.55f});
    sparrow.r = 160; sparrow.g = 130; sparrow.b = 90; sparrow.adultYears = 1; sparrow.maxAgeYears = 4; sparrow.gestationTicks = 60; sparrow.litterMax = 4;
    sparrow.baseHp = 5; sparrow.baseDamage = 0.5f; sparrow.baseSpeed = 0.22f;
    defs_.push_back(sparrow);

    SpeciesDef fish;
    fish.name = "Cá"; fish.diet = Diet::Herbivore; fish.social = SocialType::Herd; fish.size = 0.3f;
    fish.habitat = Habitat::Water; fish.plan = BodyPlan::Fish;
    fish.baseGenome = makeGenome({0.05f, 0.15f, 0.70f, 0.75f, 0.05f, 0.20f, 0.95f, 0.25f, 0.55f, 0.50f});
    fish.r = 120; fish.g = 175; fish.b = 200; fish.adultYears = 1; fish.maxAgeYears = 6; fish.gestationTicks = 60; fish.litterMax = 6;
    fish.baseHp = 8; fish.baseDamage = 0.5f; fish.baseSpeed = 0.15f;
    defs_.push_back(fish);

    SpeciesDef shark;
    shark.name = "Cá mập"; shark.diet = Diet::Carnivore; shark.social = SocialType::Solitary; shark.size = 1.5f;
    shark.habitat = Habitat::Water; shark.plan = BodyPlan::Fish;
    shark.baseGenome = makeGenome({0.15f, 0.80f, 0.70f, 0.10f, 0.80f, 0.25f, 0.30f, 0.70f, 0.50f, 0.50f});
    shark.r = 110; shark.g = 125; shark.b = 140; shark.adultYears = 5; shark.maxAgeYears = 40; shark.gestationTicks = 400; shark.litterMax = 2;
    shark.baseHp = 90; shark.baseDamage = 14; shark.baseSpeed = 0.17f;
    defs_.push_back(shark);

    SpeciesDef bat;
    bat.name = "Dơi"; bat.diet = Diet::Herbivore; bat.social = SocialType::Herd; bat.size = 0.25f;
    bat.habitat = Habitat::Cave; bat.plan = BodyPlan::Bat;
    bat.baseGenome = makeGenome({0.10f, 0.10f, 0.80f, 0.85f, 0.10f, 0.30f, 0.80f, 0.40f, 0.60f, 0.50f});
    bat.r = 70; bat.g = 60; bat.b = 75; bat.adultYears = 1; bat.maxAgeYears = 15; bat.gestationTicks = 120; bat.litterMax = 1;
    bat.baseHp = 6; bat.baseDamage = 0.5f; bat.baseSpeed = 0.18f;
    defs_.push_back(bat);

    SpeciesDef spider;
    spider.name = "Nhện hang"; spider.diet = Diet::Carnivore; spider.social = SocialType::Solitary; spider.size = 0.4f;
    spider.habitat = Habitat::Cave; spider.plan = BodyPlan::Spider;
    spider.baseGenome = makeGenome({0.10f, 0.40f, 0.55f, 0.05f, 0.85f, 0.20f, 0.70f, 0.30f, 0.60f, 0.50f});
    spider.r = 60; spider.g = 50; spider.b = 45; spider.adultYears = 1; spider.maxAgeYears = 6; spider.gestationTicks = 150; spider.litterMax = 5;
    spider.baseHp = 14; spider.baseDamage = 5; spider.baseSpeed = 0.13f;
    defs_.push_back(spider);

    // Sức chứa môi trường (tỉ lệ trần dân số): con mồi nhỏ nhiều, thú săn lớn ít, quái vật hiếm
    const std::pair<int, float> caps[] = {
        {sp::Wolf, 0.025f}, {sp::Deer, 0.07f}, {sp::Rabbit, 0.06f}, {sp::Bear, 0.012f}, {sp::Troll, 0.002f}, {sp::Goblin, 0.03f},
        {sp::Boar, 0.03f}, {sp::Mammoth, 0.015f}, {sp::Fox, 0.012f}, {sp::Monkey, 0.03f}, {sp::Crocodile, 0.006f}, {sp::Eagle, 0.006f},
        {sp::Sparrow, 0.04f}, {sp::Fish, 0.06f}, {sp::Shark, 0.006f}, {sp::Bat, 0.03f}, {sp::CaveSpider, 0.012f}};
    for (auto& d : defs_) if (d.dexterous) d.capShare = 0.5f;
    for (auto [id, c] : caps) defs_[id].capShare = c;

    // Hình dáng: chủng tộc có tay khéo đứng thẳng
    for (auto& d : defs_) if (d.dexterous) d.plan = BodyPlan::Humanoid;
    defs_[sp::Troll].plan = BodyPlan::Humanoid;

    // Gen sinh học bổ sung: khéo tay, kích thước, bơi lội — cũng tiến hoá qua các thế hệ
    for (auto& d : defs_) {
        d.baseGenome[Gene::Dexterity] = d.dexterous ? 0.8f : 0.08f;
        d.baseGenome[Gene::Size] = 0.5f;
        d.baseGenome[Gene::Swimming] = 0.3f;
    }
    defs_[sp::Wolf].baseGenome[Gene::Swimming] = 0.45f;
    defs_[sp::Bear].baseGenome[Gene::Swimming] = 0.55f;
    defs_[sp::Deer].baseGenome[Gene::Swimming] = 0.4f;
    defs_[sp::Wolf].baseGenome[Gene::Dexterity] = 0.15f; // chân trước sói linh hoạt hơn hươu một chút
    defs_[sp::Monkey].baseGenome[Gene::Dexterity] = 0.45f; // khỉ: bàn tay cầm nắm — sát ngưỡng tiến hoá thành loài dùng công cụ
    defs_[sp::Crocodile].baseGenome[Gene::Swimming] = 0.95f;
    defs_[sp::Fish].baseGenome[Gene::Swimming] = 1.0f;
    defs_[sp::Shark].baseGenome[Gene::Swimming] = 1.0f;
    defs_[sp::Mammoth].baseGenome[Gene::Size] = 0.8f;
}

const char* geneName(Gene g) {
    static const char* n[] = {"Trí tuệ", "Sức mạnh", "Tốc độ", "Tính bầy đàn", "Hung hăng", "Tò mò", "Sinh sản",
                              "Tuổi thọ", "Chịu lạnh", "Chịu nóng", "Khéo tay", "Kích thước", "Bơi lội"};
    return n[static_cast<int>(g)];
}

const char* geneEffectName(GeneEffect e) {
    static const char* n[] = {"Sinh lực", "Cơ bắp", "Nhanh nhẹn", "Não bộ", "Sinh sản", "Trường thọ", "Lông dày",
                              "Tuyến mồ hôi", "Nọc độc", "Vảy giáp", "Quang hợp", "Phát quang", "Mang cá", "Đôi cánh"};
    return n[static_cast<int>(e)];
}

const char* geneEffectDesc(GeneEffect e) {
    static const char* n[] = {"Máu tối đa tăng", "Sát thương tăng", "Tốc độ di chuyển tăng", "Tiềm năng trí tuệ tăng (học & phát minh nhanh hơn)",
                              "Đẻ nhiều con hơn", "Sống lâu hơn", "Chịu lạnh tốt", "Chịu nóng tốt", "Đòn đánh có độc: sát thương tăng mạnh",
                              "Da vảy cứng: máu tăng nhưng chậm hơn", "Da có diệp lục: đói chậm hơn khi có nắng",
                              "Cơ thể phát sáng trong bóng tối", "Thở được dưới nước: sống cả trên cạn lẫn dưới nước",
                              "Mọc cánh: bay qua núi, sông, biển"};
    return n[static_cast<int>(e)];
}

void addCustomGene(SpeciesDef& d, const CustomGene& g) {
    d.customGenes.push_back(g);
    float s = g.strength;
    Genome& b = d.baseGenome;
    switch (g.effect) {
    case GeneEffect::Brain: b[Gene::Intelligence] = clamp01(b[Gene::Intelligence] + 0.25f * s); break;
    case GeneEffect::Fertility: b[Gene::Fertility] = clamp01(b[Gene::Fertility] + 0.3f * s); d.litterMax = std::max(d.litterMax, 1 + static_cast<int>(s * 3)); break;
    case GeneEffect::Longevity: b[Gene::Longevity] = clamp01(b[Gene::Longevity] + 0.3f * s); d.maxAgeYears = static_cast<int>(d.maxAgeYears * (1 + s)); break;
    case GeneEffect::Fur: b[Gene::ColdResist] = clamp01(b[Gene::ColdResist] + 0.4f * s); break;
    case GeneEffect::Sweat: b[Gene::HeatResist] = clamp01(b[Gene::HeatResist] + 0.4f * s); break;
    case GeneEffect::Gills:
        b[Gene::Swimming] = 1.0f;
        if (d.habitat == Habitat::Land) d.habitat = Habitat::Amphibious;
        break;
    case GeneEffect::Wings:
        if (d.habitat != Habitat::Cave) d.habitat = Habitat::Air;
        break;
    default: break;   // các hiệu ứng chỉ số cơ thể: áp dụng lúc sinh ra (spawnCreature)
    }
}

bool SpeciesRegistry::canHunt(int a, int b) const {
    const SpeciesDef& A = defs_[a];
    const SpeciesDef& B = defs_[b];
    if (A.diet == Diet::Herbivore) return false;
    if (root(a) == root(b)) return false; // không ăn thịt đồng loại
    // Môi trường sống phải "chạm" nhau: thú trong hang không săn được thú trên mặt đất; cá mập
    // chỉ săn dưới nước; thú cạn không bắt được cá (trừ loài lội nước/bay: cá sấu, đại bàng)
    auto layer = [](Habitat h) { return h == Habitat::Cave ? 1 : 0; };
    if (layer(A.habitat) != layer(B.habitat)) return false;
    if (B.habitat == Habitat::Water && A.habitat != Habitat::Water && A.habitat != Habitat::Amphibious && A.habitat != Habitat::Air) return false;
    if (A.habitat == Habitat::Water && B.habitat != Habitat::Water && B.habitat != Habitat::Amphibious) return false;
    if (A.monster) return true;
    // Thú săn những loài nhỏ hơn đáng kể hoặc ăn cỏ; loài có trí tuệ không là "con mồi" của nhau
    if (B.dexterous && !A.monster && A.diet != Diet::Carnivore) return false;
    return B.size <= A.size * 1.3f && (B.diet == Diet::Herbivore || B.size < A.size * 0.6f);
}

} // namespace wb
