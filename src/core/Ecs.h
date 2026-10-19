// =============================================================================
// Ecs.h — Entity Component System tối giản dạng "sparse set" (giống EnTT).
//
//  * Entity  : chỉ là một số nguyên 32-bit = [version 12 bit | index 20 bit].
//              version tăng mỗi lần index được tái sử dụng → phát hiện handle cũ.
//  * Component: struct dữ liệu thuần (POD-ish), không chứa logic.
//  * Pool<T> : mảng dày (dense) chứa component liền kề trong bộ nhớ → duyệt rất
//              nhanh, thân thiện cache; mảng thưa (sparse) ánh xạ index → vị trí.
//  * System  : hàm tự do duyệt qua các component (xem thư mục sim/, ai/).
//
// Vì sao ECS? Game có hàng nghìn sinh vật, mỗi con chạy AI riêng. Lưu dữ liệu
// liền kề giúp các hệ thống (di chuyển, trao đổi chất, AI) chạy theo lô (batch)
// và dễ song song hoá.
// =============================================================================
#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <cassert>
#include <utility>

namespace wb {

struct SaveAccess;

using Entity = uint32_t;
constexpr Entity NullEntity = 0xFFFFFFFFu;
constexpr uint32_t kIndexBits = 20;
constexpr uint32_t kIndexMask = (1u << kIndexBits) - 1;

inline uint32_t entityIndex(Entity e) { return e & kIndexMask; }
inline uint32_t entityVersion(Entity e) { return e >> kIndexBits; }

// Cấp phát id kiểu cho từng loại component lúc chạy (không cần RTTI).
inline uint32_t nextComponentTypeId() { static uint32_t counter = 0; return counter++; }
template <class T> uint32_t componentTypeId() { static const uint32_t id = nextComponentTypeId(); return id; }

struct IPool {
    virtual ~IPool() = default;
    virtual void removeIfPresent(Entity e) = 0;
};

template <class T>
class Pool final : public IPool {
public:
    std::vector<T> data;          // component, liền kề
    std::vector<Entity> entities; // entity tương ứng với từng phần tử data
    std::vector<uint32_t> sparse; // index entity → vị trí trong dense (+1, 0 = không có)

    bool has(Entity e) const {
        uint32_t i = entityIndex(e);
        return i < sparse.size() && sparse[i] != 0 && entities[sparse[i] - 1] == e;
    }
    T* tryGet(Entity e) { return has(e) ? &data[sparse[entityIndex(e)] - 1] : nullptr; }
    const T* tryGet(Entity e) const { return has(e) ? &data[sparse[entityIndex(e)] - 1] : nullptr; }
    T& get(Entity e) { assert(has(e)); return data[sparse[entityIndex(e)] - 1]; }

    template <class... Args>
    T& emplace(Entity e, Args&&... args) {
        uint32_t i = entityIndex(e);
        if (i >= sparse.size()) sparse.resize(i + 1, 0);
        if (has(e)) { data[sparse[i] - 1] = T{std::forward<Args>(args)...}; return data[sparse[i] - 1]; }
        data.push_back(T{std::forward<Args>(args)...});
        entities.push_back(e);
        sparse[i] = static_cast<uint32_t>(data.size());
        return data.back();
    }
    // Xoá theo kiểu "swap-and-pop": O(1), giữ mảng dày liền kề.
    void removeIfPresent(Entity e) override {
        if (!has(e)) return;
        uint32_t pos = sparse[entityIndex(e)] - 1;
        uint32_t last = static_cast<uint32_t>(data.size() - 1);
        if (pos != last) {
            data[pos] = std::move(data[last]);
            entities[pos] = entities[last];
            sparse[entityIndex(entities[pos])] = pos + 1;
        }
        data.pop_back();
        entities.pop_back();
        sparse[entityIndex(e)] = 0;
    }
    size_t size() const { return data.size(); }
};

class Registry {
public:
    Entity create() {
        uint32_t idx;
        if (!freeList_.empty()) { idx = freeList_.back(); freeList_.pop_back(); }
        else { idx = static_cast<uint32_t>(versions_.size()); versions_.push_back(0); }
        ++alive_;
        return (versions_[idx] << kIndexBits) | idx;
    }
    bool valid(Entity e) const {
        if (e == NullEntity) return false;
        uint32_t i = entityIndex(e);
        return i < versions_.size() && versions_[i] == entityVersion(e);
    }
    // Huỷ ngay lập tức: gỡ mọi component và tăng version.
    void destroy(Entity e) {
        if (!valid(e)) return;
        for (auto& p : pools_) if (p) p->removeIfPresent(e);
        uint32_t i = entityIndex(e);
        versions_[i] = (versions_[i] + 1) & 0xFFF;
        freeList_.push_back(i);
        --alive_;
    }

    template <class T> Pool<T>& pool() {
        uint32_t id = componentTypeId<T>();
        if (id >= pools_.size()) pools_.resize(id + 1);
        if (!pools_[id]) pools_[id] = std::make_unique<Pool<T>>();
        return *static_cast<Pool<T>*>(pools_[id].get());
    }
    template <class T, class... Args> T& emplace(Entity e, Args&&... a) {
        return pool<T>().emplace(e, std::forward<Args>(a)...);
    }
    template <class T> T& get(Entity e) { return pool<T>().get(e); }
    template <class T> T* tryGet(Entity e) { return valid(e) ? pool<T>().tryGet(e) : nullptr; }
    template <class T> bool has(Entity e) { return valid(e) && pool<T>().has(e); }
    template <class T> void remove(Entity e) { pool<T>().removeIfPresent(e); }

    // Duyệt mọi entity có đủ các component A, B... Gọi f(entity, A&, B&...).
    // Duyệt trên mảng dày của A (nên đặt component "hiếm nhất" ở vị trí đầu).
    template <class A, class... Rest, class F> void each(F&& f) {
        auto& pa = pool<A>();
        for (size_t k = 0; k < pa.entities.size(); ++k) {
            Entity e = pa.entities[k];
            if ((pool<Rest>().has(e) && ...)) f(e, pa.data[k], pool<Rest>().get(e)...);
        }
    }
    size_t aliveCount() const { return alive_; }

private:
    friend struct SaveAccess;
    std::vector<uint32_t> versions_;
    std::vector<uint32_t> freeList_;
    std::vector<std::unique_ptr<IPool>> pools_;
    size_t alive_ = 0;
};

} // namespace wb
