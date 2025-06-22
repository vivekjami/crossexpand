#include "utils/performance_monitor.hpp"
#include "utils/logger.hpp"
#include <sstream>
#include <algorithm>
#include <fstream>
#include <thread>

#if defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#endif

namespace crossexpand {

PerformanceMonitor::PerformanceMonitor() : start_time_(std::chrono::steady_clock::now()) {
    // Initialize system metrics
    events_processed_ = std::make_unique<CounterMetric>("events_processed");
    templates_expanded_ = std::make_unique<CounterMetric>("templates_expanded");
    text_injections_ = std::make_unique<CounterMetric>("text_injections");
    errors_occurred_ = std::make_unique<CounterMetric>("errors_occurred");
    
    memory_usage_mb_ = std::make_unique<GaugeMetric>("memory_usage_mb");
    cpu_usage_percent_ = std::make_unique<GaugeMetric>("cpu_usage_percent");
    queue_size_ = std::make_unique<GaugeMetric>("queue_size");
    active_threads_ = std::make_unique<GaugeMetric>("active_threads");
    
    template_expansion_time_ = std::make_unique<TimerMetric>("template_expansion_time");
    text_injection_time_ = std::make_unique<TimerMetric>("text_injection_time");
    event_processing_time_ = std::make_unique<TimerMetric>("event_processing_time");
    
    LOG_INFO("PerformanceMonitor initialized");
}

CounterMetric& PerformanceMonitor::counter(const std::string& name) {
    return get_metric<CounterMetric>(name);
}

GaugeMetric& PerformanceMonitor::gauge(const std::string& name) {
    return get_metric<GaugeMetric>(name);
}

HistogramMetric& PerformanceMonitor::histogram(const std::string& name) {
    return get_metric<HistogramMetric>(name);
}

TimerMetric& PerformanceMonitor::timer(const std::string& name) {
    return get_metric<TimerMetric>(name);
}

std::vector<std::string> PerformanceMonitor::get_all_metrics() const {
    std::vector<std::string> result;
    
    if (!enabled_.load()) {
        return result;
    }
    
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    
    // Add system metrics
    result.push_back(events_processed_->to_string());
    result.push_back(templates_expanded_->to_string());
    result.push_back(text_injections_->to_string());
    result.push_back(errors_occurred_->to_string());
    
    result.push_back(memory_usage_mb_->to_string());
    result.push_back(cpu_usage_percent_->to_string());
    result.push_back(queue_size_->to_string());
    result.push_back(active_threads_->to_string());
    
    result.push_back(template_expansion_time_->to_string());
    result.push_back(text_injection_time_->to_string());
    result.push_back(event_processing_time_->to_string());
    
    // Add custom metrics
    for (const auto& pair : metrics_) {
        result.push_back(pair.second->to_string());
    }
    
    return result;
}

std::string PerformanceMonitor::generate_report() const {
    if (!enabled_.load()) {
        return "Performance monitoring disabled";
    }
    
    std::ostringstream oss;
    oss << "=== CrossExpand Performance Report ===\n";
    oss << "Uptime: " << uptime().count() << "ms\n";
    oss << "Status: " << (enabled_.load() ? "Enabled" : "Disabled") << "\n\n";
    
    auto metrics = get_all_metrics();
    
    oss << "System Metrics:\n";
    for (const auto& metric : metrics) {
        if (metric.find("events_processed") != std::string::npos ||
            metric.find("templates_expanded") != std::string::npos ||
            metric.find("text_injections") != std::string::npos ||
            metric.find("errors_occurred") != std::string::npos ||
            metric.find("memory_usage_mb") != std::string::npos ||
            metric.find("cpu_usage_percent") != std::string::npos ||
            metric.find("queue_size") != std::string::npos ||
            metric.find("active_threads") != std::string::npos) {
            oss << "  " << metric << "\n";
        }
    }
    
    oss << "\nTiming Metrics:\n";
    for (const auto& metric : metrics) {
        if (metric.find("_time") != std::string::npos) {
            oss << "  " << metric << "\n";
        }
    }
    
    oss << "\nCustom Metrics:\n";
    for (const auto& metric : metrics) {
        // Skip system metrics we already printed
        if (metric.find("events_processed") == std::string::npos &&
            metric.find("templates_expanded") == std::string::npos &&
            metric.find("text_injections") == std::string::npos &&
            metric.find("errors_occurred") == std::string::npos &&
            metric.find("memory_usage_mb") == std::string::npos &&
            metric.find("cpu_usage_percent") == std::string::npos &&
            metric.find("queue_size") == std::string::npos &&
            metric.find("active_threads") == std::string::npos &&
            metric.find("_time") == std::string::npos) {
            oss << "  " << metric << "\n";
        }
    }
    
    oss << "\n=== End Report ===\n";
    return oss.str();
}

void PerformanceMonitor::reset_all() {
    if (!enabled_.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Reset system metrics
    events_processed_->reset();
    templates_expanded_->reset();
    text_injections_->reset();
    errors_occurred_->reset();
    
    memory_usage_mb_->reset();
    cpu_usage_percent_->reset();
    queue_size_->reset();
    active_threads_->reset();
    
    template_expansion_time_->reset();
    text_injection_time_->reset();
    event_processing_time_->reset();
    
    // Reset custom metrics
    for (auto& pair : metrics_) {
        pair.second->reset();
    }
    
    start_time_ = std::chrono::steady_clock::now();
    LOG_INFO("All performance metrics reset");
}

std::chrono::milliseconds PerformanceMonitor::uptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
}

void PerformanceMonitor::update_system_metrics() {
    if (!enabled_.load()) {
        return;
    }
    
    try {
        // Update memory usage
        memory_usage_mb_->set(get_memory_usage_mb());
        
        // Update CPU usage
        cpu_usage_percent_->set(get_cpu_usage_percent());
        
        // Update thread count
        active_threads_->set(std::thread::hardware_concurrency());
        
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to update system metrics: {}", e.what());
    }
}

double PerformanceMonitor::get_memory_usage_mb() const {
#if defined(__linux__)
    // Read from /proc/self/status
    std::ifstream status_file("/proc/self/status");
    std::string line;
    
    while (std::getline(status_file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            // Extract the number (in KB)
            std::istringstream iss(line.substr(6));
            long kb;
            if (iss >> kb) {
                return kb / 1024.0; // Convert to MB
            }
            break;
        }
    }
#endif
    
    // Fallback - use rusage if available
#if defined(__linux__) || defined(__APPLE__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__linux__)
        return usage.ru_maxrss / 1024.0; // Linux reports in KB
#elif defined(__APPLE__)
        return usage.ru_maxrss / (1024.0 * 1024.0); // macOS reports in bytes
#endif
    }
#endif
    
