#include "core/enhanced_text_injector.hpp"
#include "utils/logger.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <random>
#include <algorithm>

namespace crossexpand {

// EnhancedTextInjector Implementation
EnhancedTextInjector::EnhancedTextInjector() {
    load_default_profiles();
    LOG_DEBUG("EnhancedTextInjector created with {} app profiles", app_profiles_.size());
}

bool EnhancedTextInjector::Initialize() {
    // Initialize all strategy implementations
    fast_injector_ = std::make_unique<FastTypingInjector>();
    natural_injector_ = std::make_unique<NaturalTypingInjector>();
    clipboard_injector_ = std::make_unique<ClipboardInjector>();
    
    bool success = true;
    success &= fast_injector_->Initialize();
    success &= natural_injector_->Initialize();
    success &= clipboard_injector_->Initialize();
    
    if (success) {
        LOG_INFO("EnhancedTextInjector initialized with all strategies");
    } else {
        LOG_ERROR("Failed to initialize one or more injection strategies");
    }
    
    return success;
}

void EnhancedTextInjector::Shutdown() {
    if (fast_injector_) fast_injector_->Shutdown();
    if (natural_injector_) natural_injector_->Shutdown();
    if (clipboard_injector_) clipboard_injector_->Shutdown();
    
    LOG_INFO("EnhancedTextInjector shutdown complete");
}

bool EnhancedTextInjector::InjectText(const std::string& text, InjectionMethod method) {
    // Convert old method to new strategy
    InjectionStrategy strategy = InjectionStrategy::ADAPTIVE;
    switch (method) {
        case InjectionMethod::SIMULATE_TYPING:
            strategy = InjectionStrategy::FAST_TYPING;
            break;
        case InjectionMethod::CLIPBOARD_PASTE:
            strategy = InjectionStrategy::CLIPBOARD_PASTE;
            break;
        default:
            strategy = InjectionStrategy::ADAPTIVE;
            break;
    }
    
    return inject_text_enhanced(text, strategy);
}

bool EnhancedTextInjector::inject_text_enhanced(const std::string& text, InjectionStrategy strategy) {
    auto start_time = std::chrono::steady_clock::now();
    
    InjectionMetrics metrics;
    metrics.characters_injected = text.length();
    metrics.strategy_used = strategy;
    
    // Choose strategy if adaptive
    if (strategy == InjectionStrategy::ADAPTIVE) {
        strategy = choose_optimal_strategy(text);
        metrics.strategy_used = strategy;
    }
    
    bool success = false;
    
    try {
        switch (strategy) {
            case InjectionStrategy::FAST_TYPING:
                success = inject_fast_typing(text);
                break;
                
            case InjectionStrategy::NATURAL_TYPING:
                success = inject_natural_typing(text);
                break;
                
            case InjectionStrategy::CLIPBOARD_PASTE:
                success = inject_clipboard_paste(text);
                break;
                
            default:
                success = inject_fast_typing(text); // Fallback
                break;
        }
    } catch (const std::exception& e) {
        metrics.error_message = e.what();
        success = false;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    metrics.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    metrics.success = success;
    
    record_metrics(metrics);
    
    total_injections_.fetch_add(1);
    if (success) {
        successful_injections_.fetch_add(1);
    }
    
    return success;
}

InjectionStrategy EnhancedTextInjector::choose_optimal_strategy(const std::string& text) const {
    // Simple heuristics for strategy selection
    if (text.length() > 500) {
        return InjectionStrategy::CLIPBOARD_PASTE;
    }
    
    if (text.length() > 100) {
        return InjectionStrategy::FAST_TYPING;
    }
    
    return InjectionStrategy::NATURAL_TYPING;
}

bool EnhancedTextInjector::inject_fast_typing(const std::string& text) {
    return fast_injector_ && fast_injector_->InjectText(text);
}

bool EnhancedTextInjector::inject_natural_typing(const std::string& text) {
    return natural_injector_ && natural_injector_->InjectText(text);
}

bool EnhancedTextInjector::inject_clipboard_paste(const std::string& text) {
    return clipboard_injector_ && clipboard_injector_->InjectText(text);
}

void EnhancedTextInjector::record_metrics(const InjectionMetrics& metrics) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    recent_metrics_.push_back(metrics);
    
    // Keep only recent metrics
    if (recent_metrics_.size() > 100) {
        recent_metrics_.erase(recent_metrics_.begin());
    }
}

void EnhancedTextInjector::load_default_profiles() {
    // Default profile for most applications
    AppProfile default_profile;
    default_profile.app_name = "default";
    default_profile.preferred_strategy = InjectionStrategy::ADAPTIVE;
    app_profiles_["default"] = default_profile;
    
    // Terminal applications
    AppProfile terminal_profile;
    terminal_profile.app_name = "terminal";
    terminal_profile.preferred_strategy = InjectionStrategy::NATURAL_TYPING;
    terminal_profile.typing_delay_ms = 20;
    app_profiles_["gnome-terminal"] = terminal_profile;
    app_profiles_["xterm"] = terminal_profile;
    app_profiles_["konsole"] = terminal_profile;
}

bool EnhancedTextInjector::DeletePreviousChars(size_t count) {
    return fast_injector_ && fast_injector_->DeletePreviousChars(count);
}

bool EnhancedTextInjector::IsReady() const {
    return fast_injector_ && fast_injector_->IsReady();
}

std::string EnhancedTextInjector::GetLastError() const {
    if (fast_injector_) {
        return fast_injector_->GetLastError();
    }
    return "EnhancedTextInjector not initialized";
}

// FastTypingInjector Implementation
FastTypingInjector::FastTypingInjector() : display_(nullptr) {
    LOG_DEBUG("FastTypingInjector created");
}

bool FastTypingInjector::Initialize() {
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        last_error_ = "Failed to open X11 display";
        return false;
    }
    
