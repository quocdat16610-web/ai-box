// =============================================================================
// Invention.h — Công nghệ MỞ: khái niệm vật lý, thiết kế, quy trình hoá học.
//
// KHÔNG có cây công nghệ. Thay vào đó, tri thức vật chất có 4 lớp, đều mở rộng
// vô hạn và phụ thuộc vào trải nghiệm riêng của từng cá thể:
//
//  1. VẬT LIỆU đã biết   — những thứ đã cầm, đào, nhặt, hoặc thấy sinh ra trong
//                           một phản ứng. Danh sách vật liệu của thế giới lại tự
//                           dài thêm (hợp kim mới) → không có giới hạn trên.
//  2. KHÁI NIỆM vật lý   — sắc bén, đòn bẩy, sức nổi, đàn hồi, cách nhiệt, nóng
//                           chảy, khử quặng, quay, áp suất, ma sát… Học được bằng
//                           QUAN SÁT sự kiện vật lý/hoá học thật trong thế giới, theo
//                           bất kỳ thứ tự nào (không có tiền đề cố định). Khái niệm
//                           quyết định sinh vật "nghĩ ra" được hình dạng nào.
//  3. THIẾT KẾ           — tổ hợp các bộ phận (vật liệu × hình dạng × kích thước).
//                           Hiệu năng (sức cắt, tầm bắn, sức nổi, nhiệt độ lò…) được
//                           TÍNH BẰNG CÔNG THỨC VẬT LÝ từ thuộc tính vật liệu, không
//                           tra bảng. Mỗi lần cải tiến tạo một thiết kế con → phả hệ
//                           phát minh, và chất lượng có thể tăng mãi.
//  4. QUY TRÌNH          — công thức hoá học (đầu vào, nhiệt độ, sản phẩm) học được
//                           từ một phản ứng THẬT đã quan sát (cháy rừng để lại than,
//                           dung nham gặp nước ra đá vỏ chai, thử nung quặng với than…).
//
// Sáng tạo = tìm kiếm tổ hợp có định hướng: sinh nhiều phương án (đột biến thiết
// kế cũ, lai ghép, ý tưởng mới), ƯỚC LƯỢNG bằng "mô hình tinh thần" (sai số giảm
// khi thông minh hơn), làm nguyên mẫu, THỬ NGHIỆM thật, giữ lại nếu tốt hơn.
// =============================================================================
#pragma once
#include "../phys/Material.h"
#include "../phys/MatterGrid.h"
#include "../core/Math.h"
#include "../core/Ecs.h"
#include <array>
#include <bitset>
#include <string>
#include <vector>

