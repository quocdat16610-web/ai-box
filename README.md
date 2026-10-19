# WorldBox AI 🌍

Game sandbox mô phỏng thế giới **pixel art 2D top-down** lấy cảm hứng từ *WorldBox*, viết bằng **C++17 + raylib**,
chạy trên **PC và Android (APK)**. Bạn là Thần: nặn địa hình, gọi mưa, giáng thiên thạch, thả sinh vật…
và quan sát chúng **tự suy nghĩ, học hỏi, tiến hoá** thành bộ tộc, làng mạc và quốc gia.

> Điểm khác biệt chính:
> * **Mỗi sinh vật có một bộ não AI riêng** (Utility AI + GOAP + trí nhớ ngắn/dài hạn + tính cách + kỹ năng + sáng tạo).
> * **Vật lý & hoá học thật**: nhiệt độ (°C), chất lỏng chảy theo địa hình, khí & áp suất, trọng lực, va chạm, nổ;
>   46 vật liệu có thuộc tính riêng, 13 quy luật phản ứng tổng quát (cháy, nhiệt phân, nung, khử quặng, hợp kim,
>   axit–bazơ, hydrat hoá, oxi hoá, kết tủa, phân huỷ, nổ…), chu trình nước – carbon – dinh dưỡng.
> * **Công nghệ MỞ, không có tech tree**: sinh vật hiểu khái niệm vật lý từ quan sát, tự ghép vật liệu × hình dạng thành
>   thiết kế (hiệu năng tính bằng vật lý), làm nguyên mẫu, thử nghiệm, cải tiến không giới hạn; học quy trình hoá học
>   từ các phản ứng thật; hợp kim mới sinh ra trong lúc chơi → vật liệu, thời đại, trình độ công nghệ **không có trần**.

## Tính năng

* **Hành tinh hình cầu**: bản đồ quấn vòng đông–tây như Trái Đất (đi mãi về phía đông sẽ vòng về phía tây), hai cực
  có chỏm băng; **thu nhỏ hết cỡ → quả địa cầu 3D** xoay được (ngày/đêm, khí quyển, nhãn quốc gia), chạm để hạ xuống.
  6 kiểu hành tinh sinh ngẫu nhiên: siêu lục địa, nhiều châu lục, quần đảo, hành tinh đại dương, hành tinh khô, hoặc
  để máy bốc ngẫu nhiên.
* **Lòng đất**: hang động ngoằn ngoèo, sông hồ ngầm, túi magma, mạch quặng, nấm phát quang; dơi & nhện sống trong hang;
  sinh vật có tay **đào hầm mỏ** men theo mạch quặng. Nút **Lòng đất** để nhìn thẳng xuống hang & hầm mỏ.
* **Góc nhìn thứ nhất**: chọn bất kỳ sinh vật nào → **Nhìn qua mắt** (cảnh 3D low-poly có ánh nắng, sương mù xa, mây trôi,
  cỏ & cây đung đưa, mặt nước gợn sóng; cá thấy đáy biển; dơi thấy vách hang) → **Điều khiển** để nhập vai: cần ảo/WASD đi,
  kéo để nhìn, tấn công, ăn, thu thập, dựng lều. Sinh vật có **hoạt ảnh**: chân bước, tay vung, đuôi vẫy, chim vỗ cánh, cá quẫy,
  ngủ nằm, ăn cúi đầu; người chơi thấy tay mình cầm giáo/rìu vung lên khi hành động.
* **Lưu / tải thế giới**: 3 ô lưu + 1 ô tự động lưu (mỗi 3 phút, khi thoát / về menu); F5 lưu nhanh, F9 tải nhanh. Lưu đầy đủ
  mọi thứ (địa hình, vật chất, lòng đất, sinh vật kèm trí nhớ & kế hoạch, bộ lạc, quốc gia, loài tự tạo...) — tải lên chạy tiếp
  giống hệt như chưa từng dừng.
* **Phòng thí nghiệm gen** (mục Sự sống): cấy ghép gen giữa hai loài, chỉnh từng gen, tạo **gen mới tuỳ ý** (cánh, mang cá,
  nọc độc, vảy giáp, quang hợp, phát quang…), chỉnh màu, kích thước, hình dáng, môi trường sống → tạo loài mới và thả ra.
  Loài nhân tạo vẫn tự tiến hoá; khi phân hoá loài trong tự nhiên cũng có thể **đột biến ra gen mới**.
* **21 loài**: 4 chủng tộc, quái vật, và muông thú trên cạn (sói, hươu, thỏ, gấu, lợn rừng, voi ma mút, cáo, khỉ), dưới
  nước (cá, cá mập), lưỡng cư (cá sấu), trên trời (đại bàng, chim sẻ), trong hang (dơi, nhện hang).

* **Thế giới**: địa hình sinh thủ tục (biển, cát, đồng cỏ, rừng, rừng rậm, sa mạc, thảo nguyên, đồi, núi, tuyết,
  lãnh nguyên, đầm lầy, núi lửa) + địa hình sinh ra từ vật lý (băng, bazan, thuỷ tinh); mỏ đá/quặng nhiều loại
  (đá lửa, đá vôi, đất sét, đồng, thiếc, sắt, vàng, lưu huỳnh, diêm tiêu, 2 khoáng chất lạ theo seed); mùa, mưa theo
  độ ẩm khí quyển, sét (nung chảy cát thành thuỷ tinh!), hạn hán, cháy rừng lan theo nhiệt & gió.
