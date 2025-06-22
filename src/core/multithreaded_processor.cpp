#include "core/multithreaded_processor.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <functional>

namespace crossexpand {

// ThreadWorker Implementation
ThreadWorker::ThreadWorker(ThreadType type, const std::string& name)
    : type_(type), worker_name_(name) {
    start_time_ = std::chrono::steady_clock::now();
    LOG_DEBUG("ThreadWorker '{}' created for type {}", name, static_cast<int>(type));
}

ThreadWorker::~ThreadWorker() {
    if (is_running_.load()) {
        stop();
        join();
    }
    LOG_DEBUG("ThreadWorker '{}' destroyed", worker_name_);
}

bool ThreadWorker::start() {
    if (is_running_.load()) {
        LOG_WARNING("ThreadWorker '{}' already running", worker_name_);
        return false;
    }

    try {
        should_stop_.store(false);
        thread_ = std::thread(&ThreadWorker::run, this);
        
        // Wait for thread to actually start
        auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        while (!is_running_.load() && std::chrono::steady_clock::now() < timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        if (is_running_.load()) {
            LOG_INFO("ThreadWorker '{}' started successfully", worker_name_);
            return true;
        } else {
            LOG_ERROR("ThreadWorker '{}' failed to start within timeout", worker_name_);
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start ThreadWorker '{}': {}", worker_name_, e.what());
        return false;
    }
}

void ThreadWorker::stop() {
    if (!is_running_.load()) {
        return;
    }
    
    LOG_DEBUG("Stopping ThreadWorker '{}'", worker_name_);
    should_stop_.store(true);
}

void ThreadWorker::join() {
    if (thread_.joinable()) {
        thread_.join();
        LOG_DEBUG("ThreadWorker '{}' joined", worker_name_);
    }
}

PerformanceStats ThreadWorker::get_stats() const {
    PerformanceStats stats;
    stats.tasks_processed = tasks_processed_.load();
    stats.total_processing_time_ms = total_processing_time_ms_.load();
    
    auto now = std::chrono::steady_clock::now();
    auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time_).count();
    stats.uptime_ms = uptime_ms;
    
    if (stats.tasks_processed > 0) {
        stats.average_processing_time_ms = 
            static_cast<double>(stats.total_processing_time_ms) / stats.tasks_processed;
    }
    
    return stats;
}

void ThreadWorker::run() {
    LOG_DEBUG("ThreadWorker '{}' thread started", worker_name_);
    is_running_.store(true);
    
    while (!should_stop_.load()) {
        auto task_start = std::chrono::steady_clock::now();
        
        // Execute thread-type specific work
        bool worked = execute_work();
        
        if (worked) {
            auto task_end = std::chrono::steady_clock::now();
            auto task_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                task_end - task_start).count();
            
            tasks_processed_.fetch_add(1);
            total_processing_time_ms_.fetch_add(task_duration);
        } else {
            // No work available, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    is_running_.store(false);
    LOG_DEBUG("ThreadWorker '{}' thread finished", worker_name_);
}

bool ThreadWorker::execute_work() {
    // Base implementation - derived classes should override
    switch (type_) {
        case ThreadType::INPUT_MONITOR:
            return execute_input_monitoring();
        case ThreadType::EVENT_PROCESSOR:
            return execute_event_processing();
        case ThreadType::TEXT_INJECTOR:
            return execute_text_injection();
        case ThreadType::HEALTH_MONITOR:
            return execute_health_monitoring();
        case ThreadType::CONFIG_WATCHER:
            return execute_config_watching();
        default:
            LOG_WARNING("Unknown thread type for worker '{}'", worker_name_);
            return false;
    }
}

bool ThreadWorker::execute_input_monitoring() {
    // Placeholder for input monitoring work
    // In a real implementation, this would interface with the InputManager
    return false;
}

bool ThreadWorker::execute_event_processing() {
    // Placeholder for event processing work  
    // In a real implementation, this would process events from the EventQueue
    return false;
}

bool ThreadWorker::execute_text_injection() {
    // Placeholder for text injection work
    // In a real implementation, this would interface with the TextInjector
    return false;
}

bool ThreadWorker::execute_health_monitoring() {
    // Health monitoring work - always has something to do
    // Collect system metrics, check thread health, etc.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

bool ThreadWorker::execute_config_watching() {
    // Config watching work - check for file changes
    // In a real implementation, this would watch config files for changes
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return true;
}

// MultithreadedProcessor Implementation
MultithreadedProcessor::MultithreadedProcessor()
    : event_queue_(std::make_unique<EventQueue>())
    , template_engine_(std::make_unique<AdvancedTemplateEngine>())
    , text_injector_(std::make_unique<EnhancedTextInjector>()) {
    LOG_INFO("MultithreadedProcessor created");
}

MultithreadedProcessor::~MultithreadedProcessor() {
    shutdown();
    LOG_INFO("MultithreadedProcessor destroyed");
}

bool MultithreadedProcessor::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_running_.load()) {
        LOG_WARNING("MultithreadedProcessor already initialized");
        return true;
    }
    
    LOG_INFO("Initializing MultithreadedProcessor...");
    
    try {
        // Initialize core components
        if (!template_engine_->initialize()) {
            LOG_ERROR("Failed to initialize AdvancedTemplateEngine");
            return false;
        }
        
        if (!text_injector_->initialize()) {
            LOG_ERROR("Failed to initialize EnhancedTextInjector");
            return false;
        }
        
        // Create and start worker threads
        create_worker_threads();
        
        if (!start_worker_threads()) {
            LOG_ERROR("Failed to start worker threads");
            return false;
        }
        
        is_running_.store(true);
        start_time_ = std::chrono::steady_clock::now();
        
        LOG_INFO("MultithreadedProcessor initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during MultithreadedProcessor initialization: {}", e.what());
        cleanup();
        return false;
    }
}

void MultithreadedProcessor::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_running_.load()) {
        return;
    }
    
