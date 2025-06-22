#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <signal.h>
#include <iomanip>
#include <filesystem>

// Day 3 Components
#include "gui/web_gui.hpp"
#include "core/plugin_system.hpp"
#include "core/advanced_template_engine.hpp"
#include "core/enhanced_text_injector.hpp"
#include "core/memory_pool.hpp"
#include "utils/performance_monitor.hpp"
#include "utils/logger.hpp"
#include "utils/config_manager.hpp"

using namespace crossexpand;

namespace crossexpand {
// Global instances for Day 3
std::unique_ptr<PluginManager> g_plugin_manager;
std::unique_ptr<AdvancedTemplateEngine> g_advanced_template_engine;
std::unique_ptr<WebGUI> g_web_gui;
std::unique_ptr<EnhancedTextInjector> g_text_injector;
} // namespace crossexpand

// Signal handler for graceful shutdown
std::atomic<bool> g_shutdown_requested(false);

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully...\n";
    g_shutdown_requested.store(true);
}

bool initialize_core_systems() {
    std::cout << "🚀 Initializing CrossExpand Day 3 Systems...\n\n";
    
    // Initialize logging
    auto& logger = Logger::Instance();
    logger.SetLevel(LogLevel::INFO);
    LOG_INFO("CrossExpand Day 3 - Full Production System Starting");
    
    // Initialize performance monitoring
    auto& monitor = performance_monitor();
    monitor.enable();
    monitor.reset_all();
    
    // Initialize memory management
    g_memory_manager = std::make_unique<MemoryPoolManager>();
    g_string_intern_pool = std::make_unique<StringInternPool>();
    g_memory_monitor = std::make_unique<MemoryMonitor>();
    LOG_INFO("✅ Memory management system initialized");
    
    // Initialize advanced template engine
    g_advanced_template_engine = std::make_unique<AdvancedTemplateEngine>();
    LOG_INFO("✅ Advanced template engine initialized");
    
    // Initialize plugin system
    g_plugin_manager = std::make_unique<PluginManager>();
    
    // Add built-in plugins (we'll build them as part of the main executable for simplicity)
    std::filesystem::create_directories("./plugins");
    
    // Scan for and load plugins
    g_plugin_manager->scan_for_plugins();
    LOG_INFO("✅ Plugin system initialized with {} plugins", g_plugin_manager->get_loaded_plugins().size());
    
    // Initialize text injector
    g_text_injector = std::make_unique<EnhancedTextInjector>();
    if (g_text_injector->Initialize()) {
        LOG_INFO("✅ Enhanced text injector initialized");
    } else {
        LOG_WARNING("⚠️ Text injector initialization failed (X11 not available?)");
    }
    
    // Add some sample templates
    g_advanced_template_engine->add_advanced_template("greeting", "Hello {name}, welcome to CrossExpand Day 3!");
    g_advanced_template_engine->add_advanced_template("email_signature", 
        "Best regards,\n{user()}\n{company}\nEmail: {email}\nPhone: {phone}");
    g_advanced_template_engine->add_advanced_template("current_datetime", 
        "Current date and time: {date()} at {time()}");
    g_advanced_template_engine->add_advanced_template("code_comment", 
        "// Created by {user()} on {date()}\n// {description}");
    
    LOG_INFO("✅ Sample templates loaded");
    
    return true;
}

bool initialize_web_gui() {
    std::cout << "🌐 Starting Web GUI Interface...\n";
    
    // Initialize web GUI
    g_web_gui = std::make_unique<WebGUI>(8080);
    
    if (!g_web_gui->initialize()) {
        LOG_ERROR("Failed to initialize web GUI");
        return false;
    }
    
    std::cout << "✅ Web GUI started successfully!\n";
    std::cout << "🌍 Access the interface at: " << g_web_gui->get_gui_url() << "\n\n";
    
    return true;
}

void run_demonstration() {
    std::cout << "🧪 Running CrossExpand Day 3 Feature Demonstration...\n\n";
    
    // Demonstrate template expansion
    std::cout << "📝 Template Expansion Demo:\n";
    Context context;
    context["name"] = "Developer";
    context["company"] = "CrossExpand Corp";
    context["email"] = "dev@crossexpand.com";
    context["phone"] = "+1-555-0123";
    context["description"] = "Advanced text expansion system";
    
    auto templates = {"greeting", "email_signature", "current_datetime", "code_comment"};
    for (const auto& template_name : templates) {
        auto result = g_advanced_template_engine->expand_advanced(template_name, context);
        std::cout << "  " << template_name << ": " << result << "\n";
    }
    std::cout << "\n";
    
    // Demonstrate plugin system
    std::cout << "🔌 Plugin System Demo:\n";
    auto loaded_plugins = g_plugin_manager->get_loaded_plugins();
    std::cout << "  Loaded plugins: " << loaded_plugins.size() << "\n";
    for (const auto& plugin_name : loaded_plugins) {
        auto plugin = g_plugin_manager->get_plugin(plugin_name);
        if (plugin) {
            auto info = plugin->get_info();
            std::cout << "    - " << info.name << " v" << info.version << " by " << info.author << "\n";
        }
    }
    std::cout << "\n";
    
    // Demonstrate performance monitoring
    std::cout << "📊 Performance Monitoring Demo:\n";
    auto& monitor = performance_monitor();
    
    // Simulate some operations
    for (int i = 0; i < 100; ++i) {
        auto timer = monitor.timer("demo_operations").time();
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        monitor.counter("operations_performed").increment();
    }
    
    std::cout << "  Operations performed: " << monitor.counter("operations_performed").get() << "\n";
    std::cout << "  Average operation time: " << monitor.timer("demo_operations").mean_microseconds() << " μs\n";
    std::cout << "  System uptime: " << monitor.uptime().count() << " ms\n\n";
    
    // Demonstrate memory pool efficiency
    std::cout << "💾 Memory Pool Demo:\n";
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<void*> allocations;
    for (int i = 0; i < 1000; ++i) {
        void* ptr = g_memory_manager->allocate(64);
        if (ptr) allocations.push_back(ptr);
    }
    
    auto mid = std::chrono::high_resolution_clock::now();
    
    for (void* ptr : allocations) {
        g_memory_manager->deallocate(ptr, 64);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    auto alloc_time = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto dealloc_time = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);
    
    std::cout << "  1000 allocations: " << alloc_time.count() << " μs\n";
    std::cout << "  1000 deallocations: " << dealloc_time.count() << " μs\n";
    auto stats = g_memory_manager->get_stats();
    std::cout << "  Total pool allocations: " << stats.total_allocations << "\n";
    std::cout << "  Pool hit rate: " << stats.pool_hit_rate_percent << "%\n\n";
}

