// Text.cpp — Nạp font Unicode (tiếng Việt) hoặc bỏ dấu khi không có.
#include "Text.h"
#include <vector>
#include <cstring>
#include <fstream>
#include <iterator>

namespace wb::ui {

namespace {
Font gFont;
bool gHasFont = false;

// Giải mã một ký tự UTF-8 → codepoint, trả về số byte đã đọc.
int decodeUtf8(const char* s, int& cp) {
    unsigned char c = static_cast<unsigned char>(s[0]);
    if (c < 0x80) { cp = c; return 1; }
    if ((c >> 5) == 6 && s[1]) { cp = ((c & 0x1F) << 6) | (s[1] & 0x3F); return 2; }
    if ((c >> 4) == 14 && s[1] && s[2]) { cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); return 3; }
    if ((c >> 3) == 30 && s[1] && s[2] && s[3]) { cp = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); return 4; }
    cp = '?';
    return 1;
}

// Bảng ký tự tiếng Việt có dấu → chữ không dấu.
char baseLetter(int cp) {
    static const char* groups[] = {
        "aàáảãạăằắẳẵặâầấẩẫậ", "AÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬ", "eèéẻẽẹêềếểễệ", "EÈÉẺẼẸÊỀẾỂỄỆ",
        "iìíỉĩị", "IÌÍỈĨỊ", "oòóỏõọôồốổỗộơờớởỡợ", "OÒÓỎÕỌÔỒỐỔỖỘƠỜỚỞỠỢ",
        "uùúủũụưừứửữự", "UÙÚỦŨỤƯỪỨỬỮỰ", "yỳýỷỹỵ", "YỲÝỶỸỴ", "dđ", "DĐ",
    };
    for (const char* g : groups) {
        const char* p = g;
        while (*p) {
            int c;
            p += decodeUtf8(p, c);
            if (c == cp) return g[0];
        }
    }
    return '?';
}

std::vector<int> vietnameseCodepoints() {
    std::vector<int> cps;
    for (int c = 32; c < 127; ++c) cps.push_back(c);
    const char* extra =
        "àáảãạăằắẳẵặâầấẩẫậèéẻẽẹêềếểễệìíỉĩịòóỏõọôồốổỗộơờớởỡợùúủũụưừứửữựỳýỷỹỵđ"
        "ÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬÈÉẺẼẸÊỀẾỂỄỆÌÍỈĨỊÒÓỎÕỌÔỒỐỔỖỘƠỜỚỞỠỢÙÚỦŨỤƯỪỨỬỮỰỲÝỶỸỴĐ→←↑↓•…—–“”·×°²³½₀₁₂₃₄−≈≤≥⚗";
    const char* p = extra;
    while (*p) { int c; p += decodeUtf8(p, c); cps.push_back(c); }
    return cps;
}
} // namespace

std::string stripDiacritics(const std::string& s) {
    std::string out;
    const char* p = s.c_str();
    while (*p) {
        int cp;
        int n = decodeUtf8(p, cp);
        if (cp < 128) out += static_cast<char>(cp);
        else if (cp == 0x2192) out += "->";
        else if (cp == 0x2014 || cp == 0x2013 || cp == 0x2212) out += "-";
        else if (cp >= 0x2080 && cp <= 0x2089) out += static_cast<char>('0' + (cp - 0x2080));   // chỉ số dưới: CO₂ → CO2
        else if (cp == 0xB2) out += '2';
        else if (cp == 0xB3) out += '3';
        else if (cp == 0xB0) out += "o";   // °C → oC
        else out += baseLetter(cp);
        p += n;
    }
    return out;
}

void initText(float baseSize) {
    static const char* candidates[] = {
        "assets/font.ttf",
        "/system/fonts/Roboto-Regular.ttf",          // Android
        "/system/fonts/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", // Linux
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:/Windows/Fonts/arial.ttf",                 // Windows
        "C:/Windows/Fonts/segoeui.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf", // macOS
        "/Library/Fonts/Arial Unicode.ttf",
    };
    std::vector<int> cps = vietnameseCodepoints();
    for (const char* path : candidates) {
        // Tự đọc file bằng C++ chuẩn: trên Android, fopen của raylib bị chuyển hướng
        // vào thư mục assets/internal nên không mở được /system/fonts.
        std::ifstream in(path, std::ios::binary);
        if (!in) continue;
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (data.size() < 1024) continue;
        gFont = LoadFontFromMemory(".ttf", data.data(), static_cast<int>(data.size()), static_cast<int>(baseSize * 2), cps.data(),
                                   static_cast<int>(cps.size()));
        if (gFont.texture.id != 0 && gFont.glyphCount > 100) {
            SetTextureFilter(gFont.texture, TEXTURE_FILTER_BILINEAR);
            gHasFont = true;
            TraceLog(LOG_INFO, "WorldBox: dùng font %s", path);
            return;
        }
    }
    gFont = GetFontDefault();
    gHasFont = false;
    TraceLog(LOG_WARNING, "WorldBox: không tìm thấy font Unicode, chữ sẽ được bỏ dấu");
}

void shutdownText() {
    if (gHasFont) UnloadFont(gFont);
    gHasFont = false;
}

bool hasUnicodeFont() { return gHasFont; }

void drawText(const std::string& s, float x, float y, float size, Color c) {
    if (gHasFont) DrawTextEx(gFont, s.c_str(), {x, y}, size, 0.5f, c);
    else DrawTextEx(GetFontDefault(), stripDiacritics(s).c_str(), {x, y}, size, size / 10.0f, c);
}

float measureText(const std::string& s, float size) {
    if (gHasFont) return MeasureTextEx(gFont, s.c_str(), size, 0.5f).x;
    return MeasureTextEx(GetFontDefault(), stripDiacritics(s).c_str(), size, size / 10.0f).x;
}

} // namespace wb::ui
