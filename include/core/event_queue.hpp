#pragma once

#include <atomic>
#include <memory>
#include <chrono>
#include <array>

namespace crossexpand {

// Simple key event for internal use in EventQueue
struct SimpleKeyEvent {
    int keycode;
    char character;
    bool is_pressed;
    std::chrono::steady_clock::time_point timestamp;
    
    SimpleKeyEvent() : keycode(0), character(0), is_pressed(false) {}
    SimpleKeyEvent(int key, char ch, bool pressed) 
        : keycode(key), character(ch), is_pressed(pressed)
        , timestamp(std::chrono::steady_clock::now()) {}
};

enum class EventPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

struct ProcessingEvent {
    SimpleKeyEvent key_event;  // Use SimpleKeyEvent instead of forward-declared KeyEvent
    EventPriority priority;
    std::chrono::steady_clock::time_point timestamp;
    uint64_t sequence_id;
    
    ProcessingEvent() = default;
    ProcessingEvent(const SimpleKeyEvent& ke, EventPriority prio = EventPriority::NORMAL)
        : key_event(ke), priority(prio), 
          timestamp(std::chrono::steady_clock::now()),
          sequence_id(0) {}
};

// Lock-free SPSC (Single Producer Single Consumer) queue
template<typename T, size_t Capacity>
class LockFreeQueue {
private:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    struct alignas(64) Slot {
        std::atomic<bool> ready{false};
        T data;
    };
    
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    std::array<Slot, Capacity> slots_;

public:
    LockFreeQueue() = default;
    ~LockFreeQueue() = default;
    
    // Non-copyable, non-movable
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    
    bool try_push(const T& item) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & (Capacity - 1);
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }
        
        slots_[head].data = item;
        slots_[head].ready.store(true, std::memory_order_release);
        head_.store(next_head, std::memory_order_release);
        return true;
    }
    
    bool try_pop(T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        
        if (!slots_[tail].ready.load(std::memory_order_acquire)) {
            return false; // Queue is empty
        }
        
        item = std::move(slots_[tail].data);
        slots_[tail].ready.store(false, std::memory_order_release);
        tail_.store((tail + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }
    
    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & (Capacity - 1);
    }
    
    bool empty() const {
        return size() == 0;
    }
    
    bool full() const {
        return size() == Capacity - 1;
    }
    
    size_t capacity() const {
        return Capacity - 1; // One slot reserved for full/empty distinction
    }
};

// Multi-priority event queue with statistics
class EventQueue {
private:
    static constexpr size_t QUEUE_SIZE = 4096;
    static constexpr size_t NUM_PRIORITIES = 4;
    
    std::array<LockFreeQueue<ProcessingEvent, QUEUE_SIZE>, NUM_PRIORITIES> queues_;
    
    // Statistics
    alignas(64) std::atomic<uint64_t> total_pushed_{0};
    alignas(64) std::atomic<uint64_t> total_popped_{0};
    alignas(64) std::atomic<uint64_t> drops_by_priority_[NUM_PRIORITIES];
    alignas(64) std::atomic<uint64_t> sequence_counter_{0};
    
    std::chrono::steady_clock::time_point start_time_;

public:
    EventQueue();
    ~EventQueue() = default;
    
    // Push event with priority
    bool push(const ProcessingEvent& event);
    bool push(const SimpleKeyEvent& key_event, EventPriority priority = EventPriority::NORMAL);
    
    // Pop highest priority event
    bool pop(ProcessingEvent& event);
    
    // Statistics
    struct Stats {
        uint64_t total_pushed;
        uint64_t total_popped;
        uint64_t total_dropped;
        std::array<uint64_t, NUM_PRIORITIES> drops_by_priority;
        std::array<size_t, NUM_PRIORITIES> current_sizes;
        double uptime_seconds;
        double events_per_second;
    };
    
    Stats get_stats() const;
    void reset_stats();
    
    // Queue health
    size_t total_size() const;
    bool is_healthy() const;
    double utilization() const;
};

} // namespace crossexpand
