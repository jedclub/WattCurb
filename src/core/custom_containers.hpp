#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <string_view>
#include <type_traits>
#include <utility>
#include <iterator>
#include <charconv>
#include <algorithm>
#include <ostream>

namespace wattcurb::core {

// ============================================================================
// FixedVector<T, Capacity>: Zero-Allocation Inline Vector (REF-REQ-017, REF-ARCH-006)
// ============================================================================
template <typename T, size_t Capacity>
class alignas(alignof(T) > 64 ? alignof(T) : 64) FixedVector {
public:
    static_assert(Capacity > 0, "Capacity must be greater than 0");

    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    constexpr FixedVector() noexcept : size_(0) {}

    ~FixedVector() noexcept {
        clear();
    }

    // TriviallyCopyable optimization
    FixedVector(const FixedVector& other) noexcept {
        copy_from(other);
    }

    FixedVector& operator=(const FixedVector& other) noexcept {
        if (this != &other) {
            clear();
            copy_from(other);
        }
        return *this;
    }

    FixedVector(FixedVector&& other) noexcept {
        move_from(std::move(other));
    }

    FixedVector& operator=(FixedVector&& other) noexcept {
        if (this != &other) {
            clear();
            move_from(std::move(other));
        }
        return *this;
    }

    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] constexpr size_type max_size() const noexcept { return Capacity; }
    [[nodiscard]] constexpr size_type capacity() const noexcept { return Capacity; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr bool full() const noexcept { return size_ >= Capacity; }

    [[nodiscard]] pointer data() noexcept { return reinterpret_cast<pointer>(storage_); }
    [[nodiscard]] const_pointer data() const noexcept { return reinterpret_cast<const_pointer>(storage_); }

    [[nodiscard]] iterator begin() noexcept { return data(); }
    [[nodiscard]] const_iterator begin() const noexcept { return data(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return data(); }

    [[nodiscard]] iterator end() noexcept { return data() + size_; }
    [[nodiscard]] const_iterator end() const noexcept { return data() + size_; }
    [[nodiscard]] const_iterator cend() const noexcept { return data() + size_; }

    static reference dummy_instance() noexcept {
        static T dummy{};
        return dummy;
    }

    [[nodiscard]] bool check_integrity() const noexcept {
        return canary_ == CANARY_MAGIC;
    }

    [[nodiscard]] bool overflow_occurred() const noexcept {
        return overflow_count_ > 0;
    }

    [[nodiscard]] uint32_t overflow_count() const noexcept {
        return overflow_count_;
    }

    void reset_overflow() noexcept {
        overflow_count_ = 0;
    }

    [[nodiscard]] reference operator[](size_type idx) noexcept {
        if (size_ == 0) return dummy_instance();
        if (idx >= size_) idx = size_ - 1; // Safe saturating clamp
        return data()[idx];
    }
    [[nodiscard]] const_reference operator[](size_type idx) const noexcept {
        if (size_ == 0) return dummy_instance();
        if (idx >= size_) idx = size_ - 1; // Safe saturating clamp
        return data()[idx];
    }

    [[nodiscard]] reference at(size_type idx) noexcept {
        if (size_ == 0) return dummy_instance();
        if (idx >= size_) idx = size_ - 1;
        return data()[idx];
    }
    [[nodiscard]] const_reference at(size_type idx) const noexcept {
        if (size_ == 0) return dummy_instance();
        if (idx >= size_) idx = size_ - 1;
        return data()[idx];
    }

    [[nodiscard]] reference front() noexcept {
        if (size_ == 0) return dummy_instance();
        return data()[0];
    }
    [[nodiscard]] const_reference front() const noexcept {
        if (size_ == 0) return dummy_instance();
        return data()[0];
    }

    [[nodiscard]] reference back() noexcept {
        if (size_ == 0) return dummy_instance();
        return data()[size_ - 1];
    }
    [[nodiscard]] const_reference back() const noexcept {
        if (size_ == 0) return dummy_instance();
        return data()[size_ - 1];
    }

    [[nodiscard]] std::span<T> span() noexcept { return std::span<T>(data(), size_); }
    [[nodiscard]] std::span<const T> span() const noexcept { return std::span<const T>(data(), size_); }
    operator std::span<T>() noexcept { return std::span<T>(data(), size_); }
    operator std::span<const T>() const noexcept { return std::span<const T>(data(), size_); }

    bool push_back(const T& val) noexcept {
        if (size_ >= Capacity) {
            ++overflow_count_;
            return false;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(data() + size_, &val, sizeof(T));
        } else {
            ::new (static_cast<void*>(data() + size_)) T(val);
        }
        ++size_;
        return true;
    }

    bool push_back(T&& val) noexcept {
        if (size_ >= Capacity) {
            ++overflow_count_;
            return false;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(data() + size_, &val, sizeof(T));
        } else {
            ::new (static_cast<void*>(data() + size_)) T(std::move(val));
        }
        ++size_;
        return true;
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) noexcept {
        if (size_ >= Capacity) {
            ++overflow_count_;
            return (size_ > 0) ? data()[size_ - 1] : dummy_instance(); // Safe bound clamp
        }
        pointer ptr = data() + size_;
        ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
        ++size_;
        return *ptr;
    }

    void pop_back() noexcept {
        if (size_ > 0) {
            --size_;
            if constexpr (!std::is_trivially_destructible_v<T>) {
                data()[size_].~T();
            }
        }
    }

    void clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = 0; i < size_; ++i) {
                data()[i].~T();
            }
        }
        size_ = 0;
    }

    void resize(size_type new_size) noexcept {
        if (new_size > Capacity) new_size = Capacity;
        if (new_size < size_) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_type i = new_size; i < size_; ++i) {
                    data()[i].~T();
                }
            }
            size_ = new_size;
        } else if (new_size > size_) {
            for (size_type i = size_; i < new_size; ++i) {
                ::new (static_cast<void*>(data() + i)) T();
            }
            size_ = new_size;
        }
    }