    LOG_DEBUG("FastTypingInjector initialized");
    return true;
}

void FastTypingInjector::Shutdown() {
    if (display_) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

bool FastTypingInjector::InjectText(const std::string& text, InjectionMethod) {
    if (!display_) return false;
    
    for (char c : text) {
        if (!type_character_fast(c)) {
            return false;
        }
        std::this_thread::sleep_for(base_delay_);
    }
    
    XFlush(display_);
    return true;
}

bool FastTypingInjector::type_character_fast(char c) {
    // Simplified character typing - would be expanded in full implementation
    KeySym keysym = 0;
    
    if (c >= 'a' && c <= 'z') {
        keysym = XK_a + (c - 'a');
    } else if (c == ' ') {
        keysym = XK_space;
    } else if (c == '\n') {
        keysym = XK_Return;
    }
    
    if (keysym == 0) return true; // Skip unsupported
    
    KeyCode keycode = XKeysymToKeycode(display_, keysym);
    if (keycode == 0) return false;
    
    XTestFakeKeyEvent(display_, keycode, True, CurrentTime);
    XTestFakeKeyEvent(display_, keycode, False, CurrentTime);
    
    return true;
}

bool FastTypingInjector::DeletePreviousChars(size_t count) {
    if (!display_) return false;
    
    KeyCode backspace = XKeysymToKeycode(display_, XK_BackSpace);
    for (size_t i = 0; i < count; ++i) {
        XTestFakeKeyEvent(display_, backspace, True, CurrentTime);
        XTestFakeKeyEvent(display_, backspace, False, CurrentTime);
        std::this_thread::sleep_for(base_delay_);
    }
    
    XFlush(display_);
    return true;
}

bool FastTypingInjector::IsReady() const {
    return display_ != nullptr;
}

std::string FastTypingInjector::GetLastError() const {
    return last_error_;
}

// NaturalTypingInjector Implementation
NaturalTypingInjector::NaturalTypingInjector() 
    : display_(nullptr), gen_(rd_()) {
    LOG_DEBUG("NaturalTypingInjector created");
}

bool NaturalTypingInjector::Initialize() {
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        last_error_ = "Failed to open X11 display";
        return false;
    }
    
    LOG_DEBUG("NaturalTypingInjector initialized");
    return true;
}

