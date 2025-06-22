#pragma once

#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstddef>
#include <new>
#include <string>
#include <string_view>
#include <unordered_set>
#include <shared_mutex>

namespace crossexpand {

// Memory pool for fixed-size allocations
template<size_t BlockSize, size_t BlockCount>
class FixedSizePool {
private:
    static_assert(BlockSize >= sizeof(void*), "BlockSize must be at least pointer size");
    static_assert(BlockCount > 0, "BlockCount must be positive");
    
    alignas(std::max_align_t) char memory_[BlockSize * BlockCount];
    void* free_list_;
    std::atomic<size_t> allocated_count_{0};
    std::atomic<size_t> peak_usage_{0};
    std::mutex mutex_;
    bool initialized_;

public:
    FixedSizePool() : free_list_(nullptr), initialized_(false) {
        initialize();
    }
    
    ~FixedSizePool() = default;
    
    // Non-copyable, non-movable
    FixedSizePool(const FixedSizePool&) = delete;
    FixedSizePool& operator=(const FixedSizePool&) = delete;
    
    void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!free_list_) {
            return nullptr; // Pool exhausted
        }
        
        void* result = free_list_;
        free_list_ = *static_cast<void**>(free_list_);
        
        size_t current = allocated_count_.fetch_add(1) + 1;
        
        // Update peak usage atomically
        size_t peak = peak_usage_.load();
        while (current > peak && !peak_usage_.compare_exchange_weak(peak, current)) {
            // Retry if another thread updated peak_usage_
        }
        
        return result;
    }
    
    void deallocate(void* ptr) {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Add back to free list
        *static_cast<void**>(ptr) = free_list_;
        free_list_ = ptr;
        
        allocated_count_.fetch_sub(1);
    }
    
    // Statistics
    size_t allocated_count() const { return allocated_count_.load(); }
    size_t peak_usage() const { return peak_usage_.load(); }
    size_t capacity() const { return BlockCount; }
    size_t block_size() const { return BlockSize; }
    double utilization() const { 
        return static_cast<double>(allocated_count()) / BlockCount; 
    }
    
    bool is_from_pool(void* ptr) const {
        return ptr >= memory_ && ptr < (memory_ + sizeof(memory_));
    }

private:
    void initialize() {
        // Initialize free list
        for (size_t i = 0; i < BlockCount - 1; ++i) {
            char* block = memory_ + i * BlockSize;
            char* next_block = memory_ + (i + 1) * BlockSize;
            *reinterpret_cast<void**>(block) = next_block;
        }
        
        // Last block points to nullptr
        char* last_block = memory_ + (BlockCount - 1) * BlockSize;
        *reinterpret_cast<void**>(last_block) = nullptr;
        
        free_list_ = memory_;
        initialized_ = true;
    }
};

// Memory pool manager with multiple pool sizes
class MemoryPoolManager {
private:
    // Different pool sizes for different allocation patterns
    std::unique_ptr<FixedSizePool<64, 1024>> small_pool_;     // 64KB total
    std::unique_ptr<FixedSizePool<256, 512>> medium_pool_;    // 128KB total
    std::unique_ptr<FixedSizePool<1024, 256>> large_pool_;    // 256KB total
    std::unique_ptr<FixedSizePool<4096, 64>> xlarge_pool_;    // 256KB total
    
    // Fallback to system allocator for oversized requests
    std::atomic<size_t> system_allocations_{0};
    std::atomic<size_t> system_deallocations_{0};
    std::atomic<size_t> total_system_bytes_{0};
    
    // Performance tracking
    std::atomic<uint64_t> total_allocations_{0};
    std::atomic<uint64_t> pool_allocations_{0};
    std::chrono::steady_clock::time_point start_time_;

public:
    MemoryPoolManager();
    ~MemoryPoolManager() = default;
    
    // Main allocation interface
    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);
    
    // Typed allocation helpers
    template<typename T>
    T* allocate_object() {
        void* ptr = allocate(sizeof(T));
        return static_cast<T*>(ptr);
    }
    
    template<typename T>
    void deallocate_object(T* ptr) {
        deallocate(ptr, sizeof(T));
    }
    
    // String allocation optimizations
    char* allocate_string(size_t length);
    void deallocate_string(char* str, size_t length);
    
    // Statistics and monitoring
    struct PoolStats {
        // Per-pool statistics
        struct {
            size_t allocated;
            size_t peak_usage;
            size_t capacity;
            double utilization;
        } small, medium, large, xlarge;
        
        // System allocator fallback
        size_t system_allocations;
        size_t system_bytes;
        
        // Overall statistics
        uint64_t total_allocations;
        uint64_t pool_hit_rate_percent;
        std::chrono::milliseconds uptime;
    };
    
    PoolStats get_stats() const;
    void reset_stats();
    
    // Memory pressure management
    bool is_under_pressure() const;
    void compact_pools();
    
    // Configuration
    void set_auto_compact(bool enable);
    void set_pressure_threshold(double threshold);

