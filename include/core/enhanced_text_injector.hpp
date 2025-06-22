#pragma once

#include "core/text_injector.hpp"
#include <unordered_map>
#include <memory>
#include <chrono>
#include <atomic>
#include <random>
#include <mutex>
#include <thread>
#include <vector>
#include <optional>

// X11 includes for Linux platform
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#endif

namespace crossexpand {

// Enhanced injection strategies
enum class InjectionStrategy {
    FAST_TYPING,      // Optimized typing simulation
    NATURAL_TYPING,   // Human-like typing with delays
    CLIPBOARD_PASTE,  // Copy-paste method
    DIRECT_WRITE,     // Direct memory/API writing
    ADAPTIVE          // Choose best method automatically
};

// Unicode support levels
enum class UnicodeSupport {
    ASCII_ONLY,
    LATIN_EXTENDED,
    FULL_UNICODE
};

// Application-specific optimization
struct AppProfile {
    std::string app_name;
    std::string window_class;
    InjectionStrategy preferred_strategy;
    UnicodeSupport unicode_level;
    int typing_delay_ms;
    bool supports_clipboard;
    bool requires_focus;
    
    AppProfile() 
        : preferred_strategy(InjectionStrategy::ADAPTIVE)
        , unicode_level(UnicodeSupport::FULL_UNICODE)
        , typing_delay_ms(10)
        , supports_clipboard(true)
        , requires_focus(true) {}
};

// Injection performance metrics
struct InjectionMetrics {
    std::chrono::milliseconds total_time;
    std::chrono::milliseconds preparation_time;
    std::chrono::milliseconds execution_time;
    size_t characters_injected;
    size_t special_characters;
    InjectionStrategy strategy_used;
    bool success;
    std::string error_message;
};

// Enhanced text injector with multiple strategies
class EnhancedTextInjector : public TextInjector {
private:
    std::unordered_map<std::string, AppProfile> app_profiles_;
    std::atomic<uint64_t> total_injections_{0};
    std::atomic<uint64_t> successful_injections_{0};
    std::vector<InjectionMetrics> recent_metrics_;
    mutable std::mutex metrics_mutex_;
    
    // Strategy implementations
    std::unique_ptr<TextInjector> fast_injector_;
    std::unique_ptr<TextInjector> natural_injector_;
    std::unique_ptr<TextInjector> clipboard_injector_;

public:
    EnhancedTextInjector();
    ~EnhancedTextInjector() = default;
    
    // TextInjector interface
    bool Initialize() override;
    void Shutdown() override;
    bool InjectText(const std::string& text, InjectionMethod method = InjectionMethod::SIMULATE_TYPING) override;
    bool DeletePreviousChars(size_t count) override;
    bool IsReady() const override;
    std::string GetLastError() const override;
    
    // Enhanced interface
    bool inject_text_enhanced(const std::string& text, 
                             InjectionStrategy strategy = InjectionStrategy::ADAPTIVE);
    
    bool inject_with_profile(const std::string& text, const AppProfile& profile);
    
    // Application profiling
    void add_app_profile(const AppProfile& profile);
    AppProfile get_app_profile(const std::string& app_name) const;
    void auto_detect_application();
    
    // Unicode support
    bool supports_unicode_level(UnicodeSupport level) const;
    std::string normalize_unicode(const std::string& text, UnicodeSupport target_level) const;
    
    // Performance monitoring
    InjectionMetrics get_last_metrics() const;
    std::vector<InjectionMetrics> get_recent_metrics(size_t count = 10) const;
    
    struct PerformanceStats {
        uint64_t total_injections;
        uint64_t successful_injections;
        double success_rate;
        std::chrono::milliseconds average_injection_time;
        std::chrono::milliseconds fastest_injection;
        std::chrono::milliseconds slowest_injection;
        size_t total_characters;
        double characters_per_second;
    };
    
    PerformanceStats get_performance_stats() const;
    void reset_metrics();

private:
    InjectionStrategy choose_optimal_strategy(const std::string& text) const;
    bool inject_fast_typing(const std::string& text);
    bool inject_natural_typing(const std::string& text);
    bool inject_clipboard_paste(const std::string& text);
    
    void record_metrics(const InjectionMetrics& metrics);
    bool validate_text(const std::string& text) const;
    
    // Application detection
    std::string get_active_window_class() const;
    std::string get_active_application_name() const;
    
    // Default profiles
    void load_default_profiles();
};

// Fast typing injector optimized for speed
class FastTypingInjector : public TextInjector {
private:
    Display* display_;
    std::string last_error_;
    std::chrono::milliseconds base_delay_{5};

public:
    FastTypingInjector();
    ~FastTypingInjector() = default;
    
    bool Initialize() override;
    void Shutdown() override;
    bool InjectText(const std::string& text, InjectionMethod method = InjectionMethod::SIMULATE_TYPING) override;
    bool DeletePreviousChars(size_t count) override;
    bool IsReady() const override;
    std::string GetLastError() const override;
    
    void set_typing_delay(std::chrono::milliseconds delay) { base_delay_ = delay; }

private:
    bool type_character_fast(char c);
    bool handle_special_sequence(const std::string& sequence);
};

// Natural typing injector with human-like patterns
class NaturalTypingInjector : public TextInjector {
private:
    Display* display_;
    std::string last_error_;
    std::random_device rd_;
    std::mt19937 gen_;
    
    // Typing rhythm parameters
    std::chrono::milliseconds base_delay_{25};
    std::chrono::milliseconds variance_{10};
    double pause_probability_{0.05};

public:
    NaturalTypingInjector();
    ~NaturalTypingInjector() = default;
    
    bool Initialize() override;
    void Shutdown() override;
    bool InjectText(const std::string& text, InjectionMethod method = InjectionMethod::SIMULATE_TYPING) override;
    bool DeletePreviousChars(size_t count) override;
    bool IsReady() const override;
    std::string GetLastError() const override;
    
    void set_typing_rhythm(std::chrono::milliseconds base, 
                          std::chrono::milliseconds variance,
                          double pause_prob);

private:
    bool type_character_natural(char c);
    std::chrono::milliseconds calculate_delay();
    void maybe_pause();
};

// Clipboard-based injector for large text
class ClipboardInjector : public TextInjector {
private:
    Display* display_;
    std::string last_error_;
    std::string original_clipboard_;

public:
    ClipboardInjector();
    ~ClipboardInjector() = default;
    
    bool Initialize() override;
    void Shutdown() override;
    bool InjectText(const std::string& text, InjectionMethod method = InjectionMethod::CLIPBOARD_PASTE) override;
    bool DeletePreviousChars(size_t count) override;
    bool IsReady() const override;
    std::string GetLastError() const override;

private:
    bool set_clipboard_content(const std::string& text);
    std::string get_clipboard_content() const;
    bool restore_clipboard();
    bool send_paste_command();
};

} // namespace crossexpand
