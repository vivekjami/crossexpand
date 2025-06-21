#pragma once

#include <string>
#include <memory>

namespace crossexpand {

enum class InjectionMethod {
    SIMULATE_TYPING,
    CLIPBOARD_PASTE,
    DIRECT_INSERT
};

class TextInjector {
public:
    virtual ~TextInjector() = default;
    
    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    
    // Text injection
    virtual bool InjectText(const std::string& text, 
                           InjectionMethod method = InjectionMethod::SIMULATE_TYPING) = 0;
    
    // Backspace support
    virtual bool DeletePreviousChars(size_t count) = 0;
    
    // State management
    virtual bool IsReady() const = 0;
    virtual std::string GetLastError() const = 0;
};

// Factory function
std::unique_ptr<TextInjector> CreateTextInjector();

} // namespace crossexpand
