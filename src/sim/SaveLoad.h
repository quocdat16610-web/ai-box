// =============================================================================
// SaveLoad.h — Lưu / tải TOÀN BỘ thế giới vào một tệp nhị phân.
//
// Lưu đầy đủ trạng thái để tải lên chạy tiếp y như chưa từng dừng (kiểm thử:
// chạy 2 năm → lưu → tải → chạy 2 năm == chạy liền 4 năm, giống hệt từng con số):
//   * thế giới: địa hình, tài nguyên, lòng đất, lưới vật chất (nhiệt/lỏng/khí),
//     lịch sử phản ứng, khí quyển, thời tiết, vật phẩm, vật thể đang bay
//   * vật liệu (kể cả hợp kim sinh ra trong lúc chơi), thiết kế, quy trình
//   * mọi sinh vật: cơ thể, bộ gen, đồ đạc, quan hệ, BỘ NÃO (trí nhớ ngắn/dài hạn,
//     kế hoạch GOAP, kỹ năng, tri thức, ý tưởng, bộ sinh số ngẫu nhiên riêng)
//   * nhóm / bộ lạc / quốc gia: ngôn ngữ, văn hoá, luật, ngoại giao, kho, lãnh thổ
//   * loài (kể cả loài tiến hoá và loài tạo ở phòng thí nghiệm gen), biên niên sử
//
// Phần đầu tệp (SaveInfo) đọc được riêng để hiện danh sách ô lưu mà không tải cả thế giới.
// Tệp gắn với phiên bản định dạng; tệp của phiên bản khác bị từ chối kèm thông báo.
// =============================================================================
#pragma once
#include <memory>
#include <string>

namespace wb {

class Simulation;

struct SaveInfo {
    std::string title;         // vd: "Người nguyên thuỷ · Quần đảo"
    std::string savedAt;       // thời điểm lưu (giờ máy)
    long tick = 0;
    int population = 0, groups = 0, nations = 0, species = 0;
    int scenario = 0, worldType = 0, worldSize = 0;
    float camX = 0, camY = 0, camZoom = 1;   // vị trí camera để mở lại đúng chỗ
};

bool saveWorld(Simulation& sim, const std::string& path, const SaveInfo& info, std::string* error = nullptr);
std::unique_ptr<Simulation> loadWorld(const std::string& path, SaveInfo* info = nullptr, std::string* error = nullptr);
bool readSaveInfo(const std::string& path, SaveInfo& info);

} // namespace wb
