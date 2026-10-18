// =============================================================================
// Neural.h — MẠNG NEURON của từng bộ não.
//
// Mỗi sinh vật mang một mạng perceptron nhiều lớp riêng:
//     20 đầu vào  → 16 neuron ẩn (tanh) → 1 đầu ra cho MỖI mục tiêu (tanh)
// Đầu vào là những gì nó cảm nhận về bản thân & thế giới: 8 nhu cầu, mối đe doạ,
// máu, số đồng loại quanh mình, có nhóm không, tuổi, trí tuệ, tính cách, đồ ăn
// mang theo, hạnh phúc. Đầu ra là "trực giác" thích/ghét từng mục tiêu, cộng vào
// điểm Utility — nên hai cá thể cùng hoàn cảnh có thể chọn khác nhau tuỳ những gì
// bộ não của chúng đã HỌC được.
//
// Học (trong đời): mỗi khi theo đuổi xong một mục tiêu, bộ não nhận phần thưởng
// = nhu cầu được thoả mãn + hạnh phúc tăng + kế hoạch thành công (âm nếu thất bại)
// rồi lan truyền ngược (backpropagation) cập nhật trọng số cho mục tiêu đó.
// Tiến hoá (qua thế hệ): con sinh ra mang trọng số trung bình của bố & mẹ + đột
// biến Gauss → chọn lọc tự nhiên giữ lại những bộ não "trực giác tốt".
// =============================================================================
#pragma once
#include "../core/Math.h"
#include <array>
#include <cmath>

namespace wb {

constexpr int kNnIn = 20, kNnHid = 16, kNnOut = 20;   // kNnOut = số mục tiêu (kiểm tra trong Brain.cpp)

struct NeuralNet {
    std::array<float, kNnHid * (kNnIn + 1)> w1{};   // ẩn ← vào (+ bias)
    std::array<float, kNnOut * (kNnHid + 1)> w2{};  // ra ← ẩn (+ bias)
    std::array<float, kNnIn> in{};                  // kích hoạt gần nhất (để hiển thị)
    std::array<float, kNnHid> hid{};
    std::array<float, kNnOut> out{};
    uint32_t updates = 0;                           // số lần đã học

    void randomize(Rng& rng, float scale = 0.35f) {
        for (float& w : w1) w = rng.gauss(scale);
        for (float& w : w2) w = rng.gauss(scale * 0.6f);
    }
    // Di truyền: trung bình hai bộ não cha mẹ + đột biến.
    void inherit(const NeuralNet* a, const NeuralNet* b, Rng& rng, float mutation) {
        for (size_t i = 0; i < w1.size(); ++i) {
            float v = a && b ? (a->w1[i] + b->w1[i]) * 0.5f : (a ? a->w1[i] : (b ? b->w1[i] : rng.gauss(0.35f)));
            w1[i] = v + rng.gauss(mutation);
        }
        for (size_t i = 0; i < w2.size(); ++i) {
            float v = a && b ? (a->w2[i] + b->w2[i]) * 0.5f : (a ? a->w2[i] : (b ? b->w2[i] : rng.gauss(0.2f)));
            w2[i] = v + rng.gauss(mutation);
        }
    }
    void forward(const std::array<float, kNnIn>& x) {
        in = x;
        for (int j = 0; j < kNnHid; ++j) {
            const float* w = &w1[j * (kNnIn + 1)];
            float z = w[kNnIn];
            for (int i = 0; i < kNnIn; ++i) z += w[i] * x[i];
            hid[j] = std::tanh(z);
        }
        for (int k = 0; k < kNnOut; ++k) {
            const float* w = &w2[k * (kNnHid + 1)];
            float z = w[kNnHid];
            for (int j = 0; j < kNnHid; ++j) z += w[j] * hid[j];
            out[k] = std::tanh(z);
        }
    }
    // Học tăng cường cho MỘT đầu ra: đẩy đầu ra k về phía (giá trị hiện tại + thưởng).
    void reinforce(const std::array<float, kNnIn>& x, int k, float reward, float lr) {
        forward(x);
        float o = out[k];
        float target = std::clamp(o + reward, -1.0f, 1.0f);
        float delta = (target - o) * (1.0f - o * o);
        float* w = &w2[k * (kNnHid + 1)];
        std::array<float, kNnHid> dh;
        for (int j = 0; j < kNnHid; ++j) dh[j] = delta * w[j] * (1.0f - hid[j] * hid[j]);
        for (int j = 0; j < kNnHid; ++j) w[j] += lr * delta * hid[j];
        w[kNnHid] += lr * delta;
        for (int j = 0; j < kNnHid; ++j) {
            float* v = &w1[j * (kNnIn + 1)];
            for (int i = 0; i < kNnIn; ++i) v[i] += lr * dh[j] * x[i];
            v[kNnIn] += lr * dh[j];
        }
        updates++;
    }
};

} // namespace wb
