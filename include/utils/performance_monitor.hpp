#pragma once

#include <chrono>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <limits>

namespace crossexpand {

// Performance metric types
enum class MetricType {
    COUNTER,        // Simple incrementing counter
    GAUGE,          // Current value (can go up/down)
    HISTOGRAM,      // Distribution of values
    TIMER           // Duration measurements
};

// Base metric class
class Metric {
protected:
    std::string name_;
    MetricType type_;
    std::chrono::steady_clock::time_point created_at_;

public:
    Metric(const std::string& name, MetricType type)
        : name_(name), type_(type), created_at_(std::chrono::steady_clock::now()) {}
    
    virtual ~Metric() = default;
    
    const std::string& name() const { return name_; }
    MetricType type() const { return type_; }
    
    virtual std::string to_string() const = 0;
    virtual void reset() = 0;
};

// Counter metric - monotonically increasing
class CounterMetric : public Metric {
private:
    std::atomic<uint64_t> value_{0};

public:
    CounterMetric(const std::string& name) : Metric(name, MetricType::COUNTER) {}
    
    void increment(uint64_t delta = 1) {
        value_.fetch_add(delta);
    }
    
    uint64_t get() const {
        return value_.load();
    }
    
    void reset() override {
        value_.store(0);
    }
    
    std::string to_string() const override {
        return name_ + ": " + std::to_string(get());
    }
};

// Gauge metric - current value
class GaugeMetric : public Metric {
private:
    std::atomic<double> value_{0.0};

public:
    GaugeMetric(const std::string& name) : Metric(name, MetricType::GAUGE) {}
    
    void set(double value) {
        value_.store(value);
    }
    
    void add(double delta) {
        // Atomic add for double
        double current = value_.load();
        while (!value_.compare_exchange_weak(current, current + delta)) {
            // Retry on failure
        }
    }
    
    double get() const {
        return value_.load();
    }
    
    void reset() override {
        value_.store(0.0);
    }
    
    std::string to_string() const override {
        return name_ + ": " + std::to_string(get());
    }
};

// Histogram metric - tracks distribution of values
class HistogramMetric : public Metric {
private:
    std::mutex mutex_;
    std::vector<double> values_;
    std::atomic<uint64_t> count_{0};
    std::atomic<double> sum_{0.0};
    std::atomic<double> min_{std::numeric_limits<double>::max()};
    std::atomic<double> max_{std::numeric_limits<double>::lowest()};

    void update_bounds(double value) {
        // Update min
        double current_min = min_.load();
        while (value < current_min && 
               !min_.compare_exchange_weak(current_min, value)) {
            // Retry
        }
        
        // Update max
        double current_max = max_.load();
        while (value > current_max && 
               !max_.compare_exchange_weak(current_max, value)) {
            // Retry
        }
    }

public:
    HistogramMetric(const std::string& name) : Metric(name, MetricType::HISTOGRAM) {}
    
    void observe(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.push_back(value);
        count_.fetch_add(1);
        
        // Atomic add for sum
        double current_sum = sum_.load();
        while (!sum_.compare_exchange_weak(current_sum, current_sum + value)) {
            // Retry
        }
        
        update_bounds(value);
    }
    
    uint64_t count() const { return count_.load(); }
    double sum() const { return sum_.load(); }
    double min() const { 
        double val = min_.load();
        return val == std::numeric_limits<double>::max() ? 0.0 : val;
    }
    double max() const { 
        double val = max_.load();
        return val == std::numeric_limits<double>::lowest() ? 0.0 : val;
    }
    
    double mean() const {
        uint64_t c = count();
        return c > 0 ? sum() / c : 0.0;
    }
    
    double percentile(double p) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        if (values_.empty()) return 0.0;
        
        auto sorted = values_;
        std::sort(sorted.begin(), sorted.end());
        
        size_t index = static_cast<size_t>(p * (sorted.size() - 1));
        return sorted[index];
    }
    
    void reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.clear();
        count_.store(0);
        sum_.store(0.0);
        min_.store(std::numeric_limits<double>::max());
        max_.store(std::numeric_limits<double>::lowest());
    }
    
    std::string to_string() const override {
        return name_ + ": count=" + std::to_string(count()) + 
               ", mean=" + std::to_string(mean()) +
               ", min=" + std::to_string(min()) +
               ", max=" + std::to_string(max());
    }
};

// Timer metric - specialized histogram for duration measurements
class TimerMetric : public Metric {
private:
    HistogramMetric histogram_;

public:
    TimerMetric(const std::string& name) 
        : Metric(name, MetricType::TIMER), histogram_(name + "_duration") {}
    
