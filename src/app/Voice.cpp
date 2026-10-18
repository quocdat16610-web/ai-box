// =============================================================================
// Voice.cpp — Tổng hợp tiếng nói nguồn–bộ lọc (formant synthesis).
// =============================================================================
#include "Voice.h"
#include "../sim/Simulation.h"
#include <cmath>
#include <cstring>

namespace wb {

namespace {

constexpr float kPi = 3.14159265f;

// Bộ cộng hưởng 2 cực (formant): khuếch đại dải tần quanh f với độ rộng bw.
struct Resonator {
    float y1 = 0, y2 = 0, a = 0, b1 = 0, b2 = 0;
    void set(float f, float bw, float fs) {
        float r = std::exp(-kPi * bw / fs);
        b1 = 2 * r * std::cos(2 * kPi * f / fs);
        b2 = -r * r;
        a = 1 - r;
    }
    float tick(float x) { float y = a * x + b1 * y1 + b2 * y2; y2 = y1; y1 = y; return y; }
};

enum class Ph { Vowel, Plosive, Fricative, Nasal, Liquid, Pause };
struct Phone { Ph kind; float f1 = 500, f2 = 1500; bool voiced = true; float bright = 0.5f; float dur = 0.1f; };

bool vowelFormants(uint32_t cp, float& f1, float& f2) {
    switch (cp) {
    case 'a': case 0xE1: case 0xE0: case 0xE3: f1 = 730; f2 = 1090; return true;
    case 'e': case 0xE9: case 0xE8: f1 = 530; f2 = 1840; return true;
    case 'i': case 0xED: case 0xEC: f1 = 270; f2 = 2290; return true;
    case 'o': case 0xF3: case 0xF2: f1 = 570; f2 = 840; return true;
    case 'u': case 0xFA: case 0xF9: f1 = 300; f2 = 870; return true;
    case 0x103: f1 = 660; f2 = 1200; return true;   // ă
    case 0xE2: f1 = 600; f2 = 1250; return true;    // â
    case 0xEA: f1 = 400; f2 = 2000; return true;    // ê
    case 0xF4: f1 = 450; f2 = 800; return true;     // ô
    case 0x1A1: f1 = 500; f2 = 1300; return true;   // ơ
    case 0x1B0: f1 = 320; f2 = 1350; return true;   // ư
    case 0xFD: case 'y': f1 = 280; f2 = 2200; return true;
    default: return false;
    }
}

// Tách UTF-8 thành dãy âm vị.
std::vector<Phone> parse(const std::string& text) {
    std::vector<Phone> out;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        uint32_t cp = c;
        int len = 1;
        if (c >= 0xF0) { cp = c & 0x07; len = 4; }
        else if (c >= 0xE0) { cp = c & 0x0F; len = 3; }
        else if (c >= 0xC0) { cp = c & 0x1F; len = 2; }
        for (int k = 1; k < len && i + k < text.size(); ++k) cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        i += len;
        if (cp >= 'A' && cp <= 'Z') cp += 32;
        if (cp == 0x102) cp = 0x103; if (cp == 0xD4) cp = 0xF4; if (cp == 0x1AF) cp = 0x1B0; if (cp == 0xCA) cp = 0xEA;
        Phone p;
        float f1, f2;
        if (vowelFormants(cp, f1, f2)) { p.kind = Ph::Vowel; p.f1 = f1; p.f2 = f2; p.dur = 0.11f; out.push_back(p); continue; }
        switch (cp) {
        case 'k': case 'c': case 'q': case 't': case 'p': p.kind = Ph::Plosive; p.voiced = false; p.bright = cp == 'p' ? 0.3f : 0.8f; p.dur = 0.05f; break;
        case 'g': case 'd': case 'b': p.kind = Ph::Plosive; p.voiced = true; p.bright = cp == 'b' ? 0.25f : 0.55f; p.dur = 0.045f; break;
        case 's': case 'x': p.kind = Ph::Fricative; p.voiced = false; p.bright = 0.95f; p.dur = 0.08f; break;
        case 'z': case 'j': p.kind = Ph::Fricative; p.voiced = true; p.bright = 0.85f; p.dur = 0.07f; break;
        case 'f': case 'h': p.kind = Ph::Fricative; p.voiced = false; p.bright = 0.35f; p.dur = 0.06f; break;
        case 'v': p.kind = Ph::Fricative; p.voiced = true; p.bright = 0.35f; p.dur = 0.06f; break;
        case 'm': p.kind = Ph::Nasal; p.f1 = 250; p.f2 = 1000; p.dur = 0.06f; break;
        case 'n': p.kind = Ph::Nasal; p.f1 = 260; p.f2 = 1500; p.dur = 0.055f; break;
        case 'l': p.kind = Ph::Liquid; p.f1 = 360; p.f2 = 1300; p.dur = 0.05f; break;
        case 'r': p.kind = Ph::Liquid; p.f1 = 450; p.f2 = 1200; p.bright = 1; p.dur = 0.06f; break;   // r rung
        case 'w': p.kind = Ph::Liquid; p.f1 = 300; p.f2 = 700; p.dur = 0.04f; break;
        case ' ': case '-': p.kind = Ph::Pause; p.dur = 0.08f; break;
        case ',': p.kind = Ph::Pause; p.dur = 0.15f; break;
        case '.': case '!': case '?': p.kind = Ph::Pause; p.dur = 0.12f; break;
        default: continue;
        }
        // "th", "sh", "kh": bật hơi
        if (cp == 'h' && !out.empty() && (out.back().kind == Ph::Plosive || out.back().kind == Ph::Fricative)) { out.back().dur += 0.03f; out.back().bright *= 0.8f; continue; }
        out.push_back(p);
    }
    return out;
}

} // namespace