namespace wb {

class Simulation;
struct Mind;

// ---------------------------------------------------------------- Khái niệm
enum class Concept : uint8_t {
    Sharpness, Leverage, Buoyancy, Elasticity, Containment, Combustion, Insulation,
    Melting, Reduction, Alloying, Rotation, Pressure, Friction, Count
};
constexpr int kConceptCount = static_cast<int>(Concept::Count);
using ConceptSet = std::bitset<kConceptCount>;
const char* conceptName(Concept c);

// ---------------------------------------------------------------- Hình dạng bộ phận
enum class Shape : uint8_t {
    Lump,   // cục (đá ném, búa thô)
    Flake,  // mảnh ghè sắc
    Shaft,  // cán / gậy (đòn bẩy, tầm với)
    Point,  // mũi nhọn
    Blade,  // lưỡi
    Head,   // đầu nặng (búa, chuỳ, đối trọng)
    Cord,   // dây
    Sheet,  // tấm (da, vải, kim loại lá)
    Bowl,   // vật chứa rỗng
    Plank,  // tấm ván / khúc gỗ (bè)
    Wheel,  // bánh xe
    Limb,   // thân đàn hồi (cánh cung, lò xo)
    Pipe,   // ống (ống bễ, ống dẫn)
    Wall,   // tường dày (thành lò, cách nhiệt)
    Count
};
constexpr int kShapeCount = static_cast<int>(Shape::Count);
const char* shapeName(Shape s);

struct Part {
    MatId mat = kNoMat;
    Shape shape = Shape::Lump;
    float size = 1.0f;   // hệ số kích thước 0.3..3
    bool operator==(const Part& o) const { return mat == o.mat && shape == o.shape && std::abs(size - o.size) < 0.05f; }
};

// ---------------------------------------------------------------- Mục đích sử dụng
enum class Purpose : uint8_t {
    Cut, Dig, Pierce, Smash, Ranged, Siege, Carry, Float, Heat, FireStart, Warmth, Count
};
constexpr int kPurposeCount = static_cast<int>(Purpose::Count);
const char* purposeName(Purpose p);
const char* purposeNoun(Purpose p);   // "Rìu", "Cuốc", "Giáo"...

struct DesignProps {
    std::array<float, kPurposeCount> v{}; // hiệu năng theo mục đích (đơn vị vật lý riêng từng loại)
    float mass = 0;          // kg
    float durability = 0;    // độ bền
    float launchSpeed = 0;   // m/s (vũ khí tầm xa / máy bắn đá)
    float projMass = 0;      // kg đạn
    float heatLimit = 1e9f;  // nhiệt độ tối đa trước khi thành lò tự nóng chảy (giới hạn vật lý)
};

struct Design {
    int id = -1;
    std::string name;
    std::vector<Part> parts;
    DesignProps props;
    float quality = 1.0f;    // tay nghề chế tác — tăng dần qua các thế hệ cải tiến (không trần)
    Purpose primary = Purpose::Cut;
    int parent = -1;         // thiết kế gốc mà nó cải tiến từ (phả hệ phát minh)
    int generation = 0;
    int group = -1;          // nhóm của người phát minh
    std::string inventor;
    long tick = 0;
    MatId mainMat = kNoMat;  // vật liệu quyết định hiệu năng chính
    // Tay nghề cải thiện công cụ (lưỡi sắc hơn, cán chắc hơn...) nhưng KHÔNG vượt được giới
    // hạn vật lý: nhiệt độ lò không thể vượt điểm nóng chảy của chính thành lò.
    float value(Purpose p) const {
        float v = props.v[static_cast<int>(p)];
        if (p == Purpose::Heat || p == Purpose::FireStart || p == Purpose::Float)
            return std::min(v * std::min(quality, 1.1f), p == Purpose::Heat ? props.heatLimit : 1e9f);
        return v * quality;
    }
    std::string recipe(const MaterialRegistry& reg) const; // "Đá lửa (lưỡi) + Gỗ (cán)"
};

class DesignRegistry {
public:
    std::vector<Design> all;
    std::array<int, kPurposeCount> worldBest{};
    DesignRegistry() { worldBest.fill(-1); }
    void clear() { all.clear(); worldBest.fill(-1); }
    // Tìm thiết kế giống hệt (phát minh độc lập ở hai nơi) — trả -1 nếu chưa có
    int findSame(const std::vector<Part>& parts, Purpose p) const;
    int add(Design d);
    const Design& operator[](int id) const { return all[id]; }
};

// ---------------------------------------------------------------- Quy trình hoá học
struct Process {
    int id = -1;
    ReactionKind kind = ReactionKind::Firing;
    std::vector<MatId> inputs, outputs;
    float minTemp = 20;
    bool lowOxygen = false;
    std::string name;
    int group = -1;
    long tick = 0;
};

class ProcessRegistry {
public:
    std::vector<Process> all;
    void clear() { all.clear(); }
    // Từ một sự kiện phản ứng đã quan sát → quy trình (tạo mới nếu chưa ai biết).
    // Trả về -1 nếu loại phản ứng không tái tạo được có chủ đích.
    int fromEvent(const ReactionEvent& e, const MaterialRegistry& reg, int group, long tick, bool& isNew);
    const Process& operator[](int id) const { return all[id]; }
};

// ---------------------------------------------------------------- Tri thức cá nhân
struct Knowhow {
    ConceptSet concepts;
    std::vector<MatId> materials;
    std::vector<int> designs;
    std::vector<int> processes;
    std::array<int, kPurposeCount> best{};          // thiết kế tốt nhất biết làm, theo mục đích
    std::array<float, kPurposeCount> frustration{}; // nhu cầu chưa thoả → động lực sáng tạo
    long lastInvent = -100000, lastExperiment = -100000;
    int inventions = 0;      // số phát minh cá nhân
    Knowhow() { best.fill(-1); }

