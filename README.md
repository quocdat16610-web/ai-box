# WorldBox AI 🌍

Game sandbox mô phỏng thế giới **pixel art 2D top-down** lấy cảm hứng từ *WorldBox*, viết bằng **C++17 + raylib**,
chạy trên **PC và Android (APK)**. Bạn là Thần: nặn địa hình, gọi mưa, giáng thiên thạch, thả sinh vật…
và quan sát chúng **tự suy nghĩ, học hỏi, tiến hoá** thành bộ tộc, làng mạc và quốc gia.

> Điểm khác biệt chính: **mỗi sinh vật có một bộ não AI riêng** (Utility AI + GOAP + trí nhớ ngắn/dài hạn +
> tính cách + kỹ năng). Không có công nghệ nào được cho sẵn — lửa, công cụ đá, ngôn ngữ, nông nghiệp, chữ viết,
> luật pháp… đều do chính sinh vật **tự phát minh** từ những gì chúng quan sát, rồi dạy lại cho nhau và cho con cháu.

## Tính năng

* **Thế giới**: địa hình sinh thủ tục (biển, cát, đồng cỏ, rừng, rừng rậm, sa mạc, thảo nguyên, đồi, núi, tuyết,
  lãnh nguyên, đầm lầy, dung nham), tài nguyên (gỗ, đá, quặng, quả), mùa, mưa di chuyển, sét, hạn hán, cháy rừng.
* **Chủng tộc & sinh vật**: Người, Elf, Orc, Dwarf, Sói, Hươu, Thỏ, Gấu, Quỷ khổng lồ, Yêu tinh, cùng **loài mới tự tiến hoá**.
* **AI cá nhân**: quan sát → suy nghĩ (Utility) → quyết định (GOAP A*) → hành động → học → tiến hoá.
* **Tiến hoá**: 10 gen di truyền + đột biến, chọn lọc tự nhiên, trí tuệ tăng theo kinh nghiệm và văn hoá,
  hình thành loài mới (vd. bầy sói tiến hoá ra chi trước cầm nắm).
* **Văn minh**: Bầy → Bộ lạc → Làng → Thị trấn → Thành phố → **Quốc gia** với lãnh thổ, luật lệ, thể chế, quân đội,
  ngôn ngữ thủ tục, tôn giáo, quốc kỳ, ngoại giao, chiến tranh, chinh phục, tách nhóm.
* **Quyền năng của Thần**: nâng/hạ đất, vẽ địa hình, lửa, thiên thạch, sét, động đất, dịch bệnh, mưa, hạn hán,
  ban phước (tăng trí tuệ — di truyền được!), trừng phạt, thả sinh vật.
* **Debug trực quan**: bong bóng suy nghĩ, thanh trí tuệ, giai đoạn nhận thức (Bản năng → Khai sáng), kế hoạch GOAP,
  điểm Utility, tri thức, trí nhớ, kỹ năng; bảng nền văn minh; **biên niên sử** thế giới.
* **Cảm ứng**: chạm, kéo, chụm 2 ngón để zoom; nút to, giao diện co giãn theo màn hình (dọc & ngang).
* **Tiếng Việt có dấu** (font hệ thống; tự bỏ dấu nếu thiết bị không có font phù hợp).

## Bắt đầu nhanh

```bash
# Linux: thư viện cho raylib
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/worldbox
```

Chạy mô phỏng không đồ hoạ (in biên niên sử):

```bash
./build/worldbox_sim --scenario wolves --years 30      # đàn sói → bộ tộc sói
./build/worldbox_sim --scenario humans --years 80      # người nguyên thuỷ → làng → quốc gia
./build/worldbox_sim --scenario sandbox --years 50 --seed 42
ctest --test-dir build --output-on-failure             # kiểm thử kịch bản
```

### Android APK

```bash
cd android
gradle assembleRelease -PndkVersion=<phiên bản NDK đã cài, vd 27.2.12479018>
adb install -r app/build/outputs/apk/release/app-release.apk
```

GitHub Actions (`.github/workflows/build.yml`) tự build APK cho mỗi lần push — tải ở mục **Artifacts** của lần chạy.

## Cấu trúc mã nguồn

```
src/core/      ECS sparse-set, lưới không gian, thread pool, toán & RNG
src/world/     Bản đồ ô, sinh địa hình (noise), môi trường (cây, lửa, mưa, mùa)
src/ai/        Gen, tính cách, trí nhớ, cây công nghệ, GOAP, Brain (AI cá nhân)
src/civ/       Nhóm xã hội, ngôn ngữ thủ tục, SocietySystem (văn minh, ngoại giao, loài mới)
src/sim/       Simulation (vòng lặp tick), loài, quyền năng Thần, kịch bản, biên niên sử
src/app/       Game raylib: renderer pixel art, UI cảm ứng, font tiếng Việt
src/headless/  Trình chạy không đồ hoạ (test/CI/benchmark)
android/       Dự án Gradle (NativeActivity, không có mã Java)
docs/          Tài liệu kiến trúc chi tiết
```

📖 Đọc **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** để hiểu sơ đồ hệ thống, sơ đồ lớp, pipeline AI,
cơ chế tiến hoá, các giai đoạn văn minh và hai ví dụ chi tiết (đàn sói, người nguyên thuỷ).