* **Vật lý & hoá học**: nhiệt độ khuếch tán, chất lỏng chảy theo độ cao, khí & áp suất, phản ứng theo thuộc tính
  vật liệu; mũi tên/đá/mảnh vỡ bay theo parabol, nổi/chìm theo Archimedes, nổ đẩy văng vật thể & sinh vật;
  bếp & lò luyện là nguồn nhiệt thật trong lưới vật chất.
* **Chủng tộc & sinh vật**: Người, Elf, Orc, Dwarf, Sói, Hươu, Thỏ, Gấu, Quỷ khổng lồ, Yêu tinh, cùng **loài mới tự tiến hoá**.
* **AI cá nhân**: quan sát (kể cả phản ứng hoá học & sự kiện vật lý) → suy nghĩ (Utility) → quyết định (GOAP A*)
  → hành động → học (khái niệm, vật liệu, thiết kế, quy trình) → tiến hoá.
* **Sáng tạo mở**: rìu đá, giáo cán xương, cung, ná, thuyền độc mộc, bè, xe có bánh, lò tường đá, mồi lửa, máy bắn đá…
  không món nào được lập trình sẵn — đều là tổ hợp do sinh vật tự nghĩ ra, làm thử và cải tiến; luyện đồng, nung vôi,
  hợp kim… là quy trình chúng tự hiểu ra khi QUAN SÁT phản ứng thật (trong lò, cháy rừng, núi lửa).
* **Tiến hoá**: 13 gen (kể cả khéo tay, kích thước, bơi lội) + đột biến, chọn lọc tự nhiên, trí tuệ tăng theo kinh nghiệm
  và văn hoá, hình thành loài mới (vd. bầy sói tiến hoá ra chi trước cầm nắm).
* **Văn minh**: Bầy → Bộ lạc → Làng → Thị trấn → Thành phố → **Quốc gia** với lãnh thổ, luật lệ, thể chế, quân đội,
  ngôn ngữ thủ tục, tôn giáo, quốc kỳ, ngoại giao, chiến tranh, chinh phục, tách nhóm; **thời đại** đặt tên theo vật liệu
  tiên tiến nhất và **trình độ công nghệ** liên tục không có trần.
* **Quyền năng của Thần**: nâng/hạ đất, vẽ địa hình, lửa, thiên thạch (mang quặng sắt, mảnh vỡ nóng), sét, động đất,
  dịch bệnh, mưa, hạn hán, ban phước, trừng phạt, thả sinh vật, **đổ nước/dung nham/axit/dầu, rắc thuốc súng,
  đốt nóng/làm lạnh, ném đá tảng, tạo mạch quặng**.
* **Debug trực quan**: bong bóng suy nghĩ, thanh trí tuệ, giai đoạn nhận thức, kế hoạch GOAP, điểm Utility, khái niệm,
  thiết kế, ý tưởng đang theo đuổi; bảng nền văn minh; **lớp nhiệt độ / khí & áp suất / phản ứng hoá học** trên bản đồ;
  bảng Hoá học; **biên niên sử** thế giới.
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
./build/worldbox_sim --scenario twins --years 120 --seed 21   # 2 bộ tộc nhân bản, 2 môi trường → 2 con đường
./build/worldbox_sim --chemtest                        # kiểm thử hoá học xác định
./build/worldbox_sim --genelab-test                    # kiểm thử phòng thí nghiệm gen
./build/worldbox_sim --world archipelago --map dao.ppm --years 0   # xuất bản đồ một hành tinh quần đảo
./build/worldbox_sim --scenario sandbox --years 50 --seed 42
ctest --test-dir build --output-on-failure             # kiểm thử kịch bản
```

### Windows

* **Không cần build**: vào tab *Actions* của repo → lần chạy mới nhất → tải artifact **worldbox-windows** →
  giải nén → chạy `worldbox.exe` (bấm "More info → Run anyway" nếu SmartScreen hỏi).
* **Tự build**: cài *Visual Studio 2022* (workload "Desktop development with C++"), *CMake* và *Git*, rồi:

```bat
cmake -S . -B build
cmake --build build --config Release
build\Release\worldbox.exe
build\Release\worldbox_sim.exe --scenario twins --years 120 --seed 21
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
src/world/     Bản đồ ô, sinh địa hình (noise), mỏ khoáng, núi lửa, thời tiết
src/phys/      Vật liệu, lưới vật chất (nhiệt/lỏng/khí), quy luật hoá học, vật lý vật thể 2.5D
src/ai/        Gen, tính cách, trí nhớ, tập quán, GOAP, Brain, công nghệ mở (Invention/Creativity)
src/civ/       Nhóm xã hội, ngôn ngữ thủ tục, SocietySystem (văn minh, ngoại giao, loài mới)
src/sim/       Simulation (vòng lặp tick), loài, quyền năng Thần, kịch bản, biên niên sử
src/app/       Game raylib: renderer pixel art, quả địa cầu 3D, góc nhìn thứ nhất, phòng gen, UI cảm ứng, font tiếng Việt
src/headless/  Trình chạy không đồ hoạ (test/CI/benchmark)
android/       Dự án Gradle (NativeActivity, không có mã Java)
cmake/         Bản vá raylib cho xoay màn hình Android
docs/          Tài liệu kiến trúc chi tiết
```

📖 Đọc **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** để hiểu sơ đồ hệ thống, sơ đồ lớp, pipeline AI,
hệ vật lý – hoá học, công nghệ mở, các giai đoạn văn minh và ba ví dụ chạy thật (đàn sói, người nguyên thuỷ,
hai bộ tộc song sinh phát triển theo hai hướng khó đoán).
