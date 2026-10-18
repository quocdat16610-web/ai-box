// =============================================================================
// Voice.h — Giọng nói tổng hợp: nghe sinh vật NÓI bằng chính ngôn ngữ của chúng.
//
// Không dùng file âm thanh. Mỗi câu (Utterance::text) được đọc thành âm thanh
// bằng mô hình nguồn – bộ lọc của thanh quản người:
//   * Nguyên âm: xung dây thanh (cao độ f0) đi qua 3 bộ cộng hưởng formant F1/F2/F3
//     (a, e, i, o, u, ă, ô, ư, ê... mỗi âm một cặp formant riêng) → nghe ra "a", "i"...
//   * Phụ âm bật (k, t, d, g, b, p): khoảng lặng + tiếng nổ ngắn; phụ âm xát
//     (s, z, x, f, v, h, th): tiếng ồn lọc; âm mũi (m, n): tiếng ngân trầm; âm lướt (l, r, y).
//   * Giọng theo cơ thể: loài to → trầm & formant thấp; con cái / trẻ con → cao hơn.
//   * Cảm xúc đổi ngữ điệu: vui → cao & nảy; sợ → cao, nhanh, run; giận → khàn, mạnh;
//     buồn → trầm, chậm; trìu mến → nhẹ, có hơi thở.
// Thú vật không có ngôn ngữ thì hú, gầm, kêu (cùng bộ tổng hợp, kiểu âm khác).
// =============================================================================
#pragma once
#include "raylib.h"
#include "../ai/Speech.h"
#include <vector>

namespace wb {

class Simulation;

struct VoiceProfile {
    float f0 = 130;          // cao độ cơ bản (Hz)
    float formant = 1.0f;    // hệ số formant (thanh quản to → < 1)
    float speed = 1.0f;      // tốc độ nói
    float rasp = 0.0f;       // độ khàn (tiếng ồn trộn vào)
    float breath = 0.0f;     // hơi thở
    float tremolo = 0.0f;    // run giọng
    float volume = 0.8f;
    float rise = 0.0f;       // lên giọng cuối câu
    bool howl = false;       // tiếng hú (sói)
    bool growl = false;      // tiếng gầm
};

// Giọng của một cá thể ở trạng thái cảm xúc hiện tại.
VoiceProfile voiceOf(Simulation& sim, Entity e, Emotion em);

class VoiceSystem {
public:
    void init();
    void shutdown();
    bool ready() const { return ready_; }
    // Đọc một câu thành tiếng (không chặn: phát song song tối đa vài câu).
    void speak(const std::string& text, const VoiceProfile& vp);
    void update();   // giải phóng các âm đã phát xong
private:
    struct Playing { Sound snd; };
    std::vector<Playing> playing_;
    bool ready_ = false;
};

// Tổng hợp mẫu âm (PCM 16-bit, 22050 Hz) — tách riêng để kiểm thử không cần thiết bị âm thanh.
std::vector<short> synthesizeSpeech(const std::string& text, const VoiceProfile& vp, int sampleRate = 22050);

} // namespace wb
