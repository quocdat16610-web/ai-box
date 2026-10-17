// =============================================================================
// Text.h — Vẽ chữ tiếng Việt.
//
// Font mặc định của raylib chỉ có ASCII. Ta thử nạp một font TTF hệ thống có
// đủ dấu tiếng Việt (Android: /system/fonts/Roboto-Regular.ttf, Linux: DejaVu,
// Windows: Arial...) cùng bảng mã các ký tự tiếng Việt. Nếu không tìm thấy font
// nào, chữ được tự động bỏ dấu ("Bộ lạc" → "Bo lac") để vẫn đọc được.
// =============================================================================
#pragma once
#include "raylib.h"
#include <string>

namespace wb::ui {

void initText(float baseSize);
void shutdownText();
void drawText(const std::string& s, float x, float y, float size, Color c);
float measureText(const std::string& s, float size);
bool hasUnicodeFont();
std::string stripDiacritics(const std::string& s);

} // namespace wb::ui
