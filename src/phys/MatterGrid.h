// =============================================================================
// MatterGrid.h — Lớp vật chất của thế giới: nhiệt độ, chất lỏng, khí, phản ứng.
//
// Mỗi ô bản đồ có:
//   * Nhiệt độ (°C) — khuếch tán sang ô bên cạnh, kéo dần về nhiệt độ khí hậu.
//   * Tối đa 3 "ngăn" vật chất rời (rắn rời / lỏng) + 1 ngăn khí. Đây là nơi diễn
//     ra hoá học: nước mưa đọng, dung nham chảy, quặng + than trong lò, máu, tro…
//   * Nguồn nhiệt (bếp lửa / lò luyện) duy trì nhiệt độ mục tiêu.
//
// Vật lý (góc nhìn top-down nên "trọng lực" = chảy theo độ cao địa hình):
//   * Chất lỏng chảy xuống ô thấp hơn, tốc độ theo độ nhớt (nước nhanh, dung nham chậm).
//   * Chất bột (cát, tro) trượt khi dốc vượt góc nghỉ.
//   * Khí khuếch tán, trôi theo gió, thoát dần lên trời; áp suất = lượng × nhiệt độ.
//   * Nổ = áp suất tăng đột ngột → đẩy vật thể & sinh vật (xem Physics.h).
//
// Chỉ các ô "hoạt động" (có vật chất, đang cháy, nhiệt độ lệch nhiều) mới được
// mô phỏng hoá học → chi phí tỷ lệ với lượng hoạt động, không phải kích thước bản đồ.
// =============================================================================
#pragma once
#include "Material.h"
#include "../core/Math.h"
#include <vector>
#include <array>
#include <deque>
#include <string>

namespace wb {

class World;

struct MatterSlot {
    MatId mat = kNoMat;
    uint16_t amount = 0;   // đơn vị khối lượng (≈ kg)
    bool molten = false;   // đã từng nóng chảy (để biết khi đông đặc thì biến thành gì)
    bool empty() const { return mat == kNoMat || amount == 0; }
};

struct MatterCell {
    std::array<MatterSlot, 3> s;
    MatterSlot gas;
    float heatTarget = 0;   // >0: lò/bếp đang giữ ô ở nhiệt độ này
    int16_t heatOwner = -1; // nhóm sở hữu lò
};

// Loại sự kiện phản ứng (để debug & để sinh vật QUAN SÁT mà học).
enum class ReactionKind : uint8_t {
    Combustion, Pyrolysis, Melting, Solidifying, Boiling, Condensing, Smelting, Alloying,
    Firing, Hydration, Neutralization, AcidMetal, Oxidation, Decay, Precipitation, Quench,
    Mixing, Explosion, Freezing, Thawing, Vitrification, Count
};
const char* reactionName(ReactionKind k);

struct ReactionEvent {
    long tick = 0;
    int x = 0, y = 0;
    ReactionKind kind = ReactionKind::Combustion;
    MatId in[3] = {kNoMat, kNoMat, kNoMat};
    MatId out[2] = {kNoMat, kNoMat};
    float temp = 0;     // nhiệt độ khi phản ứng
    bool lowOxygen = false;
    std::string describe(const MaterialRegistry& reg) const; // "Gỗ + O₂ → Tro + CO₂ (650°C)"
};

struct Explosion { int x, y; float energy; long tick; };

class MatterGrid {
public:
    void init(World& w);
    // Mô phỏng một tick (gọi từ World::update).
    void update(World& w, long tick, Rng& rng);

    // ---- Truy vấn ----
    float temp(int x, int y) const { return temp_[idx(x, y)]; }
    float ambient(const World& w, int x, int y) const;
    MatterCell& cell(int x, int y) { return cells_[idx(x, y)]; }
    const MatterCell& cell(int x, int y) const { return cells_[idx(x, y)]; }
    int amountOf(int x, int y, MatId m) const;
    bool wet(int x, int y) const;          // có nước lỏng (dập lửa, oxi hoá kim loại)
    float pressure(int x, int y) const;    // áp suất khí tương đối

    // ---- Tác động ----
    void addHeat(int x, int y, float deltaC);
    void setTemp(int x, int y, float c);
    // Thêm vật chất rời / lỏng vào ô (trả về lượng thực sự thêm được).
    int addMatter(int x, int y, MatId m, int amount, float tempC = -1000);
    int takeMatter(int x, int y, MatId m, int amount);
    void addGas(int x, int y, MatId m, int amount);
    void setHeatSource(int x, int y, float targetC, int owner);
    void clearHeatSource(int x, int y);
    void ignite(int x, int y);             // châm lửa: đốt nóng tới 700°C
    void explode(int x, int y, float energy, long tick);

    // ---- Sự kiện (debug + quan sát học hỏi) ----
    const std::deque<ReactionEvent>& recent() const { return recent_; }
    const std::vector<ReactionEvent>& thisTick() const { return tick_; }
    std::vector<Explosion>& explosions() { return explosions_; }
    std::array<long, static_cast<int>(ReactionKind::Count)> counts{}; // thống kê
    int activeCount() const { return static_cast<int>(active_.size()); }
    int burningCount() const { return static_cast<int>(burning_.size()); }

    // Khí quyển toàn cầu (chu trình vật chất): CO₂ & độ ẩm.
    float atmosCO2 = 280;    // ppm
    float humidity = 0.5f;   // 0..1 → ảnh hưởng xác suất mưa
    Vec2 wind{0.05f, 0.0f};

    // Gọi nội bộ bởi Chemistry (công khai để file Chemistry.cpp dùng)
    void emit(const ReactionEvent& e);
    void activate(int i);
    int idx(int x, int y) const { return y * w_ + x; }
    int width() const { return w_; }
    int height() const { return h_; }
    std::vector<float>& temps() { return temp_; }

private:
    void thermalStep(World& w);
    void fireStep(World& w, Rng& rng, long tick);
    void cellStep(World& w, int i, Rng& rng, long tick);
    void flowLiquids(World& w, int i);
    void flowGas(World& w, int i, Rng& rng);
    void terrainPhaseStep(World& w, long tick);
    int w_ = 0, h_ = 0;
    std::vector<float> temp_, temp2_;
    std::vector<MatterCell> cells_;
    std::vector<int> active_, nextActive_;
    std::vector<uint8_t> activeFlag_;
    std::vector<int> burning_;
    std::deque<ReactionEvent> recent_;
    std::vector<ReactionEvent> tick_;
    std::vector<Explosion> explosions_;
    long curTick_ = 0;
};

// Hoá học trên một ô (Chemistry.cpp): mọi quy luật phản ứng tổng quát.
void reactCell(World& w, MatterGrid& g, int x, int y, Rng& rng, long tick);

} // namespace wb