void print_status_update() {
    auto& monitor = performance_monitor();        std::cout << "\r📈 Status: " 
              << "Uptime: " << (monitor.uptime().count() / 1000) << "s | "
              << "Templates: " << monitor.counter("templates_expanded").get() << " | "
              << "Memory: " << std::fixed << std::setprecision(1) << monitor.gauge("memory_usage").get() << "MB | "
              << "Web GUI: " << (g_web_gui->is_running() ? "✅" : "❌") << "   " << std::flush;
}

void main_loop() {
    std::cout << "🎯 CrossExpand Day 3 is now running!\n";
    std::cout << "   - Web GUI: " << g_web_gui->get_gui_url() << "\n";
    std::cout << "   - Text expansion: Active\n";
    std::cout << "   - Plugin system: Active\n";
    std::cout << "   - Performance monitoring: Active\n\n";
    std::cout << "Press Ctrl+C to shutdown gracefully...\n\n";
    
    auto last_status_update = std::chrono::steady_clock::now();
    
    while (!g_shutdown_requested.load()) {
        // Update performance metrics
        auto& monitor = performance_monitor();
        monitor.gauge("memory_usage").set(g_memory_monitor->get_current_usage() / (1024.0 * 1024.0));
        
        // Print status update every 5 seconds
        auto now = std::chrono::steady_clock::now();
        if (now - last_status_update >= std::chrono::seconds(5)) {
            print_status_update();
            last_status_update = now;
        }
        
        // Check web GUI health
        if (!g_web_gui->is_running()) {
            LOG_WARNING("Web GUI stopped unexpectedly, attempting restart...");
            if (!g_web_gui->initialize()) {
                LOG_ERROR("Failed to restart web GUI");
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n";
}

void shutdown_systems() {
    std::cout << "🛑 Shutting down CrossExpand Day 3 systems...\n";
    
    // Shutdown in reverse order of initialization
    if (g_web_gui) {
        g_web_gui->shutdown();
        g_web_gui.reset();
        std::cout << "✅ Web GUI shutdown complete\n";
    }
    
    if (g_text_injector) {
        g_text_injector->Shutdown();
        g_text_injector.reset();
        std::cout << "✅ Text injector shutdown complete\n";
    }
    
    if (g_plugin_manager) {
        g_plugin_manager->save_plugin_config();
        g_plugin_manager.reset();
        std::cout << "✅ Plugin system shutdown complete\n";
    }
    
    if (g_advanced_template_engine) {
        g_advanced_template_engine.reset();
        std::cout << "✅ Template engine shutdown complete\n";
    }
    
    // Clean up memory management (global destructors will handle this)
    std::cout << "✅ Memory management shutdown complete\n";
    
    LOG_INFO("CrossExpand Day 3 shutdown complete");
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << R"(
╔══════════════════════════════════════════════════════════╗
║  🚀 CrossExpand Day 3 - Complete Production System       ║
║                                                          ║
║  Features:                                               ║
║  ✅ Advanced Template Engine with AST compilation        ║
║  ✅ Plugin System with dynamic loading                   ║
║  ✅ Web-based GUI with real-time monitoring             ║
║  ✅ Enhanced Text Injection with X11 integration        ║
║  ✅ Memory Pool System with performance optimization     ║
║  ✅ Comprehensive Performance Monitoring                 ║
║  ✅ Production-ready logging and error handling         ║
║                                                          ║
║  Ready for enterprise deployment! 🎉                    ║
╚══════════════════════════════════════════════════════════╝
    )" << std::endl;
    
    try {
        // Initialize all core systems
        if (!initialize_core_systems()) {
            std::cerr << "❌ Failed to initialize core systems\n";
            return 1;
        }
        
        // Initialize web GUI
        if (!initialize_web_gui()) {
            std::cerr << "❌ Failed to initialize web GUI\n";
            return 1;
        }
        
        // Run feature demonstration
        run_demonstration();
        
        // Enter main event loop
        main_loop();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << std::endl;
        LOG_FATAL("Fatal error in main: {}", e.what());
        return 1;
    }
    
    // Graceful shutdown
    shutdown_systems();
    
    std::cout << "\n🎉 CrossExpand Day 3 - Complete! Thank you for using our system.\n";
    return 0;
}
