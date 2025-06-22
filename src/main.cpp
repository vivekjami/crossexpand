#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>

// Day 1: Core components
#include "core/input_manager.hpp"
#include "core/template_engine.hpp"
#include "core/text_injector.hpp"
#include "utils/logger.hpp"
#include "utils/config_manager.hpp"

using namespace crossexpand;

// Global flag for clean shutdown
std::atomic<bool> g_running{true};

void SignalHandler(int signal) {
    LOG_INFO("Received signal {}, shutting down...", signal);
    g_running = false;
}

// Simple CrossExpand Application (Day 1 compatible)
class CrossExpandApp {
private:
    std::unique_ptr<InputManager> input_manager_;
    std::unique_ptr<TextInjector> text_injector_;
    std::unique_ptr<TemplateEngine> template_engine_;
    std::unique_ptr<ConfigManager> config_manager_;
    
    std::string current_sequence_;
    std::mutex sequence_mutex_;

public:
    bool Initialize() {
        LOG_INFO("=== Initializing CrossExpand ===");
        
        // Initialize components
        input_manager_ = CreateInputManager();
        text_injector_ = CreateTextInjector();
        template_engine_ = std::make_unique<TemplateEngine>();
        config_manager_ = std::make_unique<ConfigManager>();
        
        if (!input_manager_->Initialize()) {
            LOG_ERROR("Failed to initialize input manager: {}", input_manager_->GetLastError());
            return false;
        }
        
        if (!text_injector_->Initialize()) {
            LOG_ERROR("Failed to initialize text injector: {}", text_injector_->GetLastError());
            return false;
        }
        
        // Load configuration
        if (!config_manager_->LoadConfig()) {
            LOG_WARNING("Failed to load configuration, using defaults");
        }
        
        // Load templates from configuration
        LoadTemplatesFromConfig();
        
        // Set up keyboard callback
        input_manager_->SetKeyboardCallback([this](const KeyEvent& event) {
            return HandleKeyEvent(event);
        });
        
        // Install system hook
        if (!input_manager_->InstallSystemHook()) {
            LOG_ERROR("Failed to install system hook: {}", input_manager_->GetLastError());
            return false;
        }
        
        LOG_INFO("CrossExpand initialized successfully");
        return true;
    }
    
    void Shutdown() {
        LOG_INFO("Shutting down CrossExpand...");
        
        if (input_manager_) {
            input_manager_->Shutdown();
        }
        
        if (text_injector_) {
            text_injector_->Shutdown();
        }
        
        // Save configuration
        if (config_manager_) {
            config_manager_->SaveConfig();
        }
        
        LOG_INFO("CrossExpand shutdown complete");
    }
    
    void Run() {
        LOG_INFO("CrossExpand is running. Press Ctrl+C to exit.");
        LOG_INFO("Try typing '/hello' in any application!");
        
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    void LoadTemplatesFromConfig() {
        const auto& templates = config_manager_->GetTemplates();
        for (const auto& [shortcut, tmpl] : templates) {
            template_engine_->AddTemplate(shortcut, tmpl);
        }
        
        const auto& variables = config_manager_->GetVariables();
        for (const auto& [name, value] : variables) {
            template_engine_->SetVariable(name, value);
        }
        
        LOG_INFO("Loaded {} templates and {} variables", 
                template_engine_->GetTemplateCount(), variables.size());
    }
    
    bool HandleKeyEvent(const KeyEvent& event) {
        std::lock_guard<std::mutex> lock(sequence_mutex_);
        
        // Build current sequence
        if (!event.character.empty()) {
            current_sequence_ += event.character;
            
            // Limit sequence length to prevent memory issues
            if (current_sequence_.length() > 100) {
                current_sequence_ = current_sequence_.substr(current_sequence_.length() - 50);
            }
            
            // Check for trigger patterns
            for (size_t i = 0; i < current_sequence_.length(); ++i) {
                std::string candidate = current_sequence_.substr(i);
                
                // Check if this looks like a template trigger
                if (candidate.length() >= 2 && candidate[0] == '/') {
                    // Look for space or specific ending
                    size_t space_pos = candidate.find(' ');
                    if (space_pos != std::string::npos) {
                        std::string trigger = candidate.substr(1, space_pos - 1);
                        ProcessTemplateTrigger(trigger);
                        
                        // Clear the sequence after processing
                        current_sequence_.clear();
                        break;
                    }
                }
            }
        }
        
        return true; // Continue processing
    }
    
    void ProcessTemplateTrigger(const std::string& trigger) {
        LOG_DEBUG("Processing template trigger: '{}'", trigger);
        
        // Try to expand the template
        std::string expanded = template_engine_->Expand(trigger);
        
        if (!expanded.empty() && expanded != trigger) {
            LOG_INFO("Expanding '{}' -> '{}'", trigger, expanded);
            
            // Calculate how many characters to backspace
            size_t backspace_count = trigger.length() + 2; // +2 for '/' and ' '
            
            // Perform text replacement
            PerformTextReplacement(backspace_count, expanded);
        } else {
            LOG_DEBUG("No template found for trigger: '{}'", trigger);
        }
    }
    
    void PerformTextReplacement(size_t backspace_count, const std::string& replacement) {
        if (!text_injector_) {
            LOG_ERROR("Text injector not available");
            return;
        }
        
        try {
            // First, backspace to remove the trigger
            for (size_t i = 0; i < backspace_count; ++i) {
                text_injector_->SendBackspace();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Then inject the replacement text
            text_injector_->InjectText(replacement);
            
            LOG_DEBUG("Text replacement completed successfully");
            
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to perform text replacement: {}", e.what());
        }
    }
};

int main(int argc, char* argv[]) {
    // Install signal handlers
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    // Initialize logging
    Logger::instance().set_level(LogLevel::INFO);
    
    LOG_INFO("=== CrossExpand Starting ===");
    LOG_INFO("Version: 2.0.0 (Day 2 Development Build)");
    LOG_INFO("Build date: {}", __DATE__);
    
    // Show usage if requested
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "CrossExpand - Advanced Text Expansion Engine\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --help, -h      Show this help message\n";
            std::cout << "\nCurrently running Day 1 implementation.\n";
            std::cout << "Day 2 advanced features are in development.\n";
            return 0;
        }
    }
    
