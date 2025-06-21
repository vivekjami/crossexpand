#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>

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
        LOG_INFO("Initializing CrossExpand...");
        
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
