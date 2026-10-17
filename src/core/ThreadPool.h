// =============================================================================
// ThreadPool.h — Bể luồng cố định + parallelFor.
//
// Dùng cho pha "suy nghĩ" (Think) của AI: mỗi agent chỉ ĐỌC thế giới và GHI vào
// bộ não của chính nó, nên có thể chia N agent cho nhiều lõi CPU (kể cả điện
// thoại Android 8 nhân). Pha "hành động" (Act) thay đổi thế giới vẫn chạy tuần tự
// để giữ tính xác định và tránh data race.
// =============================================================================
#pragma once
#include <thread>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace wb {

class ThreadPool {
public:
    explicit ThreadPool(unsigned threads = 0) {
        if (threads == 0) threads = std::max(1u, std::thread::hardware_concurrency());
        threads = std::min(threads, 8u);
        for (unsigned i = 1; i < threads; ++i) workers_.emplace_back([this] { workerLoop(); });
    }
    ~ThreadPool() {
        { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }
    unsigned size() const { return static_cast<unsigned>(workers_.size() + 1); }

    // Chạy fn(i) với i trong [0, n). Luồng gọi cũng tham gia làm việc.
    void parallelFor(int n, const std::function<void(int)>& fn) {
        if (n <= 0) return;
        if (workers_.empty() || n < 64) { for (int i = 0; i < n; ++i) fn(i); return; }
        {
            std::lock_guard<std::mutex> lk(m_);
            job_ = &fn; total_ = n; next_ = 0; pending_ = static_cast<int>(workers_.size());
            ++generation_;
        }
        cv_.notify_all();
        runChunks();
        std::unique_lock<std::mutex> lk(m_);
        done_.wait(lk, [this] { return pending_ == 0; });
        job_ = nullptr;
    }

private:
    void runChunks() {
        const int chunk = 32;
        for (;;) {
            int begin = next_.fetch_add(chunk);
            if (begin >= total_) break;
            int end = std::min(total_, begin + chunk);
            for (int i = begin; i < end; ++i) (*job_)(i);
        }
    }
    void workerLoop() {
        uint64_t seen = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [&] { return stop_ || generation_ != seen; });
                if (stop_) return;
                seen = generation_;
            }
            runChunks();
            std::lock_guard<std::mutex> lk(m_);
            if (--pending_ == 0) done_.notify_one();
        }
    }
    std::vector<std::thread> workers_;
    std::mutex m_;
    std::condition_variable cv_, done_;
    const std::function<void(int)>* job_ = nullptr;
    std::atomic<int> next_{0};
    int total_ = 0, pending_ = 0;
    uint64_t generation_ = 0;
    bool stop_ = false;
};

} // namespace wb
