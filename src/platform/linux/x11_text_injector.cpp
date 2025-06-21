#include "core/text_injector.hpp"
#include "utils/logger.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <thread>
#include <chrono>

namespace crossexpand {

class X11TextInjector : public TextInjector {
private:
    Display* display_;
    std::string last_error_;

public:
    X11TextInjector() : display_(nullptr) {
        LOG_DEBUG("X11TextInjector created");
    }
    
    ~X11TextInjector() {
        Shutdown();
    }
    
    bool Initialize() override {
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            last_error_ = "Failed to open X11 display";
            LOG_ERROR("{}", last_error_);
            return false;
        }
        
        // Check for XTest extension
        int event_base, error_base, major, minor;
        if (!XTestQueryExtension(display_, &event_base, &error_base, &major, &minor)) {
            last_error_ = "XTest extension not available";
            LOG_ERROR("{}", last_error_);
            XCloseDisplay(display_);
            display_ = nullptr;
            return false;
        }
        
        LOG_INFO("X11TextInjector initialized (XTest v{}.{})", major, minor);
        return true;
    }
    
    void Shutdown() override {
        if (display_) {
            XCloseDisplay(display_);
            display_ = nullptr;
        }
        
        LOG_INFO("X11TextInjector shutdown");
    }
    
    bool InjectText(const std::string& text, InjectionMethod method) override {
        if (!display_) {
            last_error_ = "TextInjector not initialized";
            return false;
        }
        
        switch (method) {
            case InjectionMethod::SIMULATE_TYPING:
                return SimulateTyping(text);
            case InjectionMethod::CLIPBOARD_PASTE:
                return ClipboardPaste(text);
            default:
                last_error_ = "Unsupported injection method";
                return false;
        }
    }
    
    bool DeletePreviousChars(size_t count) override {
        if (!display_) {
            last_error_ = "TextInjector not initialized";
            return false;
        }
        
        // Send backspace key events
        KeyCode backspace_key = XKeysymToKeycode(display_, XK_BackSpace);
        if (backspace_key == 0) {
            last_error_ = "Failed to get backspace keycode";
            return false;
        }
        
        for (size_t i = 0; i < count; ++i) {
            XTestFakeKeyEvent(display_, backspace_key, True, CurrentTime);
            XTestFakeKeyEvent(display_, backspace_key, False, CurrentTime);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        XFlush(display_);
        return true;
    }
    
    bool IsReady() const override {
        return display_ != nullptr;
    }
    
    std::string GetLastError() const override {
        return last_error_;
    }

private:
    bool SimulateTyping(const std::string& text) {
        for (char c : text) {
            if (!TypeCharacter(c)) {
                return false;
            }
            // Small delay to simulate natural typing
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        XFlush(display_);
        return true;
    }
    
    bool TypeCharacter(char c) {
        KeySym keysym = 0;
        bool shift_needed = false;
        
        // Map character to KeySym
        if (c >= 'a' && c <= 'z') {
            keysym = XK_a + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            keysym = XK_A + (c - 'A');
            shift_needed = true;
        } else if (c >= '0' && c <= '9') {
            keysym = XK_0 + (c - '0');
        } else {
            // Handle special characters
            switch (c) {
                case ' ': keysym = XK_space; break;
                case '\n': keysym = XK_Return; break;
                case '\t': keysym = XK_Tab; break;
                case '.': keysym = XK_period; break;
                case ',': keysym = XK_comma; break;
                case '!': keysym = XK_exclam; shift_needed = true; break;
                case '@': keysym = XK_at; shift_needed = true; break;
                case '#': keysym = XK_numbersign; shift_needed = true; break;
                case '$': keysym = XK_dollar; shift_needed = true; break;
                case '%': keysym = XK_percent; shift_needed = true; break;
                case '^': keysym = XK_asciicircum; shift_needed = true; break;
                case '&': keysym = XK_ampersand; shift_needed = true; break;
                case '*': keysym = XK_asterisk; shift_needed = true; break;
                case '(': keysym = XK_parenleft; shift_needed = true; break;
                case ')': keysym = XK_parenright; shift_needed = true; break;
                case '-': keysym = XK_minus; break;
                case '_': keysym = XK_underscore; shift_needed = true; break;
                case '=': keysym = XK_equal; break;
                case '+': keysym = XK_plus; shift_needed = true; break;
                case '[': keysym = XK_bracketleft; break;
                case ']': keysym = XK_bracketright; break;
                case '{': keysym = XK_braceleft; shift_needed = true; break;
                case '}': keysym = XK_braceright; shift_needed = true; break;
                case '\\': keysym = XK_backslash; break;
                case '|': keysym = XK_bar; shift_needed = true; break;
                case ';': keysym = XK_semicolon; break;
                case ':': keysym = XK_colon; shift_needed = true; break;
                case '\'': keysym = XK_apostrophe; break;
                case '"': keysym = XK_quotedbl; shift_needed = true; break;
                case '/': keysym = XK_slash; break;
                case '?': keysym = XK_question; shift_needed = true; break;
                case '<': keysym = XK_less; shift_needed = true; break;
                case '>': keysym = XK_greater; shift_needed = true; break;
                default:
                    LOG_WARNING("Unsupported character: '{}'", c);
                    return true; // Skip unsupported characters
            }
        }
        
        if (keysym == 0) {
            return false;
        }
        
        KeyCode keycode = XKeysymToKeycode(display_, keysym);
        if (keycode == 0) {
            last_error_ = "Failed to get keycode for character";
            return false;
        }
        
        // Press shift if needed
        KeyCode shift_keycode = 0;
        if (shift_needed) {
            shift_keycode = XKeysymToKeycode(display_, XK_Shift_L);
            XTestFakeKeyEvent(display_, shift_keycode, True, CurrentTime);
        }
        
        // Press and release the key
        XTestFakeKeyEvent(display_, keycode, True, CurrentTime);
        XTestFakeKeyEvent(display_, keycode, False, CurrentTime);
        
        // Release shift if needed
        if (shift_needed && shift_keycode != 0) {
            XTestFakeKeyEvent(display_, shift_keycode, False, CurrentTime);
        }
        
        return true;
    }
    
    bool ClipboardPaste(const std::string& text) {
        // This is a simplified implementation
        // In practice, you'd set the clipboard content and then simulate Ctrl+V
        LOG_WARNING("Clipboard paste not yet implemented");
        return SimulateTyping(text);
    }
};

std::unique_ptr<TextInjector> CreateTextInjector() {
    return std::make_unique<X11TextInjector>();
}

} // namespace crossexpand