    void record(std::chrono::microseconds duration) {
        histogram_.observe(static_cast<double>(duration.count()));
    }
    
    void record(std::chrono::milliseconds duration) {
        record(std::chrono::duration_cast<std::chrono::microseconds>(duration));
    }
    
    // RAII timer helper
    class Timer {
    private:
        TimerMetric& metric_;
        std::chrono::high_resolution_clock::time_point start_;
        
    public:
        Timer(TimerMetric& metric) 
            : metric_(metric), start_(std::chrono::high_resolution_clock::now()) {}
        
        ~Timer() {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start_);
            metric_.record(duration);
        }
    };
    
    Timer time() {
        return Timer(*this);
    }
    
    uint64_t count() const { return histogram_.count(); }
    double mean_microseconds() const { return histogram_.mean(); }
    double p95_microseconds() const { return histogram_.percentile(0.95); }
    double p99_microseconds() const { return histogram_.percentile(0.99); }
    
    void reset() override {
        histogram_.reset();
    }
    
    std::string to_string() const override {
        return name_ + ": count=" + std::to_string(count()) + 
               ", mean=" + std::to_string(mean_microseconds()) + "μs" +
               ", p95=" + std::to_string(p95_microseconds()) + "μs" +
               ", p99=" + std::to_string(p99_microseconds()) + "μs";
    }
};

// Performance monitor - manages all metrics
class PerformanceMonitor {
private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<Metric>> metrics_;
    std::chrono::steady_clock::time_point start_time_;
    std::atomic<bool> enabled_{true};

    // Predefined system metrics
    std::unique_ptr<CounterMetric> events_processed_;
    std::unique_ptr<CounterMetric> templates_expanded_;
    std::unique_ptr<CounterMetric> text_injections_;
    std::unique_ptr<CounterMetric> errors_occurred_;
    
    std::unique_ptr<GaugeMetric> memory_usage_mb_;
    std::unique_ptr<GaugeMetric> cpu_usage_percent_;
    std::unique_ptr<GaugeMetric> queue_size_;
    std::unique_ptr<GaugeMetric> active_threads_;
    
    std::unique_ptr<TimerMetric> template_expansion_time_;
    std::unique_ptr<TimerMetric> text_injection_time_;
    std::unique_ptr<TimerMetric> event_processing_time_;

public:
    PerformanceMonitor();
    ~PerformanceMonitor() = default;
    
    // Enable/disable monitoring
    void enable() { enabled_.store(true); }
    void disable() { enabled_.store(false); }
    bool is_enabled() const { return enabled_.load(); }
    
    // System metrics access
    CounterMetric& events_processed() { return *events_processed_; }
    CounterMetric& templates_expanded() { return *templates_expanded_; }
    CounterMetric& text_injections() { return *text_injections_; }
    CounterMetric& errors_occurred() { return *errors_occurred_; }
    
    GaugeMetric& memory_usage_mb() { return *memory_usage_mb_; }
    GaugeMetric& cpu_usage_percent() { return *cpu_usage_percent_; }
    GaugeMetric& queue_size() { return *queue_size_; }
    GaugeMetric& active_threads() { return *active_threads_; }
    
    TimerMetric& template_expansion_time() { return *template_expansion_time_; }
    TimerMetric& text_injection_time() { return *text_injection_time_; }
    TimerMetric& event_processing_time() { return *event_processing_time_; }
    
    // Custom metrics
    CounterMetric& counter(const std::string& name);
    GaugeMetric& gauge(const std::string& name);
    HistogramMetric& histogram(const std::string& name);
    TimerMetric& timer(const std::string& name);
    
    // Reporting
    std::vector<std::string> get_all_metrics() const;
    std::string generate_report() const;
    void reset_all();
    
    // System info
    std::chrono::milliseconds uptime() const;
    void update_system_metrics();
    
private:
    template<typename T>
    T& get_metric(const std::string& name) {
        if (!enabled_.load()) {
            static T dummy(name);
            return dummy;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = metrics_.find(name);
        if (it == metrics_.end()) {
            auto metric = std::make_unique<T>(name);
            T& ref = *metric;
            metrics_[name] = std::move(metric);
            return ref;
        }
        return static_cast<T&>(*it->second);
    }
    
    double get_memory_usage_mb() const;
    double get_cpu_usage_percent() const;
};

// Global performance monitor instance
PerformanceMonitor& performance_monitor();

// Convenience macros for timing
#define PERF_TIMER(name) auto _timer = performance_monitor().timer(name).time()
#define PERF_COUNTER_INC(name) performance_monitor().counter(name).increment()
#define PERF_GAUGE_SET(name, value) performance_monitor().gauge(name).set(value)

} // namespace crossexpand
