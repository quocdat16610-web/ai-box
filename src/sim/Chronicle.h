// =============================================================================
// Chronicle.h — Biên niên sử thế giới: ghi lại các sự kiện lịch sử quan trọng
// (phát minh đầu tiên, bộ tộc thành lập, quốc gia ra đời, chiến tranh, loài mới).
// Hiển thị trong UI và in ra ở chế độ headless.
// =============================================================================
#pragma once
#include <deque>
#include <string>
#include <cstdio>
#include <functional>

namespace wb {

struct SaveAccess;

enum class EventKind : uint8_t { Discovery, Society, War, Evolution, Disaster, Death, Misc };

struct ChronicleEntry {
    long tick;
    EventKind kind;
    std::string text;
    int x = -1, y = -1;
};

class Chronicle {
public:
    std::function<void(const ChronicleEntry&)> onEvent; // callback (vd: in ra console)
    void add(long tick, EventKind k, std::string text, int x = -1, int y = -1) {
        entries_.push_back({tick, k, std::move(text), x, y});
        if (entries_.size() > 400) entries_.pop_front();
        ++total_;
        if (onEvent) onEvent(entries_.back());
    }
    const std::deque<ChronicleEntry>& entries() const { return entries_; }
    long total() const { return total_; }
    void clear() { entries_.clear(); total_ = 0; }

private:
    friend struct SaveAccess;
    std::deque<ChronicleEntry> entries_;
    long total_ = 0;
};

} // namespace wb
