// =============================================================================
// Net.h — Tải một tệp từ Internet về máy (chạy nền, không làm đứng game).
// Dùng để lấy "thế giới đang sống" mà GitHub Actions chạy liên tục (live-world.yml).
//   * PC (Windows 10+/Linux/macOS): gọi lệnh `curl` có sẵn của hệ điều hành.
//   * Android: java.net.HttpURLConnection qua JNI (cần quyền INTERNET trong manifest).
// Tải vào tệp tạm rồi mới đổi tên → tệp đích không bao giờ bị ghi dở.
// =============================================================================
#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace wb {

class Download {
public:
    enum State { Idle, Running, Done, Failed };
    ~Download();
    void start(const std::string& url, const std::string& path);
    State state() const { return state_.load(); }
    std::string error() const { std::lock_guard<std::mutex> l(m_); return error_; }
    long bytes() const { return bytes_.load(); }   // số byte đã nhận (Android; PC chỉ biết khi xong)
    void reset() { if (state_ != Running) state_ = Idle; }

private:
    void run(std::string url, std::string path);
    std::thread th_;
    std::atomic<State> state_{Idle};
    std::atomic<long> bytes_{0};
    mutable std::mutex m_;
    std::string error_;
};

// Đường dẫn tải thế giới đang sống (đặt lúc build bằng -DWB_LIVE_WORLD_URL=...)
const char* liveWorldUrl();
// Trang GitHub hiển thị tình hình thế giới (Release "live-world")
const char* liveWorldPage();

} // namespace wb