    LOG_INFO("Shutting down MultithreadedProcessor...");
    
    is_running_.store(false);
    
    // Stop all worker threads
    stop_worker_threads();
    
    // Wait for threads to finish
    join_worker_threads();
    
    // Cleanup components
    cleanup();
    
    LOG_INFO("MultithreadedProcessor shutdown complete");
}

ProcessorStats MultithreadedProcessor::get_stats() const {
    ProcessorStats stats;
    
    // System uptime
    auto now = std::chrono::steady_clock::now();
    if (start_time_ != std::chrono::steady_clock::time_point{}) {
        auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time_).count();
        stats.uptime_ms = uptime_ms;
    }
    
    // Queue statistics
    if (event_queue_) {
        stats.queue_size = event_queue_->size();
        auto queue_stats = event_queue_->get_stats();
        stats.events_processed = queue_stats.total_processed;
        stats.events_dropped = queue_stats.total_dropped;
    }
    
    // Template engine statistics
    if (template_engine_) {
        auto engine_stats = template_engine_->get_stats();
        stats.templates_expanded = engine_stats.expansions_performed;
        stats.template_cache_hits = engine_stats.cache_hits;
        stats.template_cache_misses = engine_stats.cache_misses;
    }
    
    // Thread statistics
    stats.active_threads = 0;
    for (const auto& worker : workers_) {
        if (worker && worker->is_running()) {
            stats.active_threads++;
        }
    }
    
    stats.is_healthy = is_running_.load() && (stats.active_threads > 0);
    
    return stats;
}

void MultithreadedProcessor::create_worker_threads() {
    workers_.clear();
    
    // Create different types of worker threads
    workers_.push_back(std::make_unique<ThreadWorker>(
        ThreadType::INPUT_MONITOR, "InputMonitor"));
    
    workers_.push_back(std::make_unique<ThreadWorker>(
        ThreadType::EVENT_PROCESSOR, "EventProcessor-1"));
    
    workers_.push_back(std::make_unique<ThreadWorker>(
        ThreadType::EVENT_PROCESSOR, "EventProcessor-2"));
    
    workers_.push_back(std::make_unique<ThreadWorker>(
        ThreadType::TEXT_INJECTOR, "TextInjector"));
    
    workers_.push_back(std::make_unique<ThreadWorker>(
        ThreadType::HEALTH_MONITOR, "HealthMonitor"));
    
    workers_.push_back(std::make_unique<ThreadWorker>(
        ThreadType::CONFIG_WATCHER, "ConfigWatcher"));
    
    LOG_INFO("Created {} worker threads", workers_.size());
}

bool MultithreadedProcessor::start_worker_threads() {
    bool all_started = true;
    
    for (auto& worker : workers_) {
        if (!worker->start()) {
            LOG_ERROR("Failed to start worker thread '{}'", worker->get_name());
            all_started = false;
        }
    }
    
    if (all_started) {
        LOG_INFO("All {} worker threads started successfully", workers_.size());
    } else {
        LOG_ERROR("Some worker threads failed to start");
    }
    
    return all_started;
}

void MultithreadedProcessor::stop_worker_threads() {
    LOG_DEBUG("Stopping {} worker threads...", workers_.size());
    
    for (auto& worker : workers_) {
        if (worker) {
            worker->stop();
        }
    }
}

void MultithreadedProcessor::join_worker_threads() {
    LOG_DEBUG("Joining {} worker threads...", workers_.size());
    
    for (auto& worker : workers_) {
        if (worker) {
            worker->join();
        }
    }
    
    LOG_DEBUG("All worker threads joined");
}

void MultithreadedProcessor::cleanup() {
    // Clear workers
    workers_.clear();
    
    // Shutdown components
    if (text_injector_) {
        text_injector_->shutdown();
    }
    
    if (template_engine_) {
        template_engine_->shutdown();
    }
    
    // Clear components
    text_injector_.reset();
    template_engine_.reset();
    event_queue_.reset();
}

// TimingHelper Implementation
TimingHelper::TimingHelper(const std::string& operation_name)
    : operation_name_(operation_name)
    , start_time_(std::chrono::high_resolution_clock::now()) {
}

TimingHelper::~TimingHelper() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time_).count();
    
    LOG_DEBUG("Operation '{}' took {} μs", operation_name_, duration);
}

std::chrono::microseconds TimingHelper::elapsed() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_);
}

} // namespace crossexpand
