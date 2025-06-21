#include "core/input_manager.hpp"
#include "utils/logger.hpp"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <thread>
#include <atomic>
#include <cstring>

namespace crossexpand {

class X11InputManager : public InputManager {
private:
    Display* display_;
    std::thread polling_thread_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> hook_active_;
    
    KeyboardCallback keyboard_callback_;
    std::mutex callback_mutex_;
    std::string last_error_;
    
    // For simple key state tracking
    char prev_keys_[32];
    char curr_keys_[32];

public:
    X11InputManager() 
        : display_(nullptr), should_stop_(false), hook_active_(false) {
        memset(prev_keys_, 0, sizeof(prev_keys_));
        memset(curr_keys_, 0, sizeof(curr_keys_));
        LOG_DEBUG("X11InputManager created");
    }
    
    ~X11InputManager() {
        Shutdown();
    }
    
    bool Initialize() override {
        // Open main display connection
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            last_error_ = "Failed to open X11 display";
            LOG_ERROR("{}", last_error_);
            return false;
        }
        
        LOG_INFO("X11InputManager initialized (using keyboard polling)");
        return true;
    }
    
    void Shutdown() override {
        if (IsHookActive()) {
            UninstallSystemHook();
        }
        
        if (display_) {
            XCloseDisplay(display_);
            display_ = nullptr;
        }
        
        LOG_INFO("X11InputManager shutdown");
    }
    
    bool InstallSystemHook() override {
        if (hook_active_) {
            return true;
        }
        
        // Start polling thread
        should_stop_ = false;
        polling_thread_ = std::thread([this]() {
            PollingThreadMain();
        });
        
        hook_active_ = true;
        LOG_INFO("X11 keyboard polling started");
        return true;
    }
    
    void UninstallSystemHook() override {
        if (!hook_active_) {
            return;
        }
        
        should_stop_ = true;
        
        // Wait for thread to finish
        if (polling_thread_.joinable()) {
            polling_thread_.join();
        }
        
        hook_active_ = false;
        LOG_INFO("X11 keyboard polling stopped");
    }
    
    void SetKeyboardCallback(KeyboardCallback callback) override {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        keyboard_callback_ = callback;
    }
    
    bool IsHookActive() const override {
        return hook_active_;
    }
    
    std::string GetLastError() const override {
        return last_error_;
    }

private:
    void PollingThreadMain() {
        LOG_DEBUG("Polling thread started");
        LOG_WARNING("Using keyboard polling - this is a demo implementation");
        LOG_WARNING("For production, proper system hooks would be implemented");
        
        while (!should_stop_) {
            // Poll keyboard state
            XQueryKeymap(display_, curr_keys_);
            
            // Check for key changes
            for (int i = 0; i < 32; ++i) {
                char changed = curr_keys_[i] ^ prev_keys_[i];
                if (changed) {
                    for (int j = 0; j < 8; ++j) {
                        if (changed & (1 << j)) {
                            int keycode = i * 8 + j;
                            bool pressed = curr_keys_[i] & (1 << j);
                            
                            if (pressed) {
                                ProcessKeyPress(keycode);
                            }
                        }
                    }
                }
            }
            
            // Copy current to previous
            memcpy(prev_keys_, curr_keys_, sizeof(prev_keys_));
            
            // Sleep to avoid excessive CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        LOG_DEBUG("Polling thread finished");
    }
    
    void ProcessKeyPress(int keycode) {
        KeyEvent key_event;
        key_event.keycode = keycode;
        key_event.modifiers = 0; // Simplified for demo
        key_event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        key_event.is_repeat = false;
        
        // Convert keycode to character
        KeySym keysym = XkbKeycodeToKeysym(display_, keycode, 0, 0);
        if (keysym != NoSymbol) {
            if (keysym >= 0x20 && keysym <= 0x7E) {
                // Printable ASCII character
                key_event.character = static_cast<char>(keysym);
            } else if (keysym == XK_space) {
                key_event.character = " ";
            } else if (keysym == XK_Return) {
                key_event.character = "\n";
            } else if (keysym == XK_Tab) {
                key_event.character = "\t";
            }
        }
        
        // Only process if we have a character
        if (!key_event.character.empty()) {
            // Call callback
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (keyboard_callback_) {
                keyboard_callback_(key_event);
            }
        }
    }
};

std::unique_ptr<InputManager> CreateInputManager() {
    return std::make_unique<X11InputManager>();
}

} // namespace crossexpand