std::vector<short> synthesizeSpeech(const std::string& text, const VoiceProfile& vp, int fs) {
    std::vector<Phone> ph = parse(text);
    std::vector<float> buf;
    if (ph.empty()) return {};
    float total = 0;
    for (auto& p : ph) total += p.dur / vp.speed;
    buf.reserve(static_cast<size_t>((total + 0.3f) * fs));
    Resonator R1, R2, R3, Rn;
    float phase = 0, curF1 = ph[0].f1, curF2 = ph[0].f2;
    uint32_t noise = 0x12345678u;
    auto rnd = [&]() { noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5; return (noise & 0xFFFF) / 32768.0f - 1.0f; };
    float t = 0, nPrev = 0;
    for (size_t pi = 0; pi < ph.size(); ++pi) {
        const Phone& p = ph[pi];
        int n = static_cast<int>(p.dur / vp.speed * fs);
        float tgtF1 = p.f1 * vp.formant, tgtF2 = p.f2 * vp.formant;
        for (int s = 0; s < n; ++s, t += 1.0f / fs) {
            float u = static_cast<float>(s) / n;                    // vị trí trong âm vị 0..1
            float prog = t / std::max(0.05f, total);                // vị trí trong câu
            // Ngữ điệu: hạ dần cuối câu (declination); cảm xúc có thể lên giọng cuối câu
            float f0 = vp.f0 * (1.12f - 0.22f * prog + vp.rise * std::max(0.0f, prog - 0.7f) * 2.0f);
            f0 *= 1.0f + 0.02f * std::sin(t * 2 * kPi * 5.5f) + vp.tremolo * std::sin(t * 2 * kPi * 9.0f);
            if (vp.howl) f0 = vp.f0 * (0.8f + 0.7f * std::sin(std::min(1.0f, prog * 1.2f) * kPi));   // tiếng hú: vút lên rồi hạ
            phase += f0 / fs;
            if (phase >= 1) phase -= 1;
            float glott = (phase < 0.6f ? phase / 0.6f : (1 - phase) / 0.4f) * 2 - 1;   // xung dây thanh dạng tam giác lệch
            float nz = rnd();
            // Chuyển formant mượt giữa các âm (đồng cấu âm)
            float k = 1.0f - std::exp(-1.0f / (0.012f * fs));
            curF1 += (tgtF1 - curF1) * k; curF2 += (tgtF2 - curF2) * k;
            if ((s & 31) == 0) {
                R1.set(curF1, 90, static_cast<float>(fs)); R2.set(curF2, 130, static_cast<float>(fs)); R3.set(2600 * vp.formant, 200, static_cast<float>(fs));
                Rn.set(1200 + p.bright * 5000, 1500 + p.bright * 2500, static_cast<float>(fs));
            }
            float env = std::min(1.0f, std::min(u * 12.0f, (1 - u) * 10.0f));
            float y = 0;
            float src = glott * (1 - vp.breath) + nz * (vp.breath + vp.rasp);
            switch (p.kind) {
            case Ph::Vowel:
                y = (R1.tick(src) * 1.0f + R2.tick(src) * 0.6f + R3.tick(src) * 0.25f) * 3.0f * env;
                break;
            case Ph::Liquid: {
                float am = p.bright > 0.9f ? 0.55f + 0.45f * std::sin(t * 2 * kPi * 28) : 1.0f;   // r rung lưỡi
                y = (R1.tick(src) + R2.tick(src) * 0.4f) * 2.2f * env * am;
                break;
            }
            case Ph::Nasal: y = R1.tick(glott) * 2.6f * env; R2.tick(0); break;
            case Ph::Fricative: {
                float hiss = nz - nPrev * (0.2f + p.bright * 0.75f);   // lọc thông cao đơn giản
                y = Rn.tick(hiss) * 1.5f * env + (p.voiced ? R1.tick(glott) * 0.8f * env : 0);
                break;
            }
            case Ph::Plosive:
                if (u < 0.55f) y = p.voiced ? R1.tick(glott) * 0.25f : 0;                       // tắc
                else y = Rn.tick(nz) * 2.5f * std::exp(-(u - 0.55f) * 18.0f) + (p.voiced ? R1.tick(glott) * 0.6f : 0);  // bật
                break;
            case Ph::Pause: y = 0; R1.tick(0); R2.tick(0); break;
            }
            if (vp.growl) y = y * (0.6f + 0.4f * std::sin(t * 2 * kPi * 31)) + nz * 0.08f * env;
            nPrev = nz;
            buf.push_back(y);
        }
    }
    // Chuẩn hoá âm lượng
    float peak = 1e-4f;
    for (float v : buf) peak = std::max(peak, std::fabs(v));
    std::vector<short> pcm(buf.size());
    float g = vp.volume * 0.9f / peak;
    for (size_t i = 0; i < buf.size(); ++i) pcm[i] = static_cast<short>(std::clamp(buf[i] * g, -1.0f, 1.0f) * 32000);
    return pcm;
}