private:
    static constexpr uint64_t CANARY_MAGIC = 0xDEADBEEFCAFE0001ULL;

    void copy_from(const FixedVector& other) noexcept {
        size_ = other.size_;
        overflow_count_ = other.overflow_count_;
        canary_ = CANARY_MAGIC;
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (size_ > 0) {
                std::memcpy(data(), other.data(), size_ * sizeof(T));
            }
        } else {
            for (size_type i = 0; i < size_; ++i) {
                ::new (static_cast<void*>(data() + i)) T(other[i]);
            }
        }
    }

    void move_from(FixedVector&& other) noexcept {
        size_ = other.size_;
        overflow_count_ = other.overflow_count_;
        canary_ = CANARY_MAGIC;
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (size_ > 0) {
                std::memcpy(data(), other.data(), size_ * sizeof(T));
            }
        } else {
            for (size_type i = 0; i < size_; ++i) {
                ::new (static_cast<void*>(data() + i)) T(std::move(other[i]));
            }
        }
        other.clear();
    }

    size_type size_{0};
    uint32_t overflow_count_{0};
    alignas(T) std::byte storage_[sizeof(T) * Capacity];
    uint64_t canary_{CANARY_MAGIC};
};


// ============================================================================
// FixedString<Capacity>: Zero-Allocation TriviallyCopyable String (REF-REQ-017)
// ============================================================================
template <size_t Capacity>
class FixedString {
public:
    static_assert(Capacity > 1, "Capacity must be at least 2 for null-terminator");

    constexpr FixedString() noexcept {
        buf_[0] = '\0';
    }

    FixedString(std::string_view sv) noexcept {
        assign(sv);
    }

    FixedString(const char* s) noexcept {
        if (s) assign(std::string_view(s));
        else buf_[0] = '\0';
    }

    void assign(std::string_view sv) noexcept {
        len_ = std::min(sv.size(), Capacity - 1);
        std::memcpy(buf_.data(), sv.data(), len_);
        buf_[len_] = '\0';
    }

    FixedString& operator=(std::string_view sv) noexcept {
        assign(sv);
        return *this;
    }

    FixedString& operator=(const char* s) noexcept {
        if (s) assign(std::string_view(s));
        else clear();
        return *this;
    }

    [[nodiscard]] const char* c_str() const noexcept { return buf_.data(); }
    [[nodiscard]] std::string_view view() const noexcept { return std::string_view(buf_.data(), len_); }
    [[nodiscard]] size_t size() const noexcept { return len_; }
    [[nodiscard]] size_t length() const noexcept { return len_; }
    [[nodiscard]] bool empty() const noexcept { return len_ == 0; }
    [[nodiscard]] static constexpr size_t max_size() noexcept { return Capacity - 1; }

    void clear() noexcept {
        len_ = 0;
        buf_[0] = '\0';
    }

    bool append(std::string_view sv) noexcept {
        size_t avail = (Capacity - 1) - len_;
        size_t to_copy = std::min(sv.size(), avail);
        if (to_copy > 0) {
            std::memcpy(buf_.data() + len_, sv.data(), to_copy);
            len_ += to_copy;
            buf_[len_] = '\0';
        }
        return to_copy == sv.size();
    }

