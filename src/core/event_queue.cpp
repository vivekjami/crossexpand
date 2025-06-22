#include "core/event_queue.hpp"
#include "utils/logger.hpp"
#include <algorithm>

namespace crossexpand {

EventQueue::EventQueue() 
    : start_time_(std::chrono::steady_clock::now()) {
    for (auto& counter : drops_by_priority_) {
        counter.store(0);
    }
    LOG_DEBUG("EventQueue initialized with {} priority levels", NUM_PRIORITIES);
}

bool EventQueue::push(const ProcessingEvent& event) {
    const size_t priority_index = static_cast<size_t>(event.priority);
    
    if (priority_index >= NUM_PRIORITIES) {
        LOG_ERROR("Invalid priority level: {}", priority_index);
        return false;
    }
    
    ProcessingEvent event_copy = event;
    event_copy.sequence_id = sequence_counter_.fetch_add(1, std::memory_order_relaxed);
    
    if (queues_[priority_index].try_push(event_copy)) {
        total_pushed_.fetch_add(1, std::memory_order_relaxed);
        return true;
    } else {
        // Queue full, increment drop counter
        drops_by_priority_[priority_index].fetch_add(1, std::memory_order_relaxed);
        LOG_WARNING("Event queue full for priority {}, dropping event", priority_index);
        return false;
    }
}

bool EventQueue::push(const SimpleKeyEvent& key_event, EventPriority priority) {
    ProcessingEvent event(key_event, priority);
    return push(event);
}

bool EventQueue::pop(ProcessingEvent& event) {
    // Check queues from highest to lowest priority
    for (int priority = NUM_PRIORITIES - 1; priority >= 0; --priority) {
        if (queues_[priority].try_pop(event)) {
            total_popped_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    return false; // All queues empty
}

EventQueue::Stats EventQueue::get_stats() const {
    Stats stats;
    stats.total_pushed = total_pushed_.load(std::memory_order_relaxed);
    stats.total_popped = total_popped_.load(std::memory_order_relaxed);
    
    stats.total_dropped = 0;
    for (size_t i = 0; i < NUM_PRIORITIES; ++i) {
        stats.drops_by_priority[i] = drops_by_priority_[i].load(std::memory_order_relaxed);
        stats.total_dropped += stats.drops_by_priority[i];
        stats.current_sizes[i] = queues_[i].size();
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    stats.uptime_seconds = duration.count() / 1000.0;
    
    if (stats.uptime_seconds > 0) {
        stats.events_per_second = stats.total_popped / stats.uptime_seconds;
    } else {
        stats.events_per_second = 0.0;
    }
    
    return stats;
}

void EventQueue::reset_stats() {
    total_pushed_.store(0);
    total_popped_.store(0);
    for (auto& counter : drops_by_priority_) {
        counter.store(0);
    }
    start_time_ = std::chrono::steady_clock::now();
    LOG_INFO("EventQueue statistics reset");
}

size_t EventQueue::total_size() const {
    size_t total = 0;
    for (const auto& queue : queues_) {
        total += queue.size();
    }
    return total;
}

bool EventQueue::is_healthy() const {
    // Queue is healthy if:
    // 1. No queue is completely full
    // 2. Drop rate is low
    // 3. Processing keeps up with input
    
    for (const auto& queue : queues_) {
        if (queue.full()) {
            return false;
        }
    }
    
    auto stats = get_stats();
    if (stats.total_pushed > 1000) {
        double drop_rate = static_cast<double>(stats.total_dropped) / stats.total_pushed;
        if (drop_rate > 0.01) { // More than 1% drop rate is unhealthy
            return false;
        }
    }
    
    return true;
}

double EventQueue::utilization() const {
    size_t total_capacity = 0;
    size_t total_used = 0;
    
    for (const auto& queue : queues_) {
        total_capacity += queue.capacity();
        total_used += queue.size();
    }
    
    if (total_capacity == 0) return 0.0;
    return static_cast<double>(total_used) / total_capacity;
}

} // namespace crossexpand