    try {
        CrossExpandApp app;
        
        if (!app.Initialize()) {
            LOG_ERROR("Failed to initialize application");
            return 1;
        }
        
        app.Run();
        app.Shutdown();
        
        LOG_INFO("CrossExpand exited successfully");
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: {}", e.what());
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        LOG_ERROR("Unknown fatal error occurred");
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}

// Global flag for clean shutdown
std::atomic<bool> g_running{true};

void SignalHandler(int signal) {
    LOG_INFO("Received signal {}, shutting down...", signal);
    g_running = false;
}

// Day 2: Advanced CrossExpand Application with multithreading
class AdvancedCrossExpandApp {
private:
    std::unique_ptr<MultithreadedProcessor> processor_;
    std::unique_ptr<ConfigManager> config_manager_;
    
    // Performance monitoring
    std::thread stats_thread_;
    std::atomic<bool> should_print_stats_{false};

public:
    bool Initialize() {
        LOG_INFO("=== Initializing CrossExpand Day 2 (Advanced) ===");
        
        // Initialize performance monitoring
        auto& monitor = performance_monitor();
        monitor.enable();
        monitor.reset_all();
        
        // Initialize configuration
        config_manager_ = std::make_unique<ConfigManager>();
        if (!config_manager_->LoadConfig()) {
            LOG_WARNING("Failed to load configuration, using defaults");
        }
        
        // Initialize multithreaded processor
        processor_ = std::make_unique<MultithreadedProcessor>();
        if (!processor_->initialize()) {
            LOG_ERROR("Failed to initialize MultithreadedProcessor");
            return false;
        }
        
        // Start statistics thread
        start_stats_thread();
        
        // Log system information
        log_system_info();
        
        LOG_INFO("CrossExpand Day 2 initialized successfully");
        LOG_INFO("Advanced features: ✅ Multi-threading ✅ Memory pools ✅ Performance monitoring");
        return true;
    }
    
    void Shutdown() {
        LOG_INFO("Shutting down CrossExpand Day 2...");
        
        should_print_stats_.store(false);
        if (stats_thread_.joinable()) {
            stats_thread_.join();
        }
        
        if (processor_) {
            processor_->shutdown();
        }
        
        // Save configuration
        if (config_manager_) {
            config_manager_->SaveConfig();
        }
        
        // Print final performance report
        LOG_INFO("=== Final Performance Report ===");
        std::cout << performance_monitor().generate_report() << std::endl;
        
        LOG_INFO("CrossExpand Day 2 shutdown complete");
    }
    
