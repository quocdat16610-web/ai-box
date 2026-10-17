// =============================================================================
// Language.h — Ngôn ngữ thủ tục cho mỗi nền văn hoá.
//
// Mỗi nhóm có một bộ âm vị riêng (phụ âm đầu, nguyên âm, phụ âm cuối) và cấu
// trúc âm tiết. Tên người, tên bộ tộc, quốc gia, thần linh đều sinh từ bộ âm vị
// này → mỗi nền văn minh có "giọng" riêng nhận ra được (vd: "Grakthul", "Elúvien").
// Khi một nhóm tách ra, ngôn ngữ con bị "trôi" (drift): đổi vài âm vị → các ngôn
// ngữ cùng họ nghe na ná nhau, giống ngôn ngữ học thật.
// =============================================================================
#pragma once
#include "../core/Math.h"
#include <string>
#include <vector>

namespace wb {

class Language {
public:
    std::vector<std::string> onsets, vowels, codas;
    float codaChance = 0.4f;
    int minSyl = 1, maxSyl = 3;
    std::string familyName; // tên ngữ hệ

    // Sinh ngôn ngữ mới; "harsh" (thô ráp, cho Orc/sói) hay "soft" (mềm, cho Elf).
    static Language generate(Rng& rng, float harshness) {
        static const char* hardOn[] = {"g", "k", "kr", "gr", "th", "z", "d", "b", "r", "dr", "sk", "m"};
        static const char* softOn[] = {"l", "s", "n", "v", "f", "h", "el", "y", "m", "t", "ae", "r"};
        static const char* vow[] = {"a", "e", "i", "o", "u", "ai", "ou", "ia", "ă", "ô", "ư", "ê"};
        static const char* hardCo[] = {"k", "g", "r", "th", "sh", "x", "z", "nk", "rg", "m"};
        static const char* softCo[] = {"n", "l", "s", "th", "r", "en", "el", "is", "m", "y"};
        Language L;
        auto pick = [&](const char* const* arr, int n, int count, std::vector<std::string>& out) {
            for (int i = 0; i < count; ++i) out.push_back(arr[rng.irange(0, n - 1)]);
        };
        bool hard = rng.uniform() < harshness;
        pick(hard ? hardOn : softOn, 12, rng.irange(4, 7), L.onsets);
        pick(hard ? softOn : hardOn, 12, rng.irange(1, 2), L.onsets);
        pick(vow, 12, rng.irange(3, 5), L.vowels);
        pick(hard ? hardCo : softCo, 10, rng.irange(2, 4), L.codas);
        L.codaChance = rng.range(0.2f, 0.7f);
        L.minSyl = 1; L.maxSyl = rng.irange(2, 3);
        L.familyName = L.word(rng, 2);
        return L;
    }

    std::string syllable(Rng& rng) const {
        std::string s = onsets[rng.irange(0, static_cast<int>(onsets.size()) - 1)];
        s += vowels[rng.irange(0, static_cast<int>(vowels.size()) - 1)];
        if (rng.chance(codaChance)) s += codas[rng.irange(0, static_cast<int>(codas.size()) - 1)];
        return s;
    }

    std::string word(Rng& rng, int syl = 0) const {
        if (syl <= 0) syl = rng.irange(minSyl, maxSyl);
        std::string w;
        for (int i = 0; i < syl; ++i) w += syllable(rng);
        if (!w.empty() && w[0] >= 'a' && w[0] <= 'z') w[0] = static_cast<char>(w[0] - 'a' + 'A');
        return w;
    }

    // Ngôn ngữ con: thay ngẫu nhiên vài âm vị.
    Language drift(Rng& rng) const {
        Language c = *this;
        Language donor = generate(rng, 0.5f);
        if (!c.onsets.empty()) c.onsets[rng.irange(0, static_cast<int>(c.onsets.size()) - 1)] = donor.onsets[0];
        if (!c.vowels.empty()) c.vowels[rng.irange(0, static_cast<int>(c.vowels.size()) - 1)] = donor.vowels[0];
        if (rng.chance(0.5f)) c.codas.push_back(donor.codas[0]);
        c.codaChance = clamp01(c.codaChance + rng.gauss(0.1f));
        return c;
    }
};

} // namespace wb
