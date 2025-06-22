#pragma once

#include "core/event_queue.hpp"
#include "core/advanced_template_engine.hpp"
#include "core/enhanced_text_injector.hpp"
#include "core/input_manager.hpp"  // Add this include
#include "utils/logger.hpp"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <memory>

namespace crossexpand {

// Thread types in the system
enum class ThreadType {
    INPUT_MONITOR,     // Keyboard input monitoring
    EVENT_PROCESSOR,   // Event processing and template expansion
    TEXT_INJECTOR,     // Text injection operations
    HEALTH_MONITOR,    // System health and performance monitoring
    CONFIG_WATCHER     // Configuration file watching
};

// Thread pool worker
class ThreadWorker {
private:
    ThreadType type_;
    std::thread thread_;
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_running_{false};
    std::string worker_name_;
    
    // Performance metrics
    std::atomic<uint64_t> tasks_processed_{0};
    std::atomic<uint64_t> total_processing_time_ms_{0};
    std::chrono::steady_clock::time_point start_time_;

public:
    ThreadWorker(ThreadType type, const std::string& name);
    ~ThreadWorker();
    
    // Lifecycle
    bool start();
    void stop();
    void join();
    
    // Status
    bool is_running() const { return is_running_.load(); }
    ThreadType get_type() const { return type_; }
    std::string get_name() const { return worker_name_; }
    
    // Metrics
    uint64_t get_tasks_processed() const { return tasks_processed_.load(); }
    double get_average_processing_time() const;
    std::chrono::milliseconds get_uptime() const;

protected:
    virtual void run() = 0;
    void increment_task_counter(std::chrono::milliseconds processing_time);
    bool should_stop() const { return should_stop_.load(); }
};

// Input monitoring worker
class InputMonitorWorker : public ThreadWorker {
private:
    std::shared_ptr<InputManager> input_manager_;
    std::shared_ptr<EventQueue> event_queue_;

public:
    InputMonitorWorker(std::shared_ptr<InputManager> input_mgr,
                      std::shared_ptr<EventQueue> queue);

protected:
    void run() override;
    bool handle_key_event(const KeyEvent& event);
};

// Event processing worker
class EventProcessorWorker : public ThreadWorker {
private:
    std::shared_ptr<EventQueue> event_queue_;
    std::shared_ptr<AdvancedTemplateEngine> template_engine_;
    std::shared_ptr<EnhancedTextInjector> text_injector_;
    
    // State management
    std::string current_sequence_;
    std::mutex sequence_mutex_;
    
    // Processing statistics
    std::atomic<uint64_t> expansions_performed_{0};
    std::atomic<uint64_t> template_lookups_{0};

public:
    EventProcessorWorker(std::shared_ptr<EventQueue> queue,
                        std::shared_ptr<AdvancedTemplateEngine> engine,
                        std::shared_ptr<EnhancedTextInjector> injector,
                        int worker_id);

protected:
    void run() override;
    bool process_event(const ProcessingEvent& event);
    bool check_for_template_trigger();
    bool expand_and_inject_template(const std::string& shortcut);
    
public:
    uint64_t get_expansions_performed() const { return expansions_performed_.load(); }
    uint64_t get_template_lookups() const { return template_lookups_.load(); }
};

// Health monitoring worker
class HealthMonitorWorker : public ThreadWorker {
private:
    std::vector<std::shared_ptr<ThreadWorker>> monitored_workers_;
    std::shared_ptr<EventQueue> event_queue_;
    std::chrono::seconds check_interval_{5};
    
    // Health thresholds
    double max_queue_utilization_{0.8};
    double min_success_rate_{0.95};
    std::chrono::seconds max_response_time_{1};

public:
    HealthMonitorWorker(std::vector<std::shared_ptr<ThreadWorker>> workers,
                       std::shared_ptr<EventQueue> queue);

protected:
    void run() override;
    void check_system_health();
    void check_worker_health(const ThreadWorker& worker);
    void check_queue_health();
    void check_memory_usage();
    void log_performance_summary();
};

// Main multi-threaded processing engine
class MultiThreadedProcessor {
private:
    // Core components
    std::shared_ptr<InputManager> input_manager_;
    std::shared_ptr<AdvancedTemplateEngine> template_engine_;
    std::shared_ptr<EnhancedTextInjector> text_injector_;
    std::shared_ptr<EventQueue> event_queue_;
    
