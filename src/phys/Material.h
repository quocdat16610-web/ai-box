// =============================================================================
// Material.h — Vật liệu với thuộc tính vật lý & hoá học (data-driven, mở rộng
// được khi đang chạy).
//
// Mọi thứ trong thế giới — gỗ, đá, quặng, kim loại, nước, máu, axit, khí… — là
// một Material. Trạng thái (rắn/lỏng/khí) KHÔNG lưu cố định mà suy ra từ nhiệt
// độ hiện tại so với điểm nóng chảy / điểm sôi → nước tự đóng băng, bốc hơi;
// đá nóng chảy thành dung nham; kim loại nung chảy để đúc.
//
// Danh sách vật liệu có thể DÀI THÊM VÔ HẠN: mỗi khi hai kim loại nóng chảy trộn
// lẫn, hệ thống sinh một hợp kim mới (xem MaterialRegistry::alloy) với thuộc
// tính tính từ hai vật liệu gốc + "hiệu ứng cộng hưởng" ngẫu nhiên theo seed thế
// giới → mỗi thế giới có một bảng hợp kim khác nhau, không ai đoán trước được.
// =============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace wb {

using MatId = uint16_t;
constexpr MatId kNoMat = 0;   // id 0 = "không khí / không có gì"

enum class Phase : uint8_t { Solid, Liquid, Gas };

struct MaterialDef {
    std::string name;       // tên tiếng Việt (thể rắn/mặc định)
    std::string formula;    // ký hiệu hoá học gợi ý (hiển thị debug)
    // ---- Vật lý ----
    float density = 1000;       // kg/m³ (nước = 1000) → nổi/chìm, khối lượng công cụ
    float meltPoint = 1e6f;     // °C
    float boilPoint = 1e6f;     // °C
    float heatCapacity = 1.0f;  // nhiệt dung tương đối (lớn → nóng/lạnh chậm)
    float conductivity = 0.3f;  // dẫn nhiệt 0..1 (thấp → cách nhiệt tốt, xây lò)
    float hardness = 1.0f;      // độ cứng Mohs 0..10 (lưỡi cắt, đào bới)
    float toughness = 0.5f;     // độ dai 0..1 (thấp = giòn, dễ vỡ)
    float elasticity = 0.1f;    // đàn hồi 0..1 (cung, lò xo)
    float viscosity = 0.2f;     // độ nhớt khi lỏng (dung nham cao, nước thấp)
    bool powder = false;        // dạng hạt/bột (cát, tro) — trượt theo dốc
    bool porous = false;        // xốp → không chứa được chất lỏng
    // ---- Hoá học ----
    float ignitionPoint = 0;    // °C bắt lửa (0 = không cháy)
    float combustionHeat = 0;   // nhiệt toả ra khi cháy (MJ/kg tương đối)
    float acidity = 7;          // pH 0..14 (axit < 7 < bazơ)
    float reactivity = 0;       // xu hướng oxi hoá 0..1 (sắt gỉ, vàng không)
    float toxicity = 0;         // độc tính khi hít/chạm
    float nutrition = 0;        // ăn được không
    bool metal = false, organic = false, oxidizer = false, explosive = false;
    // ---- Biến đổi đặc trưng (kNoMat = không có) ----
    MatId burnResidue = kNoMat;   // cặn sau khi cháy (gỗ → tro)
    MatId burnGas = kNoMat;       // khí sinh ra khi cháy (→ CO₂)
    MatId pyrolysisTo = kNoMat;   // cháy thiếu oxi (gỗ → than)
    MatId solidifiesTo = kNoMat;  // khi đông đặc lại thành chất khác (cát nóng chảy → thuỷ tinh)
    MatId firesTo = kNoMat;       // nung ≥ fireTemp thì biến thành (đất sét → gạch)
    float fireTemp = 0;
    MatId fireGas = kNoMat;       // khí thoát ra khi nung (đá vôi → CO₂)
    MatId smeltsTo = kNoMat;      // quặng + chất khử (than) ≥ smeltTemp → kim loại
    float smeltTemp = 0;
    MatId oxide = kNoMat;         // sản phẩm oxi hoá chậm (sắt → gỉ)
    MatId hydratesTo = kNoMat;    // + nước → (vôi sống → vôi tôi, toả nhiệt)
    float hydrationHeat = 0;
    MatId decaysTo = kNoMat;      // phân huỷ theo thời gian (thịt → đất mùn)
    MatId decayGas = kNoMat;      // (→ khí mê-tan)
    MatId dissolvedSalt = kNoMat; // bay hơi hết dung môi thì kết tủa (nước muối → muối)
    // ---- Hiển thị ----
    uint8_t r = 200, g = 200, b = 200;
    // ---- Nguồn gốc (hợp kim / vật liệu sinh động) ----
    MatId parentA = kNoMat, parentB = kNoMat;
    int generation = 0;   // 0 = tự nhiên, 1 = hợp kim cấp 1, ...
    bool natural = true;

    Phase phaseAt(float tempC) const {
        if (tempC >= boilPoint) return Phase::Gas;
        if (tempC >= meltPoint) return Phase::Liquid;
        return Phase::Solid;
    }
};

// Id cố định của các vật liệu tự nhiên (thứ tự trong MaterialRegistry::initDefaults).
namespace mat {
enum : MatId {
    Air = 0, Water, SaltWater, Salt, Sand, Glass, Dirt, Mud, Clay, Brick,
    Stone, Flint, Limestone, Quicklime, SlakedLime, Basalt, Obsidian,
    Wood, Charcoal, Ash, Fiber, Hide, Flesh, Bone, Blood, Fat,
    CopperOre, Copper, TinOre, Tin, IronOre, Iron, Rust, Gold,
    Sulfur, Saltpeter, Gunpowder, Acid, Lye, Oil,
    CO2, Methane, Hydrogen, SO2,
    Coal, SilverOre, Silver, Gem, Herb,   // khoáng sản lòng đất (than đá, bạc, đá quý) + thảo dược
    DefaultCount
};
}

class MaterialRegistry {
public:
    void initDefaults(uint32_t worldSeed);
    const MaterialDef& operator[](MatId id) const { return defs_[id]; }
    MaterialDef& operator[](MatId id) { return defs_[id]; }
    int count() const { return static_cast<int>(defs_.size()); }
    MatId add(const MaterialDef& d);

    // Hợp kim của hai kim loại (tạo mới nếu chưa có). Tính chất = pha trộn có
    // trọng số + cộng hưởng ngẫu nhiên xác định theo (seed thế giới, a, b).
    // `namer` (có thể rỗng) dùng để đặt tên theo ngôn ngữ của người phát hiện.
    MatId alloy(MatId a, MatId b, const std::string& suggestedName = "");
    // Tên hiển thị theo trạng thái ("Nước" / "Băng" / "Hơi nước", "Đồng nóng chảy"...)
    std::string displayName(MatId id, float tempC) const;
    // Điểm "tiến bộ" của vật liệu (dùng đặt tên thời đại & đo trình độ công nghệ)
    float advancement(MatId id) const;

    // Khoáng chất "lạ" sinh ngẫu nhiên theo seed (mỗi thế giới một khác) + kim loại của chúng.
    std::vector<MatId> exoticOres;

private:
    std::vector<MaterialDef> defs_;
    std::unordered_map<uint32_t, MatId> alloyCache_;
    uint32_t seed_ = 0;
};

} // namespace wb