    return 0.0; // Unable to determine
}

double PerformanceMonitor::get_cpu_usage_percent() const {
    // This is a simplified implementation
    // A real implementation would track CPU time over intervals
    
#if defined(__linux__)
    static auto last_check = std::chrono::steady_clock::now();
    static long last_cpu_time = 0;
    
    auto now = std::chrono::steady_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_check).count();
    
    // Only check every second to get meaningful CPU usage
    if (time_diff < 1000) {
        return cpu_usage_percent_->get(); // Return cached value
    }
    
    last_check = now;
    
    // Read CPU time from /proc/self/stat
    std::ifstream stat_file("/proc/self/stat");
    std::string line;
    if (std::getline(stat_file, line)) {
        std::istringstream iss(line);
        std::string token;
        
        // Skip to the 14th and 15th fields (utime and stime)
        for (int i = 0; i < 13; ++i) {
            iss >> token;
        }
        
        long utime, stime;
        if (iss >> utime >> stime) {
            long current_cpu_time = utime + stime;
            
            if (last_cpu_time > 0) {
                long cpu_diff = current_cpu_time - last_cpu_time;
                // Convert to percentage (very rough approximation)
                double cpu_percent = (cpu_diff * 100.0) / (time_diff * sysconf(_SC_CLK_TCK) / 1000.0);
                last_cpu_time = current_cpu_time;
                return std::min(100.0, std::max(0.0, cpu_percent));
            }
            
            last_cpu_time = current_cpu_time;
        }
    }
#endif
    
    return 0.0; // Unable to determine
}

// Global instance
static std::unique_ptr<PerformanceMonitor> g_performance_monitor;
static std::once_flag g_monitor_init_flag;

PerformanceMonitor& performance_monitor() {
    std::call_once(g_monitor_init_flag, []() {
        g_performance_monitor = std::make_unique<PerformanceMonitor>();
    });
    return *g_performance_monitor;
}

} // namespace crossexpand