void NaturalTypingInjector::Shutdown() {
    if (display_) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

bool NaturalTypingInjector::InjectText(const std::string& text, InjectionMethod) {
    if (!display_) return false;
    
    for (char c : text) {
        if (!type_character_natural(c)) {
            return false;
        }
        
        auto delay = calculate_delay();
        std::this_thread::sleep_for(delay);
        
        maybe_pause();
    }
    
    XFlush(display_);
    return true;
}

std::chrono::milliseconds NaturalTypingInjector::calculate_delay() {
    std::uniform_int_distribution<> dist(-variance_.count(), variance_.count());
    auto variation = std::chrono::milliseconds(dist(gen_));
    return base_delay_ + variation;
}

void NaturalTypingInjector::maybe_pause() {
    std::uniform_real_distribution<> dist(0.0, 1.0);
    if (dist(gen_) < pause_probability_) {
        int pause_ms = 100 + static_cast<int>(dist(gen_) * 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(pause_ms));
    }
}

bool NaturalTypingInjector::type_character_natural(char c) {
    // Same as fast typing but with more natural rhythm
    return true; // Simplified for now
}

bool NaturalTypingInjector::DeletePreviousChars(size_t count) {
    // Similar to fast injector but with natural delays
    return true; // Simplified
}

bool NaturalTypingInjector::IsReady() const {
    return display_ != nullptr;
}

std::string NaturalTypingInjector::GetLastError() const {
    return last_error_;
}

// ClipboardInjector Implementation  
ClipboardInjector::ClipboardInjector() : display_(nullptr) {
    LOG_DEBUG("ClipboardInjector created");
}

bool ClipboardInjector::Initialize() {
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        last_error_ = "Failed to open X11 display";
        return false;
    }
    
    LOG_DEBUG("ClipboardInjector initialized");
    return true;
}

void ClipboardInjector::Shutdown() {
    if (display_) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

bool ClipboardInjector::InjectText(const std::string& text, InjectionMethod) {
    if (!display_) return false;
    
    // Save original clipboard
    original_clipboard_ = get_clipboard_content();
    
    // Set new content
    if (!set_clipboard_content(text)) {
        return false;
    }
    
    // Send paste command (Ctrl+V)
    if (!send_paste_command()) {
        restore_clipboard();
        return false;
    }
    
    // Restore original clipboard after a delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    restore_clipboard();
    
    return true;
}

bool ClipboardInjector::set_clipboard_content(const std::string& text) {
    // Simplified clipboard setting - would use proper X11 clipboard API
    LOG_WARNING("Clipboard injection not fully implemented");
    return false; // Not implemented for demo
}

std::string ClipboardInjector::get_clipboard_content() const {
    return ""; // Not implemented for demo
}

bool ClipboardInjector::send_paste_command() {
    if (!display_) return false;
    
    // Send Ctrl+V
    KeyCode ctrl = XKeysymToKeycode(display_, XK_Control_L);
    KeyCode v = XKeysymToKeycode(display_, XK_v);
    
    XTestFakeKeyEvent(display_, ctrl, True, CurrentTime);
    XTestFakeKeyEvent(display_, v, True, CurrentTime);
    XTestFakeKeyEvent(display_, v, False, CurrentTime);
    XTestFakeKeyEvent(display_, ctrl, False, CurrentTime);
    
    XFlush(display_);
    return true;
}

bool ClipboardInjector::DeletePreviousChars(size_t count) {
    return false; // Not applicable for clipboard injection
}

bool ClipboardInjector::IsReady() const {
    return display_ != nullptr;
}

std::string ClipboardInjector::GetLastError() const {
    return last_error_;
}

bool ClipboardInjector::restore_clipboard() {
    // Restore the original clipboard content
    // For now, just return true as a placeholder
    return true;
}

} // namespace crossexpand
