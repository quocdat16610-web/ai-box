// =============================================================================
// Speech.h — CẢM XÚC và LỜI NÓI của sinh vật.
//
// Cảm xúc nảy sinh từ trạng thái thật (bị đe doạ → sợ, bị tấn công + nóng tính →
// giận, nhu cầu thoả mãn → vui, đạt ước mơ / vừa hiểu ra điều mới → tự hào...).
//
// Lời nói: mỗi câu là một chuỗi Ý NGHĨA (Meaning) — "ở kia có thức ăn, lại đây!".
// Từ vựng KHÔNG cố định: từ cho mỗi ý nghĩa được sinh từ bộ âm vị của chính ngôn
// ngữ nhóm (Language.h) → mỗi tộc một thứ tiếng; nhóm tách ra thì ngôn ngữ trôi
// dạt nên từ vựng hai bên nghe na ná nhau (từ cùng gốc). Trật tự từ (SVO / SOV)
// cũng là đặc điểm riêng của từng ngôn ngữ. Trước khi phát minh "Ngôn ngữ" thì
// chỉ có tiếng kêu đơn âm (tiền ngôn ngữ); thú vật thì hú, gầm, kêu.
// Trình thông dịch dịch ngược chuỗi ý nghĩa sang tiếng Việt kèm sắc thái cảm xúc.
// =============================================================================
#pragma once
#include "../civ/Language.h"
#include "../core/Ecs.h"
#include <cctype>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