    bool knowsMat(MatId m) const;
    bool learnMat(MatId m);                  // true nếu là vật liệu mới với cá thể
    bool knowsDesign(int id) const;
    bool knowsProcess(int id) const;
    void learnDesign(const DesignRegistry& reg, int id);
    void learnProcess(int id);
    float bestValue(const DesignRegistry& reg, Purpose p) const {
        int b = best[static_cast<int>(p)];
        return b >= 0 ? reg[b].value(p) : 0.0f;
    }
    void frustrate(Purpose p, float amount) { float& f = frustration[static_cast<int>(p)]; f = std::min(3.0f, f + amount); }
};

// ---------------------------------------------------------------- Vật lý của thiết kế
DesignProps evaluateDesign(const MaterialRegistry& reg, const std::vector<Part>& parts);
float partMass(const MaterialRegistry& reg, const Part& p);
int partCost(const MaterialRegistry& reg, const Part& p);   // số đơn vị vật liệu cần
bool knappable(const MaterialDef& d);                       // ghè đẽo được (đá giòn, cứng)

// Năng lực chế tạo hiện có (giới hạn bởi công cụ & nhiệt độ lò mà cá thể/nhóm có)
struct Workshop {
    float cut = 0.5f;     // sức cắt tốt nhất (công cụ)
    float smash = 1.0f;   // sức đập (rèn)
    float heat = 20;      // nhiệt độ cao nhất đạt được (bếp/lò)
};
bool canFabricate(const MaterialRegistry& reg, const Part& p, const Workshop& ws);

// Tên tiếng Việt của vật liệu chủ đạo, dùng cho tên thời đại.
std::string eraName(const MaterialRegistry& reg, MatId m);

// ---------------------------------------------------------------- Sáng tạo (Creativity.cpp)
// Các hàm dưới đây cần toàn bộ Simulation và CHỈ được gọi ở pha tuần tự (act).
struct InventResult {
    bool tried = false, improved = false;
    int design = -1;
    float before = 0, after = 0;
};
Workshop workshopOf(Simulation& sim, Entity e);
// Sáng tạo gồm 2 bước:
//  1. conceiveIdea  — nghĩ ra phương án hứa hẹn nhất (từ MỌI vật liệu đã biết) → lưu vào Mind
//  2. realizeIdea   — khi đã gom đủ vật liệu: làm nguyên mẫu + thử nghiệm thật
bool conceiveIdea(Simulation& sim, Entity e, Purpose p);
MatId missingForIdea(Simulation& sim, Entity e);   // vật liệu còn thiếu cho ý tưởng (kNoMat = đủ)
InventResult realizeIdea(Simulation& sim, Entity e);
// Chế tạo lại một thiết kế đã biết (cần vật liệu + năng lực chế tạo).
bool craftDesign(Simulation& sim, Entity e, int designId, Purpose slot);
MatId missingForDesign(Simulation& sim, Entity e, int designId);
// ---- Người chơi tự thiết kế & chế tạo (cùng luật vật lý với AI) ----
struct CraftCheck {
    DesignProps props;
    float value = 0;                            // hiệu năng cho mục đích đã chọn (đã nhân tay nghề)
    float quality = 1;
    std::vector<std::pair<MatId, int>> need;    // vật liệu cần (gộp theo loại)
    MatId missing = kNoMat;                     // vật liệu còn thiếu
    int badPart = -1;                           // bộ phận không gia công được
    std::string problem;                        // lý do (tiếng Việt)
    int existing = -1;                          // đã có thiết kế giống hệt trên thế giới
    bool ok() const { return badPart < 0 && missing == kNoMat && value > 0; }
};
// Lý do không gia công được bộ phận với công cụ / lò hiện có ("" nếu làm được)
std::string fabricateProblem(const MaterialRegistry& reg, const Part& p, const Workshop& ws);
CraftCheck checkCraft(Simulation& sim, Entity e, const std::vector<Part>& parts, Purpose p);
// Chế tạo thiết kế người chơi tự ghép: tiêu vật liệu thật, ghi nhận thiết kế (đặt tên theo ngôn ngữ nhóm),
// trang bị vào ô của mục đích. Trả về id thiết kế (-1 nếu thất bại), msg = kết quả.
int playerCraft(Simulation& sim, Entity e, const std::vector<Part>& parts, Purpose p, std::string& msg);
// Vật liệu người chơi dùng được: đã biết + đang mang + trong kho nhóm (sắp theo số lượng có sẵn)
std::vector<MatId> craftableMaterials(Simulation& sim, Entity e);
// Gợi ý: bộ khung bộ phận hợp lý cho mục đích, chọn vật liệu tốt nhất đang có (tối ưu tham lam từng bộ phận)
std::vector<Part> suggestDesign(Simulation& sim, Entity e, Purpose p);
// Học từ quan sát: phản ứng hoá học / sự kiện vật lý nhìn thấy được.
void observeReaction(Simulation& sim, Entity e, const ReactionEvent& ev);
void observePhysEvent(Simulation& sim, Entity e, int physKind, MatId m);
// Trải nghiệm trực tiếp có thể dạy một khái niệm (xác suất theo trí tuệ).
void experience(Simulation& sim, Entity e, Concept c, float chance);
// Dạy học / thừa hưởng tri thức vật chất.
void shareKnowhow(Simulation& sim, Entity teacher, Entity learner, bool language);
void inheritKnowhow(Simulation& sim, Entity child, Entity parent);
Purpose mostFrustrated(const Knowhow& k);
Purpose pickInventPurpose(const Knowhow& k, Rng& rng);
// Kho vật liệu: túi cá nhân trước, rồi kho chung của nhóm.
int availableMat(Simulation& sim, Entity e, MatId m);
bool consumeMat(Simulation& sim, Entity e, MatId m, int units);
// Hiệu năng của vật đang mang theo cho mục đích p (0 nếu không có).
float heldValue(const Simulation& sim, Entity e, Purpose p);

} // namespace wb