    // Worker threads
    std::vector<std::shared_ptr<ThreadWorker>> workers_;
    std::shared_ptr<InputMonitorWorker> input_worker_;
    std::vector<std::shared_ptr<EventProcessorWorker>> processor_workers_;
    std::shared_ptr<HealthMonitorWorker> health_worker_;
    
    // Configuration
    size_t num_processor_threads_;
    bool enable_health_monitoring_;
    
    // State
    std::atomic<bool> is_running_{false};
    mutable std::mutex state_mutex_;

public:
    MultiThreadedProcessor(size_t num_threads = std::thread::hardware_concurrency());
    ~MultiThreadedProcessor();
    
    // Lifecycle
    bool initialize();
    bool start();
    void stop();
    void shutdown();
    
    // Component access
    std::shared_ptr<AdvancedTemplateEngine> get_template_engine() const;
    std::shared_ptr<EnhancedTextInjector> get_text_injector() const;
    std::shared_ptr<EventQueue> get_event_queue() const;
    
    // Configuration
    void set_processor_thread_count(size_t count);
    void enable_health_monitoring(bool enable);
    
    // Status and metrics
    bool is_running() const { return is_running_.load(); }
    
    struct SystemStats {
        size_t total_threads;
        size_t active_threads;
        uint64_t total_events_processed;
        uint64_t total_expansions_performed;
        double events_per_second;
        double expansions_per_second;
        std::chrono::milliseconds average_processing_time;
        EventQueue::Stats queue_stats;
    };
    
    SystemStats get_system_stats() const;
    
    // Performance tuning
    void optimize_for_workload();
    void reset_performance_counters();

private:
    void create_workers();
    void start_workers();
    void stop_workers();
    void cleanup_workers();
    
    // Worker management
    bool restart_failed_worker(size_t worker_index);
    void monitor_worker_health();
};

// Thread-safe performance collector
class PerformanceCollector {
private:
    struct MetricPoint {
        std::chrono::steady_clock::time_point timestamp;
        std::string metric_name;
        double value;
        std::unordered_map<std::string, std::string> tags;
    };
    
    std::vector<MetricPoint> metrics_;
    mutable std::shared_mutex metrics_mutex_;
    std::chrono::steady_clock::time_point start_time_;

public:
    PerformanceCollector();
    
    void record_metric(const std::string& name, double value,
                      const std::unordered_map<std::string, std::string>& tags = {});
    
    void record_timing(const std::string& name, std::chrono::milliseconds duration,
                      const std::unordered_map<std::string, std::string>& tags = {});
    
    void record_counter(const std::string& name, uint64_t count,
                       const std::unordered_map<std::string, std::string>& tags = {});
    
    // Statistics
    double get_average(const std::string& metric_name, 
                      std::chrono::seconds window = std::chrono::seconds(60)) const;
    
    double get_max(const std::string& metric_name,
                   std::chrono::seconds window = std::chrono::seconds(60)) const;
    
    size_t get_count(const std::string& metric_name,
                     std::chrono::seconds window = std::chrono::seconds(60)) const;
    
    // Export
    std::string export_to_json(std::chrono::seconds window = std::chrono::seconds(300)) const;
    void export_to_file(const std::string& filename) const;
    
    void clear_old_metrics(std::chrono::seconds max_age = std::chrono::hours(1));
};

// Global performance collector instance
extern std::unique_ptr<PerformanceCollector> g_performance_collector;

// RAII timing helper
class ScopedTimer {
private:
    std::string metric_name_;
    std::chrono::steady_clock::time_point start_time_;
    std::unordered_map<std::string, std::string> tags_;

public:
    ScopedTimer(const std::string& name, 
               const std::unordered_map<std::string, std::string>& tags = {})
        : metric_name_(name), start_time_(std::chrono::steady_clock::now()), tags_(tags) {}
    
    ~ScopedTimer() {
        auto duration = std::chrono::steady_clock::now() - start_time_;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        
        if (g_performance_collector) {
            g_performance_collector->record_timing(metric_name_, ms, tags_);
        }
    }
};

#define SCOPED_TIMER(name) ScopedTimer _timer(name)
#define SCOPED_TIMER_WITH_TAGS(name, tags) ScopedTimer _timer(name, tags)

} // namespace crossexpand
