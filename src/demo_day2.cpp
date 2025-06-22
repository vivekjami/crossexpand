#include <iostream>
#include <chrono>
#include <thread>

// Day 2 Advanced Components
#include "core/event_queue.hpp"
#include "core/advanced_template_engine.hpp"
#include "core/enhanced_text_injector.hpp"
#include "core/memory_pool.hpp"
#include "utils/performance_monitor.hpp"
#include "utils/logger.hpp"

using namespace crossexpand;

void demonstrate_day2_features() {
    std::cout << "=== CrossExpand Day 2 Advanced Features Demo ===\n\n";
    
    // Initialize performance monitoring
    auto& monitor = performance_monitor();
    monitor.enable();
    monitor.reset_all();
    
    // 1. Event Queue Demonstration
    std::cout << "1. Lock-Free Event Queue:\n";
    EventQueue queue;
    
    // Add events with different priorities
    SimpleKeyEvent key1(65, 'A', true);
    SimpleKeyEvent key2(66, 'B', true);
    SimpleKeyEvent key3(67, 'C', true);
    
    queue.push(key1, EventPriority::LOW);
    queue.push(key2, EventPriority::HIGH);
    queue.push(key3, EventPriority::CRITICAL);
    
    std::cout << "   Added 3 events with different priorities\n";
    std::cout << "   Queue size: " << queue.total_size() << "\n";
    
    // Process events (should come out in priority order)
    ProcessingEvent event;
    while (queue.pop(event)) {
        std::cout << "   Processed event with priority: " 
                  << static_cast<int>(event.priority) << "\n";
    }
    
    auto queue_stats = queue.get_stats();
    std::cout << "   Queue stats - Pushed: " << queue_stats.total_pushed 
              << ", Popped: " << queue_stats.total_popped << "\n\n";
    
    // 2. Advanced Template Engine
    std::cout << "2. Advanced Template Engine:\n";
    AdvancedTemplateEngine engine;
    engine.initialize();
    
    // Register advanced templates
    engine.register_template("greeting", "Hello {name}, today is {date()}!");
    engine.register_template("conditional", "Status: {if premium}Premium User{else}Standard User{endif}");
    engine.register_template("system", "User: {user()}, Time: {time()}");
    
    TemplateContext context;
    context.set_variable("name", "Developer");
    context.set_variable("premium", "true");
    
    std::cout << "   Template 1: " << engine.expand("greeting", context) << "\n";
    std::cout << "   Template 2: " << engine.expand("conditional", context) << "\n";
    std::cout << "   Template 3: " << engine.expand("system", context) << "\n";
    
    auto engine_stats = engine.get_stats();
    std::cout << "   Engine stats - Expansions: " << engine_stats.expansions_performed
              << ", Cache hits: " << engine_stats.cache_hits << "\n\n";
    
    // 3. Memory Pool System
    std::cout << "3. Memory Pool System:\n";
    MemoryPool<64> pool(1024);  // 64-byte blocks
    
    // Allocate and measure performance
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<void*> ptrs;
    for (int i = 0; i < 100; ++i) {
        void* ptr = pool.allocate();
        if (ptr) ptrs.push_back(ptr);
    }
    
    auto mid = std::chrono::high_resolution_clock::now();
    
    for (void* ptr : ptrs) {
        pool.deallocate(ptr);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    auto alloc_time = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto dealloc_time = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);
    
    std::cout << "   Allocated/deallocated 100 blocks\n";
    std::cout << "   Allocation time: " << alloc_time.count() << " μs\n";
    std::cout << "   Deallocation time: " << dealloc_time.count() << " μs\n";
    std::cout << "   Pool utilization: " << pool.utilization() << "%\n\n";
    
    // 4. Performance Monitoring
    std::cout << "4. Performance Monitoring:\n";
    
    // Test different metric types
    monitor.counter("demo_events").increment(10);
    monitor.gauge("memory_usage").set(45.6);
    monitor.histogram("response_times").observe(1.2);
    monitor.histogram("response_times").observe(0.8);
    monitor.histogram("response_times").observe(2.1);
    
    // Timer demonstration
    {
        auto timer = monitor.timer("demo_operation").time();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "   Recorded various metrics\n";
    std::cout << "   Counter value: " << monitor.counter("demo_events").get() << "\n";
    std::cout << "   Gauge value: " << monitor.gauge("memory_usage").get() << "\n";
    std::cout << "   Histogram count: " << monitor.histogram("response_times").count() << "\n";
    std::cout << "   Timer count: " << monitor.timer("demo_operation").count() << "\n\n";
    
    // 5. Enhanced Text Injector
    std::cout << "5. Enhanced Text Injector:\n";
    EnhancedTextInjector injector;
    
    if (injector.Initialize()) {
        // Test strategy selection
        InjectionContext ctx;
        ctx.target_application = "demo";
        ctx.text_length = 50;
        ctx.has_special_chars = false;
        
        auto strategy = injector.select_strategy(ctx);
        std::cout << "   Selected injection strategy: " << static_cast<int>(strategy) << "\n";
        
        // Test Unicode support
        bool unicode_support = injector.supports_unicode_level(UnicodeSupport::FULL_UNICODE);
        std::cout << "   Unicode support: " << (unicode_support ? "Yes" : "No") << "\n";
        
        injector.Shutdown();
    } else {
        std::cout << "   Text injector initialization skipped (requires X11)\n";
    }
    
    std::cout << "\n=== Performance Report ===\n";
    std::cout << monitor.generate_report() << "\n";
    
    engine.shutdown();
}

void run_performance_benchmark() {
    std::cout << "\n=== Performance Benchmark ===\n";
    
    auto& monitor = performance_monitor();
    AdvancedTemplateEngine engine;
    engine.initialize();
    
    // Register test templates
    for (int i = 0; i < 10; ++i) {
        std::string name = "bench_" + std::to_string(i);
        std::string content = "Benchmark template " + std::to_string(i) + 
                             " with variable {value} and system info {user()}";
        engine.register_template(name, content);
    }
    
    const int ITERATIONS = 1000;
    std::cout << "Running " << ITERATIONS << " template expansions...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < ITERATIONS; ++i) {
        std::string template_name = "bench_" + std::to_string(i % 10);
        TemplateContext context;
        context.set_variable("value", std::to_string(i));
        
        auto timer = monitor.template_expansion_time().time();
        std::string result = engine.expand(template_name, context);
        // Timer automatically records when destroyed
        
        monitor.templates_expanded().increment();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Completed in " << total_time.count() << "ms\n";
    std::cout << "Average per operation: " << 
                 monitor.template_expansion_time().mean_microseconds() << " μs\n";
    std::cout << "Templates per second: " << 
                 (ITERATIONS * 1000.0 / total_time.count()) << "\n";
    
    engine.shutdown();
}

int main() {
    // Initialize logging
    Logger::instance().set_level(LogLevel::INFO);
    
    LOG_INFO("Starting CrossExpand Day 2 Advanced Features Demo");
    
    try {
        demonstrate_day2_features();
        run_performance_benchmark();
        
        std::cout << "\n🎉 CrossExpand Day 2 Advanced Features Demo Complete!\n";
        std::cout << "All advanced components working successfully.\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        LOG_ERROR("Demo failed: {}", e.what());
        return 1;
    }
}
