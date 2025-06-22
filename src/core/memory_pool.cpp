#include "core/memory_pool.hpp"
#include "utils/logger.hpp"
#include <cstring>
#include <algorithm>
#include <shared_mutex>

namespace crossexpand {

// Global instances
std::unique_ptr<MemoryPoolManager> g_memory_manager;
std::unique_ptr<StringInternPool> g_string_intern_pool;
std::unique_ptr<MemoryMonitor> g_memory_monitor;

// MemoryPoolManager Implementation
MemoryPoolManager::MemoryPoolManager() 
    : start_time_(std::chrono::steady_clock::now())
    , auto_compact_(false)
    , pressure_threshold_(0.8) {
    
    small_pool_ = std::make_unique<FixedSizePool<64, 1024>>();
    medium_pool_ = std::make_unique<FixedSizePool<256, 512>>();
    large_pool_ = std::make_unique<FixedSizePool<1024, 256>>();
    xlarge_pool_ = std::make_unique<FixedSizePool<4096, 64>>();
    
    LOG_INFO("MemoryPoolManager initialized with 4 pool sizes");
}

void* MemoryPoolManager::allocate(size_t size) {
    total_allocations_.fetch_add(1, std::memory_order_relaxed);
    
    if (auto* pool = get_pool_for_size(size)) {
        void* ptr = pool->allocate();
        if (ptr) {
            pool_allocations_.fetch_add(1, std::memory_order_relaxed);
            return ptr;
        }
    }
    
    // Fallback to system allocator
    return allocate_from_system(size);
}

void MemoryPoolManager::deallocate(void* ptr, size_t size) {
    if (!ptr) return;
    
    if (auto* pool = get_pool_for_size(size)) {
        if (pool->is_from_pool(ptr)) {
            pool->deallocate(ptr);
            return;
        }
    }
    
    // System allocated
    deallocate_to_system(ptr);
}

FixedSizePool<64, 1024>* MemoryPoolManager::get_pool_for_size(size_t size) {
    if (size <= 64) return small_pool_.get();
    if (size <= 256) return reinterpret_cast<FixedSizePool<64, 1024>*>(medium_pool_.get());
    if (size <= 1024) return reinterpret_cast<FixedSizePool<64, 1024>*>(large_pool_.get());
    if (size <= 4096) return reinterpret_cast<FixedSizePool<64, 1024>*>(xlarge_pool_.get());
    return nullptr;
}

void* MemoryPoolManager::allocate_from_system(size_t size) {
    system_allocations_.fetch_add(1, std::memory_order_relaxed);
    total_system_bytes_.fetch_add(size, std::memory_order_relaxed);
    
    void* ptr = std::malloc(size);
    if (!ptr) {
        LOG_ERROR("System allocation failed for {} bytes", size);
        throw std::bad_alloc();
    }
    
    return ptr;
}

void MemoryPoolManager::deallocate_to_system(void* ptr) {
    system_deallocations_.fetch_add(1, std::memory_order_relaxed);
    std::free(ptr);
}

MemoryPoolManager::PoolStats MemoryPoolManager::get_stats() const {
    PoolStats stats;
    
    // Small pool stats
    stats.small.allocated = small_pool_->allocated_count();
    stats.small.peak_usage = small_pool_->peak_usage();
    stats.small.capacity = small_pool_->capacity();
    stats.small.utilization = small_pool_->utilization();
    
    // Medium pool stats
    stats.medium.allocated = medium_pool_->allocated_count();
    stats.medium.peak_usage = medium_pool_->peak_usage();
    stats.medium.capacity = medium_pool_->capacity();
    stats.medium.utilization = medium_pool_->utilization();
    
    // Large pool stats
    stats.large.allocated = large_pool_->allocated_count();
    stats.large.peak_usage = large_pool_->peak_usage();
    stats.large.capacity = large_pool_->capacity();
    stats.large.utilization = large_pool_->utilization();
    
    // XLarge pool stats
    stats.xlarge.allocated = xlarge_pool_->allocated_count();
    stats.xlarge.peak_usage = xlarge_pool_->peak_usage();
    stats.xlarge.capacity = xlarge_pool_->capacity();
    stats.xlarge.utilization = xlarge_pool_->utilization();
    
    // System stats
    stats.system_allocations = system_allocations_.load();
    stats.system_bytes = total_system_bytes_.load();
    
    // Overall stats
    stats.total_allocations = total_allocations_.load();
    uint64_t pool_allocs = pool_allocations_.load();
    
    if (stats.total_allocations > 0) {
        stats.pool_hit_rate_percent = (pool_allocs * 100) / stats.total_allocations;
    } else {
        stats.pool_hit_rate_percent = 100;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    stats.uptime = duration;
    
    return stats;
}

bool MemoryPoolManager::is_under_pressure() const {
    auto stats = get_stats();
    
    return (stats.small.utilization > pressure_threshold_ ||
            stats.medium.utilization > pressure_threshold_ ||
            stats.large.utilization > pressure_threshold_ ||
            stats.xlarge.utilization > pressure_threshold_);
}

// StringInternPool Implementation
std::string_view StringInternPool::intern(const std::string& str) {
    {
        std::shared_lock<std::shared_mutex> read_lock(mutex_);
        auto it = interned_strings_.find(str);
        if (it != interned_strings_.end()) {
            intern_hits_.fetch_add(1, std::memory_order_relaxed);
            return std::string_view(*it);
        }
    }
    
    // Not found, need to intern
    {
        std::lock_guard<std::shared_mutex> write_lock(mutex_);
        auto [it, inserted] = interned_strings_.insert(str);
        intern_misses_.fetch_add(1, std::memory_order_relaxed);
        return std::string_view(*it);
    }
}

size_t StringInternPool::pool_size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return interned_strings_.size();
}

size_t StringInternPool::hit_rate_percent() const {
    uint64_t hits = intern_hits_.load();
    uint64_t misses = intern_misses_.load();
    uint64_t total = hits + misses;
    
    if (total == 0) return 100;
    return (hits * 100) / total;
}

// MemoryMonitor Implementation
MemoryMonitor::MemoryMonitor() 
    : start_time_(std::chrono::steady_clock::now()) {
    LOG_DEBUG("MemoryMonitor initialized");
}

void MemoryMonitor::record_allocation(size_t size) {
    size_t current = current_usage_.fetch_add(size, std::memory_order_relaxed) + size;
    
    // Update peak usage atomically
    size_t peak = peak_usage_.load(std::memory_order_relaxed);
    while (current > peak && !peak_usage_.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
        // Retry if another thread updated peak_usage_
    }
}

void MemoryMonitor::record_deallocation(size_t size) {
    current_usage_.fetch_sub(size, std::memory_order_relaxed);
}

std::chrono::milliseconds MemoryMonitor::get_uptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
}

void MemoryMonitor::reset() {
    current_usage_.store(0);
    peak_usage_.store(0);
    start_time_ = std::chrono::steady_clock::now();
}

// Convenience functions
void* pool_malloc(size_t size) {
    if (g_memory_manager) {
        return g_memory_manager->allocate(size);
    }
    return std::malloc(size);
}

void pool_free(void* ptr, size_t size) {
    if (g_memory_manager) {
        g_memory_manager->deallocate(ptr, size);
    } else {
        std::free(ptr);
    }
}

} // namespace crossexpand
