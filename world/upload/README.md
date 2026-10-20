# Đưa thế giới của bạn lên GitHub

Thả một tệp lưu `.wbs` vào thư mục này: trên trang GitHub, bấm **Add file → Upload files** rồi **Commit**.
Workflow **"Thế giới đang sống"** sẽ chạy ngay và lấy thế giới đó làm thế giới chạy liên tục mỗi giờ.
Thế giới cũ vẫn còn trong các bản sao lưu `backup-*.wbs` ở Release `live-world`.

Tệp lưu nằm trong thư mục `saves/` cạnh `worldbox.exe`:

| Tệp | Là |
|---|---|
| `autosave.wbs` | ô tự động lưu |
| `slot1.wbs`, `slot2.wbs`, `slot3.wbs` | ô lưu 1–3 |
| `github.wbs` | bản sao thế giới GitHub bạn đã tải về |

Mỗi tệp chỉ được dùng **một lần**: workflow ghi nhớ mã băm của tệp. Muốn đưa lên lại thì tải lên tệp mới.