VoiceProfile voiceOf(Simulation& sim, Entity e, Emotion em) {
    VoiceProfile vp;
    const Body& b = sim.reg.get<Body>(e);
    const SpeciesDef& sd = sim.species[b.species];
    int root = sim.species.root(b.species);
    float size = std::clamp(sd.size, 0.3f, 2.2f);
    bool child = b.ageTicks < sd.adultYears * kTicksPerYear;
    vp.f0 = (b.female ? 205.0f : 120.0f) / std::sqrt(size);
    vp.formant = 1.0f / std::pow(size, 0.25f);
    if (child) { vp.f0 *= 1.6f; vp.formant *= 1.15f; }
    switch (root) {
    case sp::Elf: vp.f0 *= 1.12f; vp.breath = 0.08f; break;
    case sp::Orc: vp.f0 *= 0.8f; vp.rasp = 0.12f; vp.formant *= 0.92f; break;
    case sp::Dwarf: vp.f0 *= 0.85f; vp.formant *= 0.95f; break;
    case sp::Goblin: vp.f0 *= 1.45f; vp.speed = 1.25f; vp.rasp = 0.06f; break;
    case sp::Troll: vp.f0 *= 0.6f; vp.rasp = 0.2f; vp.growl = true; break;
    case sp::Wolf: vp.f0 = 380; vp.howl = true; vp.speed = 0.5f; break;
    case sp::Bear: vp.f0 = 70; vp.growl = true; vp.rasp = 0.25f; vp.speed = 0.7f; break;
    case sp::Deer: vp.f0 = 520; vp.speed = 1.4f; break;
    case sp::Rabbit: vp.f0 = 900; vp.speed = 1.8f; vp.volume = 0.4f; break;
    default: break;
    }
    // Hạt giống giọng riêng từng cá thể
    float jitter = (hash32(entityIndex(e) * 977u) % 1000) / 1000.0f;
    vp.f0 *= 0.9f + jitter * 0.2f;
    switch (em) {
    case Emotion::Joy: vp.f0 *= 1.2f; vp.rise = 0.2f; vp.speed *= 1.1f; break;
    case Emotion::Fear: vp.f0 *= 1.4f; vp.speed *= 1.35f; vp.tremolo = 0.05f; vp.rise = 0.3f; break;
    case Emotion::Anger: vp.f0 *= 0.95f; vp.rasp += 0.15f; vp.volume = 1.0f; vp.growl = vp.growl || !sd.dexterous; break;
    case Emotion::Sadness: vp.f0 *= 0.85f; vp.speed *= 0.8f; vp.breath += 0.1f; vp.volume = 0.6f; break;
    case Emotion::Love: vp.breath += 0.15f; vp.volume = 0.6f; vp.speed *= 0.9f; break;
    case Emotion::Curiosity: vp.rise = 0.35f; break;
    case Emotion::Pride: vp.volume = 1.0f; vp.speed *= 0.95f; break;
    default: break;
    }
    return vp;
}

void VoiceSystem::init() {
    if (!IsAudioDeviceReady()) InitAudioDevice();
    ready_ = IsAudioDeviceReady();
}

void VoiceSystem::shutdown() {
    for (auto& p : playing_) UnloadSound(p.snd);
    playing_.clear();
    if (ready_) CloseAudioDevice();
    ready_ = false;
}

void VoiceSystem::speak(const std::string& text, const VoiceProfile& vp) {
    if (!ready_ || text.empty()) return;
    if (playing_.size() >= 4) { StopSound(playing_.front().snd); UnloadSound(playing_.front().snd); playing_.erase(playing_.begin()); }
    std::vector<short> pcm = synthesizeSpeech(text, vp);
    if (pcm.empty()) return;
    Wave w{};
    w.frameCount = static_cast<unsigned int>(pcm.size());
    w.sampleRate = 22050;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = pcm.data();
    Sound s = LoadSoundFromWave(w);   // raylib sao chép dữ liệu → pcm có thể giải phóng
    PlaySound(s);
    playing_.push_back({s});
}

void VoiceSystem::update() {
    for (size_t i = 0; i < playing_.size();) {
        if (!IsSoundPlaying(playing_[i].snd)) { UnloadSound(playing_[i].snd); playing_.erase(playing_.begin() + static_cast<long>(i)); }
        else ++i;
    }
}

} // namespace wb
