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
    
    // Add advanced templates
    engine.add_advanced_template("greeting", "Hello {name}, today is {date()}!");
    engine.add_advanced_template("conditional", "Status: {if premium}Premium User{else}Standard User{endif}");
    engine.add_advanced_template("system", "User: {user()}, Time: {time()}");
    
    // Compile templates
    engine.compile_template("greeting");
    engine.compile_template("conditional");
    engine.compile_template("system");
    
    // Use Context instead of TemplateContext
    Context context;
    context["name"] = "Alice";
    context["premium"] = "true";
    
    // Use expand_advanced instead of expand
    std::cout << "   Template 1: " << engine.expand_advanced("greeting", context) << "\n";
    context["premium"] = "false";
    std::cout << "   Template 2: " << engine.expand_advanced("conditional", context) << "\n";
    std::cout << "   Template 3: " << engine.expand_advanced("system", context) << "\n";
    
    auto engine_stats = engine.get_compilation_stats();
    std::cout << "   Compiled templates: " << engine_stats.compiled_templates << "\n\n";
    
    // 3. Memory Pool System
    std::cout << "3. Memory Pool System:\n";
    
    // Initialize global memory manager
    g_memory_manager = std::make_unique<MemoryPoolManager>();
    
    // Allocate and measure performance
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<void*> ptrs;
    for (int i = 0; i < 100; ++i) {
        void* ptr = g_memory_manager->allocate(64);
        if (ptr) ptrs.push_back(ptr);
    }
    
    auto mid = std::chrono::high_resolution_clock::now();
    
    for (void* ptr : ptrs) {
        g_memory_manager->deallocate(ptr, 64);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    auto alloc_time = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto dealloc_time = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);
    
    std::cout << "   Allocated/deallocated 100 blocks\n";
    std::cout << "   Allocation time: " << alloc_time.count() << " μs\n";
    std::cout << "   Deallocation time: " << dealloc_time.count() << " μs\n";
    
    auto pool_stats = g_memory_manager->get_stats();
    std::cout << "   Pool allocations: " << pool_stats.total_allocations << "\n\n";
    
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
        std::cout << "   Text injector initialized successfully\n";
        
        // Test basic injection
        bool result = injector.InjectText("Hello World", InjectionMethod::SIMULATE_TYPING);
        std::cout << "   Text injection result: " << (result ? "Success" : "Failed") << "\n";
        
        injector.Shutdown();
    } else {
        std::cout << "   Text injector initialization skipped (requires X11)\n";
    }
    
    std::cout << "\n=== Performance Report ===\n";
    std::cout << monitor.generate_report() << "\n";
}

void run_performance_benchmark() {
    std::cout << "\n=== Performance Benchmark ===\n";
    
    auto& monitor = performance_monitor();
    AdvancedTemplateEngine engine;
    
    // Register test templates
    for (int i = 0; i < 10; ++i) {
        std::string name = "bench_" + std::to_string(i);
        std::string content = "Benchmark template " + std::to_string(i) + 
                             " with variable {value} and system info {user()}";
        engine.add_advanced_template(name, content);
        engine.compile_template(name);
    }
    
    const int ITERATIONS = 1000;
    std::cout << "Running " << ITERATIONS << " template expansions...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < ITERATIONS; ++i) {
        std::string template_name = "bench_" + std::to_string(i % 10);
        Context context;
        context["value"] = std::to_string(i);
        
        auto timer = monitor.timer("template_expansion").time();
        std::string result = engine.expand_advanced(template_name, context);
        // Timer automatically records when destroyed
        
        monitor.counter("templates_expanded").increment();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Completed in " << total_time.count() << "ms\n";
    std::cout << "Average per operation: " << 
                 (total_time.count() * 1000.0 / ITERATIONS) << " μs\n";
    std::cout << "Templates per second: " << 
                 (ITERATIONS * 1000.0 / total_time.count()) << "\n";
}

int main() {
    // Initialize logging with simple setup
    auto& logger = Logger::Instance();
    logger.SetLevel(LogLevel::INFO);
    
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