    bool append(char c) noexcept {
        if (len_ + 1 < Capacity) {
            buf_[len_++] = c;
            buf_[len_] = '\0';
            return true;
        }
        return false;
    }

    bool append_i32(int32_t val) noexcept {
        char tmp[16];
        auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), val);
        if (ec == std::errc()) {
            return append(std::string_view(tmp, static_cast<size_t>(ptr - tmp)));
        }
        return false;
    }

    bool append_u64(uint64_t val) noexcept {
        char tmp[24];
        auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), val);
        if (ec == std::errc()) {
            return append(std::string_view(tmp, static_cast<size_t>(ptr - tmp)));
        }
        return false;
    }

    [[nodiscard]] char operator[](size_t idx) const noexcept {
        if (idx >= len_) return '\0';
        return buf_[idx];
    }

    [[nodiscard]] bool check_integrity() const noexcept {
        return len_ < Capacity && buf_[len_] == '\0' && canary_ == CANARY_MAGIC;
    }

    operator std::string_view() const noexcept { return view(); }

    bool operator==(std::string_view sv) const noexcept { return view() == sv; }
    bool operator==(const char* s) const noexcept { return view() == std::string_view(s ? s : ""); }
    bool operator==(const FixedString& o) const noexcept { return view() == o.view(); }

    friend std::ostream& operator<<(std::ostream& os, const FixedString& fs) {
        return os << fs.view();
    }

private:
    static constexpr uint64_t CANARY_MAGIC = 0x5354524741555244ULL; // "STRGUARD"

    std::array<char, Capacity> buf_{};
    size_t len_{0};
    uint64_t canary_{CANARY_MAGIC};
};

static_assert(std::is_trivially_copyable_v<FixedString<64>>, "FixedString must be TriviallyCopyable");


// ============================================================================
// TopKHeap<T, K, Compare>: Zero-Allocation Streaming Top-K Min-Heap (REF-ARCH-006)
// ============================================================================
// Maintains top-K maximum elements in an O(N log K) stream without full sort.
template <typename T, size_t K, typename Compare = std::greater<T>>
class TopKHeap {
public:
    static_assert(K > 0, "K must be > 0");

    constexpr TopKHeap() noexcept = default;

    void push(const T& item) noexcept {
        if (heap_.size() < K) {
            heap_.push_back(item);
            std::push_heap(heap_.begin(), heap_.end(), comp_);
        } else if (comp_(item, heap_.front())) {
            // New item is greater than the smallest of the top-K
            std::pop_heap(heap_.begin(), heap_.end(), comp_);
            heap_.back() = item;
            std::push_heap(heap_.begin(), heap_.end(), comp_);
        }
    }

    // Return sorted elements (from largest to smallest according to Compare)
    FixedVector<T, K> extract_sorted() const noexcept {
        FixedVector<T, K> res = heap_;
        std::sort(res.begin(), res.end(), comp_);
        return res;
    }

    [[nodiscard]] size_t size() const noexcept { return heap_.size(); }
    [[nodiscard]] bool empty() const noexcept { return heap_.empty(); }

private:
    FixedVector<T, K> heap_;
    Compare comp_{};
};

// ============================================================================
// DoubleBufferedPool<T, Capacity>: Zero-Allocation Ping-Pong Storage (REF-ARCH-006)
// ============================================================================
// Pre-allocates two 64-byte aligned buffers in BSS/static arena.
// Swapping active and previous buffers takes exactly 1 pointer swap (0 ns, 0 syscalls).
template <typename T, size_t Capacity>
class alignas(64) DoubleBufferedPool {
public:
    using VectorType = FixedVector<T, Capacity>;

    DoubleBufferedPool() noexcept {
        current_ = &buf_a_;
        previous_ = &buf_b_;
    }

    // Ping-pong pointer swap
    void swap() noexcept {
        std::swap(current_, previous_);
        previous_->clear();
    }

    [[nodiscard]] VectorType& current() noexcept { return *current_; }
    [[nodiscard]] const VectorType& current() const noexcept { return *current_; }

    [[nodiscard]] VectorType& next() noexcept { return *previous_; }
    [[nodiscard]] const VectorType& next() const noexcept { return *previous_; }

    [[nodiscard]] VectorType& previous() noexcept { return *previous_; }
    [[nodiscard]] const VectorType& previous() const noexcept { return *previous_; }

    void clear() noexcept {
        buf_a_.clear();
        buf_b_.clear();
        current_ = &buf_a_;
        previous_ = &buf_b_;
    }

private:
    VectorType buf_a_;
    VectorType buf_b_;
    VectorType* current_{nullptr};
    VectorType* previous_{nullptr};
};

} // namespace wattcurb::core
