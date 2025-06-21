#pragma once

#include <functional>
#include <string>
#include <memory>
#include <cstdint>

namespace crossexpand {

struct KeyEvent {
    uint32_t keycode;
    uint32_t modifiers;
    uint64_t timestamp;
    std::string character;
    bool is_repeat = false;
};

using KeyboardCallback = std::function<bool(const KeyEvent&)>;

class InputManager {
public:
    virtual ~InputManager() = default;
    
    // Lifecycle management
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    
    // Hook management
    virtual bool InstallSystemHook() = 0;
    virtual void UninstallSystemHook() = 0;
    
    // Event handling
    virtual void SetKeyboardCallback(KeyboardCallback callback) = 0;
    
    // State management
    virtual bool IsHookActive() const = 0;
    virtual std::string GetLastError() const = 0;
};

// Factory function
std::unique_ptr<InputManager> CreateInputManager();

} // namespace crossexpand