namespace wb {

enum class Emotion : uint8_t { Calm, Joy, Sadness, Fear, Anger, Love, Curiosity, Pride, Count };
inline const char* emotionName(Emotion e) {
    static const char* n[] = {"bình thản", "vui mừng", "buồn bã", "sợ hãi", "giận dữ", "trìu mến", "tò mò", "tự hào"};
    return n[static_cast<int>(e)];
}

enum class Meaning : uint8_t {
    I, You, We, Food, Danger, Come, Go, Here, There, Good, Bad, Happy, Sad, Afraid, Angry, Love,
    Know, Fire, Water, Stone, Tree, Ore, Home, Build, Hunt, Make, Learn, God, Enemy, Friend,
    Child, Want, Leader, Many, Not, Teach, Hello, Help, Count
};
constexpr int kMeaningCount = static_cast<int>(Meaning::Count);
inline const char* meaningGloss(Meaning m) {
    static const char* n[] = {"tôi", "bạn", "chúng ta", "thức ăn", "nguy hiểm", "lại đây", "đi đi", "ở đây", "ở kia",
                              "tốt", "xấu", "vui", "buồn", "sợ", "giận", "yêu", "biết", "lửa", "nước", "đá", "cây",
                              "quặng", "nhà", "xây", "săn", "làm ra", "học", "thần linh", "kẻ thù", "bạn bè",
                              "con", "muốn", "thủ lĩnh", "nhiều", "không", "dạy", "chào", "giúp"};
    return n[static_cast<int>(m)];
}
inline bool meaningIsVerb(Meaning m) {
    switch (m) {
    case Meaning::Come: case Meaning::Go: case Meaning::Know: case Meaning::Love: case Meaning::Build: case Meaning::Hunt:
    case Meaning::Make: case Meaning::Learn: case Meaning::Want: case Meaning::Teach: case Meaning::Help: return true;
    default: return false;
    }
}

struct Utterance {
    std::vector<Meaning> meanings;
    Emotion emotion = Emotion::Calm;
    std::string text;          // câu nói bằng tiếng của chính nó
    std::string translation;   // bản dịch tiếng Việt
    long tick = -100000;
    bool call = false;         // tiếng kêu của thú / tiền ngôn ngữ (không có từ vựng)
    Entity to = NullEntity;
};

// Chữ ký ổn định của một ngôn ngữ (từ tên ngữ hệ): các ngôn ngữ cùng họ giữ cùng "gốc từ".
inline uint32_t languageRoot(const Language& L) {
    uint32_t h = 2166136261u;
    for (unsigned char c : L.familyName) h = (h ^ c) * 16777619u;
    return h;
}
// Trật tự từ: chủ – động – tân (SVO) hay chủ – tân – động (SOV)
inline bool languageSOV(const Language& L) { return (languageRoot(L) >> 7) & 1; }

// Từ của ngôn ngữ L cho ý nghĩa m (ổn định: cùng ngôn ngữ luôn ra cùng một từ; không có hai
// ý nghĩa trùng âm). Bảng từ vựng được sinh một lần cho mỗi bộ âm vị rồi ghi nhớ.
inline std::string lexicon(const Language& L, Meaning m) {
    if (L.onsets.empty() || L.vowels.empty()) return "a";
    uint32_t key = languageRoot(L);
    for (const auto& o : L.onsets) for (unsigned char ch : o) key = (key ^ ch) * 16777619u;
    for (const auto& v : L.vowels) for (unsigned char ch : v) key = (key ^ ch) * 16777619u;
    thread_local std::vector<std::pair<uint32_t, std::vector<std::string>>> cache;
    for (auto& [k, words] : cache) if (k == key) return words[static_cast<int>(m)];
    std::vector<std::string> words;
    for (int i = 0; i < kMeaningCount; ++i) {
        Meaning mi = static_cast<Meaning>(i);
        Rng r(languageRoot(L) * 2654435761u + static_cast<uint32_t>(i) * 40503u + 7);
        bool shortWord = mi == Meaning::I || mi == Meaning::You || mi == Meaning::Not || mi == Meaning::Here;
        std::string w;
        for (int attempt = 0; attempt < 12; ++attempt) {
            int syl = (shortWord && attempt < 4) ? 1 : r.irange(1, 2) + attempt / 4;
            w.clear();
            for (int k = 0; k < syl; ++k) w += L.syllable(r);
            if (std::find(words.begin(), words.end(), w) == words.end()) break;
        }
        words.push_back(w);
    }
    if (cache.size() > 256) cache.clear();
    cache.push_back({key, words});
    return cache.back().second[static_cast<int>(m)];
}

// Ghép câu theo ngữ pháp của ngôn ngữ; trả về chữ + bản dịch.
inline void composeUtterance(const Language& L, Utterance& u, bool fullLanguage) {
    std::vector<Meaning> order = u.meanings;
    if (fullLanguage && languageSOV(L)) {
        // SOV: đẩy động từ về cuối câu
        std::vector<Meaning> nonVerb, verb;
        for (Meaning m : order) (meaningIsVerb(m) ? verb : nonVerb).push_back(m);
        order = nonVerb;
        order.insert(order.end(), verb.begin(), verb.end());
    }
    if (!fullLanguage && order.size() > 2) order.resize(2);   // tiền ngôn ngữ: tối đa 2 "từ"
    std::string t, tr;
    for (size_t i = 0; i < order.size(); ++i) {
        std::string w = lexicon(L, order[i]);
        if (!fullLanguage) {
            // Lặp lại âm đầu như tiếng kêu (cắt theo ký tự UTF-8, không cắt đôi chữ có dấu)
            size_t cut = 0;
            for (int ch = 0; ch < 2 && cut < w.size(); ++ch) {
                ++cut;
                while (cut < w.size() && (static_cast<unsigned char>(w[cut]) & 0xC0) == 0x80) ++cut;
            }
            w += w.substr(0, cut);
        }
        if (!t.empty()) t += ' ';
        t += w;
    }
    if (!t.empty() && t[0] >= 'a' && t[0] <= 'z') t[0] = static_cast<char>(t[0] - 'a' + 'A');
    bool excl = u.emotion == Emotion::Joy || u.emotion == Emotion::Fear || u.emotion == Emotion::Anger || u.emotion == Emotion::Pride;
    t += excl ? "!" : (fullLanguage ? "." : "...");
    for (size_t i = 0; i < u.meanings.size(); ++i) { if (i) tr += ' '; tr += meaningGloss(u.meanings[i]); }
    // Viết hoa chữ đầu (kể cả chữ có dấu thường gặp đầu câu)
    static const std::pair<const char*, const char*> kUp[] = {{"ở", "Ở"}, {"đ", "Đ"}, {"ă", "Ă"}, {"ô", "Ô"}, {"ư", "Ư"}, {"ê", "Ê"}};
    bool done = false;
    for (auto& [lo, up] : kUp)
        if (tr.rfind(lo, 0) == 0) { tr = std::string(up) + tr.substr(std::strlen(lo)); done = true; break; }
    if (!done && !tr.empty()) tr[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(tr[0])));
    u.text = t;
    u.translation = "\"" + tr + "\" (" + emotionName(u.emotion) + (fullLanguage ? "" : ", tiền ngôn ngữ") + ")";
}

// Tiếng kêu của thú (không có ngôn ngữ): hú / gầm / kêu theo cảm xúc.
inline void composeCall(Utterance& u, int rootSpecies, bool big) {
    static const char* fear[] = {"Aoo-íí!", "Ríí!", "Héé!"};
    static const char* anger[] = {"Grrừ!", "Ggrừừ!", "Ròooa!"};
    static const char* calm[] = {"Aoooo...", "Hừm...", "Ứm-ứm..."};
    int k = rootSpecies % 3;
    const char* s = u.emotion == Emotion::Fear ? fear[k] : (u.emotion == Emotion::Anger ? anger[big ? 2 : k] : calm[k]);
    u.text = s;
    std::string gl = u.meanings.empty() ? "" : meaningGloss(u.meanings[0]);
    u.translation = "(tiếng kêu " + std::string(emotionName(u.emotion)) + (gl.empty() ? "" : " — có lẽ: \"" + gl + "\"") + ")";
    u.call = true;
}

} // namespace wb