private:
    FixedSizePool<64, 1024>* get_pool_for_size(size_t size);
    void* allocate_from_system(size_t size);
    void deallocate_to_system(void* ptr);
    
    bool auto_compact_;
    double pressure_threshold_;
};

// RAII wrapper for pool-allocated objects
template<typename T>
class PoolAllocated {
private:
    T* ptr_;
    MemoryPoolManager* pool_;

public:
    template<typename... Args>
    PoolAllocated(MemoryPoolManager* pool, Args&&... args) 
        : pool_(pool) {
        ptr_ = pool_->allocate_object<T>();
        if (ptr_) {
            new(ptr_) T(std::forward<Args>(args)...);
        }
    }
    
    ~PoolAllocated() {
        if (ptr_) {
            ptr_->~T();
            pool_->deallocate_object(ptr_);
        }
    }
    
    // Non-copyable, movable
    PoolAllocated(const PoolAllocated&) = delete;
    PoolAllocated& operator=(const PoolAllocated&) = delete;
    
    PoolAllocated(PoolAllocated&& other) noexcept 
        : ptr_(other.ptr_), pool_(other.pool_) {
        other.ptr_ = nullptr;
    }
    
    PoolAllocated& operator=(PoolAllocated&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                ptr_->~T();
                pool_->deallocate_object(ptr_);
            }
            ptr_ = other.ptr_;
            pool_ = other.pool_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    T* get() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    
    T* release() {
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }
};

// Custom allocator for STL containers
template<typename T>
class PoolAllocator {
private:
    MemoryPoolManager* pool_;

public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = PoolAllocator<U>;
    };
    
    explicit PoolAllocator(MemoryPoolManager* pool) : pool_(pool) {}
    
    template<typename U>
    PoolAllocator(const PoolAllocator<U>& other) : pool_(other.pool_) {}
    
    pointer allocate(size_type n) {
        return static_cast<pointer>(pool_->allocate(n * sizeof(T)));
    }
    
    void deallocate(pointer p, size_type n) {
        pool_->deallocate(p, n * sizeof(T));
    }
    
    template<typename U>
    bool operator==(const PoolAllocator<U>& other) const {
        return pool_ == other.pool_;
    }
    
    template<typename U>
    bool operator!=(const PoolAllocator<U>& other) const {
        return !(*this == other);
    }
};

// String interning pool for common strings
class StringInternPool {
private:
    std::unordered_set<std::string> interned_strings_;
    mutable std::shared_mutex mutex_;
    std::atomic<size_t> intern_hits_{0};
    std::atomic<size_t> intern_misses_{0};

public:
    StringInternPool() = default;
    ~StringInternPool() = default;
    
    // Intern a string (returns string_view pointing to interned copy)
    std::string_view intern(const std::string& str);
    std::string_view intern(std::string&& str);
    
    // Check if string is already interned
    bool is_interned(const std::string& str) const;
    
    // Statistics
    size_t pool_size() const;
    size_t hit_rate_percent() const;
    void clear_pool();
    
    struct Stats {
        size_t pool_size;
        size_t total_requests;
        size_t cache_hits;
        size_t cache_misses;
        double hit_rate_percent;
        size_t estimated_memory_saved;
    };
    
    Stats get_stats() const;
};

// Global memory manager instance
extern std::unique_ptr<MemoryPoolManager> g_memory_manager;
extern std::unique_ptr<StringInternPool> g_string_intern_pool;

// Convenience functions
void* pool_malloc(size_t size);
void pool_free(void* ptr, size_t size);

template<typename T, typename... Args>
T* pool_new(Args&&... args) {
    T* ptr = static_cast<T*>(pool_malloc(sizeof(T)));
    if (ptr) {
        new(ptr) T(std::forward<Args>(args)...);
    }
    return ptr;
}

template<typename T>
void pool_delete(T* ptr) {
    if (ptr) {
        ptr->~T();
        pool_free(ptr, sizeof(T));
    }
}

// Memory usage monitoring
class MemoryMonitor {
private:
    std::atomic<size_t> current_usage_{0};
    std::atomic<size_t> peak_usage_{0};
    std::chrono::steady_clock::time_point start_time_;

public:
    MemoryMonitor();
    
    void record_allocation(size_t size);
    void record_deallocation(size_t size);
    
    size_t get_current_usage() const { return current_usage_.load(); }
    size_t get_peak_usage() const { return peak_usage_.load(); }
    std::chrono::milliseconds get_uptime() const;
    
    void reset();
};

extern std::unique_ptr<MemoryMonitor> g_memory_monitor;

} // namespace crossexpand