    void Run() {
        LOG_INFO("🚀 CrossExpand Day 2 is running!");
        LOG_INFO("📊 Performance monitoring: ENABLED");
        LOG_INFO("🧵 Multi-threaded processing: ACTIVE");
        LOG_INFO("💾 Memory pools: OPTIMIZED");
        LOG_INFO("📝 Try typing '/hello' in any application!");
        LOG_INFO("📈 Press 's' + Enter to show statistics");
        
        should_print_stats_.store(true);
        
        // Main event loop
        while (g_running) {
            // Check for user commands
            handle_user_input();
            
            // Brief sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    void start_stats_thread() {
        stats_thread_ = std::thread([this]() {
            while (should_print_stats_.load()) {
                // Update system metrics
                performance_monitor().update_system_metrics();
                
                // Sleep for 5 seconds between updates
                for (int i = 0; i < 50 && should_print_stats_.load(); ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        });
    }
    
    void handle_user_input() {
        // Check if user typed 's' for statistics
        // This is a simplified version - in a real implementation,
        // this would be integrated with the input system
        
        // For demo purposes, we'll show stats periodically
        static auto last_stats = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats);
        
        if (elapsed.count() >= 30) { // Show stats every 30 seconds
            show_statistics();
            last_stats = now;
        }
    }
    
    void show_statistics() {
        auto stats = processor_->get_stats();
        
        LOG_INFO("=== CrossExpand Statistics ===");
        LOG_INFO("Uptime: {}ms", stats.uptime_ms);
        LOG_INFO("Active threads: {}", stats.active_threads);
        LOG_INFO("Queue size: {}", stats.queue_size);
        LOG_INFO("Events processed: {}", stats.events_processed);
        LOG_INFO("Templates expanded: {}", stats.templates_expanded);
        LOG_INFO("System healthy: {}", stats.is_healthy ? "YES" : "NO");
        
        // Show memory and CPU info
        auto& monitor = performance_monitor();
        LOG_INFO("Memory usage: {:.1f} MB", monitor.memory_usage_mb().get());
        LOG_INFO("CPU usage: {:.1f}%", monitor.cpu_usage_percent().get());
    }
    
    void log_system_info() {
        LOG_INFO("=== System Information ===");
        LOG_INFO("Hardware threads: {}", std::thread::hardware_concurrency());
        LOG_INFO("C++ standard: C++{}", __cplusplus / 100);
        
        auto& monitor = performance_monitor();
        LOG_INFO("Initial memory: {:.1f} MB", monitor.memory_usage_mb().get());
    }
};

private:
    void LoadTemplatesFromConfig() {
        const auto& templates = config_manager_->GetTemplates();
        for (const auto& [shortcut, tmpl] : templates) {
            template_engine_->AddTemplate(shortcut, tmpl);
        }
        
        const auto& variables = config_manager_->GetVariables();
        for (const auto& [name, value] : variables) {
            template_engine_->SetVariable(name, value);
        }
        
        LOG_INFO("Loaded {} templates and {} variables", 
                template_engine_->GetTemplateCount(), variables.size());
    }
    
    bool HandleKeyEvent(const KeyEvent& event) {
        std::lock_guard<std::mutex> lock(sequence_mutex_);
        
        // Build current sequence
        if (!event.character.empty()) {
            current_sequence_ += event.character;
            
            // Limit sequence length to prevent memory issues
            if (current_sequence_.length() > 100) {
                current_sequence_ = current_sequence_.substr(current_sequence_.length() - 50);
            }
            
            // Check for template triggers
            return CheckForTemplateExpansion();
        }
        
        return false; // Don't suppress the key event
    }
    
    bool CheckForTemplateExpansion() {
        // Look for shortcuts starting with '/'
        size_t last_slash = current_sequence_.rfind('/');
        if (last_slash == std::string::npos) {
            return false;
        }
        
        std::string potential_shortcut = current_sequence_.substr(last_slash);
        
        // Check if this is a complete template (ends with space or punctuation)
        if (current_sequence_.back() == ' ' || 
            current_sequence_.back() == '\n' ||
            current_sequence_.back() == '\t') {
            
            // Remove the trailing character for template lookup
            std::string shortcut = potential_shortcut.substr(0, potential_shortcut.length() - 1);
            
            if (template_engine_->HasTemplate(shortcut)) {
                return ExpandTemplate(shortcut);
            }
        }
        
        return false;
    }
    
    bool ExpandTemplate(const std::string& shortcut) {
        LOG_INFO("Expanding template: {}", shortcut);
        
        // Get expanded text
        std::string expanded_text = template_engine_->Expand(shortcut);
        if (expanded_text.empty()) {
            LOG_WARNING("Template expansion returned empty text for: {}", shortcut);
            return false;
        }
        
        // Delete the shortcut text (including the trailing space/newline)
        size_t chars_to_delete = shortcut.length() + 1; // +1 for the trigger character
        if (!text_injector_->DeletePreviousChars(chars_to_delete)) {
            LOG_ERROR("Failed to delete shortcut text: {}", text_injector_->GetLastError());
            return false;
        }
        
        // Small delay before injection
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Inject the expanded text
        if (!text_injector_->InjectText(expanded_text)) {
            LOG_ERROR("Failed to inject text: {}", text_injector_->GetLastError());
            return false;
        }
        
        // Clear the sequence after successful expansion
        current_sequence_.clear();
        
        LOG_INFO("Successfully expanded '{}' to '{}' ({} chars)", 
                shortcut, expanded_text.substr(0, 50) + (expanded_text.length() > 50 ? "..." : ""), 
                expanded_text.length());
        
        return true; // Suppress the original key event
    }
};

int main() {
    // Set up signal handlers for clean shutdown
    signal(SIGINT, SignalHandler);  // Ctrl+C
    signal(SIGTERM, SignalHandler); // Termination request
    
    // Initialize logging
    Logger::Instance().SetLevel(LogLevel::INFO);
    
    try {
        CrossExpandApp app;
        
        if (!app.Initialize()) {
            LOG_FATAL("Failed to initialize CrossExpand");
            return 1;
        }
        
        app.Run();
        app.Shutdown();
        
    } catch (const std::exception& e) {
        LOG_FATAL("Unhandled exception: {}", e.what());
        return 1;
    }
    
    return 0;
}
