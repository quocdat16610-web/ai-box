// =============================================================================
// SpatialGrid.h — Phân vùng không gian dạng lưới đều (uniform grid).
//
// Mỗi tick, hệ thống mô phỏng xây lại lưới: mỗi ô (cell) chứa danh sách entity
// nằm trong đó. Truy vấn "các sinh vật trong bán kính r" chỉ cần quét vài ô lân
// cận thay vì O(N) toàn bộ → O(N·k) cho cả thế giới, cần thiết khi có hàng
// nghìn agent cùng "quan sát" môi trường.
// Lưu dạng CSR (counting sort) → không cấp phát động khi rebuild.
// =============================================================================
#pragma once
#include "Ecs.h"
#include "Math.h"
#include <vector>

namespace wb {

class SpatialGrid {
public:
    void init(int worldW, int worldH, int cellSize) {
        cellSize_ = cellSize;
        cols_ = (worldW + cellSize - 1) / cellSize;
        rows_ = (worldH + cellSize - 1) / cellSize;
        start_.assign(cols_ * rows_ + 1, 0);
    }

    // Xây lại lưới từ danh sách (entity, vị trí). Gọi một lần mỗi tick.
    void rebuild(const std::vector<std::pair<Entity, Vec2>>& items) {
        std::fill(start_.begin(), start_.end(), 0);
        cellOf_.resize(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            int c = cellIndex(items[i].second);
            cellOf_[i] = c;
            start_[c + 1]++;
        }
        for (size_t c = 1; c < start_.size(); ++c) start_[c] += start_[c - 1];
        entries_.resize(items.size());
        cursor_.assign(start_.begin(), start_.end() - 1);
        for (size_t i = 0; i < items.size(); ++i) entries_[cursor_[cellOf_[i]]++] = items[i];
    }

    // Gọi f(entity, pos) cho mọi entity trong hình vuông bao quanh vòng tròn bán kính r.
    // (Hàm gọi tự lọc theo khoảng cách thật nếu cần.)
    template <class F> void query(Vec2 center, float r, F&& f) const {
        int x0 = std::max(0, static_cast<int>((center.x - r) / cellSize_));
        int y0 = std::max(0, static_cast<int>((center.y - r) / cellSize_));
        int x1 = std::min(cols_ - 1, static_cast<int>((center.x + r) / cellSize_));
        int y1 = std::min(rows_ - 1, static_cast<int>((center.y + r) / cellSize_));
        float r2 = r * r;
        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx) {
                int c = cy * cols_ + cx;
                for (int k = start_[c]; k < start_[c + 1]; ++k)
                    if (dist2(entries_[k].second, center) <= r2) f(entries_[k].first, entries_[k].second);
            }
    }

private:
    int cellIndex(Vec2 p) const {
        int cx = std::clamp(static_cast<int>(p.x) / cellSize_, 0, cols_ - 1);
        int cy = std::clamp(static_cast<int>(p.y) / cellSize_, 0, rows_ - 1);
        return cy * cols_ + cx;
    }
    int cellSize_ = 8, cols_ = 1, rows_ = 1;
    std::vector<int> start_, cursor_, cellOf_;
    std::vector<std::pair<Entity, Vec2>> entries_;
};

} // namespace wb
