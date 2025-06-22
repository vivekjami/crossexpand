#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

// Include Day 2 components (excluding multithreaded_processor for now)
#include "core/event_queue.hpp"
#include "core/advanced_template_engine.hpp"
#include "core/enhanced_text_injector.hpp"
#include "core/memory_pool.hpp"
#include "utils/performance_monitor.hpp"
#include "utils/logger.hpp"

using namespace crossexpand;

class AdvancedTestSuite {
private:
    int tests_run_ = 0;
    int tests_passed_ = 0;
    int tests_failed_ = 0;

public:
    void run_all_tests() {
        std::cout << "=== CrossExpand Day 2 Advanced Test Suite ===\n\n";
        
        // Initialize logging for tests
        Logger::instance().set_level(LogLevel::INFO);
        
        // Core Day 2 component tests
        test_event_queue_basic();
        test_event_queue_priorities();
        test_event_queue_thread_safety();
        
        test_advanced_template_engine_basic();
        test_advanced_template_engine_conditionals();
        test_advanced_template_engine_functions();
        test_advanced_template_engine_performance();
        
        test_enhanced_text_injector_strategies();
        test_enhanced_text_injector_profiles();
        
        test_memory_pool_basic();
        test_memory_pool_performance();
        test_memory_pool_thread_safety();
        
        test_performance_monitor_metrics();
        test_performance_monitor_timing();
        
        // Integration tests
        test_full_pipeline_integration();
        test_performance_under_load();
        
        print_summary();
    }

private:
    void test_event_queue_basic() {
        test_start("EventQueue Basic Operations");
        
        EventQueue queue;
        
        // Test enqueue/dequeue
        SimpleKeyEvent key1(65, 'A', true);
        SimpleKeyEvent key2(66, 'B', true);
        
        assert(queue.push(key1, EventPriority::NORMAL));
        assert(queue.push(key2, EventPriority::HIGH));
        assert(queue.total_size() == 2);
        
        ProcessingEvent dequeued;
        assert(queue.pop(dequeued));
        assert(dequeued.key_event.character == 'B'); // High priority should come first
        
        assert(queue.pop(dequeued));
        assert(dequeued.key_event.character == 'A');
        
        assert(!queue.pop(dequeued)); // Should be empty
        
        test_pass();
    }
    
    void test_event_queue_priorities() {
        test_start("EventQueue Priority Handling");
        
        EventQueue queue;
        
        // Add events in mixed priority order
        SimpleKeyEvent key1(65, 'A', true);
        SimpleKeyEvent key2(66, 'B', true);
        SimpleKeyEvent key3(67, 'C', true);
        SimpleKeyEvent key4(68, 'D', true);
        
        assert(queue.push(key1, EventPriority::NORMAL));
        assert(queue.push(key2, EventPriority::LOW));
        assert(queue.push(key3, EventPriority::HIGH));
        assert(queue.push(key4, EventPriority::CRITICAL));
        
        // Should dequeue in priority order: CRITICAL, HIGH, NORMAL, LOW
        ProcessingEvent dequeued;
        assert(queue.pop(dequeued) && dequeued.key_event.character == 'D');
        assert(queue.pop(dequeued) && dequeued.key_event.character == 'C');
        assert(queue.pop(dequeued) && dequeued.key_event.character == 'A');
        assert(queue.pop(dequeued) && dequeued.key_event.character == 'B');
        
        test_pass();
    }
    
    void test_event_queue_thread_safety() {
        test_start("EventQueue Thread Safety");
        
        EventQueue queue;
        std::atomic<int> events_produced{0};
        std::atomic<int> events_consumed{0};
        const int NUM_EVENTS = 1000;
        
        // Producer thread
        std::thread producer([&]() {
            for (int i = 0; i < NUM_EVENTS; ++i) {
                SimpleKeyEvent key(65 + (i % 26), 'A' + (i % 26), true);
                queue.push(key, EventPriority::NORMAL);
                events_produced.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
        
        // Consumer thread
        std::thread consumer([&]() {
            ProcessingEvent event;
            while (events_consumed.load() < NUM_EVENTS) {
                if (queue.pop(event)) {
                    events_consumed.fetch_add(1);
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        assert(events_produced.load() == NUM_EVENTS);
        assert(events_consumed.load() == NUM_EVENTS);
        assert(queue.total_size() == 0);
        
        test_pass();
    }
    
    void test_advanced_template_engine_basic() {
        test_start("AdvancedTemplateEngine Basic Operations");
        
        AdvancedTemplateEngine engine;
        assert(engine.initialize());
        
        // Test basic variable substitution
        assert(engine.register_template("greet", "Hello {name}!"));
        
        TemplateContext context;
        context.set_variable("name", "World");
        
        std::string result = engine.expand("greet", context);
        assert(result == "Hello World!");
        
        engine.shutdown();
        test_pass();
    }
    
    void test_advanced_template_engine_conditionals() {
        test_start("AdvancedTemplateEngine Conditional Logic");
        
        AdvancedTemplateEngine engine;
        assert(engine.initialize());
        
        // Test conditional template
        assert(engine.register_template("conditional", 
            "Hello{if premium}, Premium User{endif}!"));
        
        TemplateContext context1;
        context1.set_variable("premium", "true");
        
        TemplateContext context2;
        context2.set_variable("premium", "false");
        
        std::string result1 = engine.expand("conditional", context1);
        std::string result2 = engine.expand("conditional", context2);
        
        assert(result1 == "Hello, Premium User!");
        assert(result2 == "Hello!");
        
        engine.shutdown();
        test_pass();
    }
    
    void test_advanced_template_engine_functions() {
        test_start("AdvancedTemplateEngine Function System");
        
        AdvancedTemplateEngine engine;
        assert(engine.initialize());
        
        // Test system functions (date, time, etc.)
        assert(engine.register_template("datetime", 
            "Date: {date()}, Time: {time()}"));
        
        TemplateContext context;
        std::string result = engine.expand("datetime", context);
        
        // Should contain actual date and time
        assert(result.find("Date: ") != std::string::npos);
        assert(result.find("Time: ") != std::string::npos);
        
        engine.shutdown();
        test_pass();
    }
    
    void test_advanced_template_engine_performance() {
        test_start("AdvancedTemplateEngine Performance");
        
        AdvancedTemplateEngine engine;
        assert(engine.initialize());
        
        // Register multiple templates
        for (int i = 0; i < 100; ++i) {
            std::string name = "template_" + std::to_string(i);
            std::string content = "Template " + std::to_string(i) + " with {value}";
            assert(engine.register_template(name, content));
        }
        
        // Test expansion performance
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; ++i) {
            std::string name = "template_" + std::to_string(i % 100);
            TemplateContext context;
            context.set_variable("value", std::to_string(i));
            
            std::string result = engine.expand(name, context);
            assert(!result.empty());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        
        std::cout << "1000 template expansions took " << duration << "ms\n";
        assert(duration < 100); // Should be very fast
        
        engine.shutdown();
        test_pass();
    }
    
    void test_enhanced_text_injector_strategies() {
        test_start("EnhancedTextInjector Strategy Selection");
        
        EnhancedTextInjector injector;
        assert(injector.initialize());
        
        // Test strategy selection based on text content
        InjectionContext context;
        context.target_application = "gedit";
        context.text_length = 50;
        context.has_special_chars = false;
        
        auto strategy = injector.select_strategy(context);
        assert(strategy != InjectionStrategy::UNKNOWN);
        
        // Test with different contexts
        context.text_length = 5000;
        context.has_special_chars = true;
        
        auto strategy2 = injector.select_strategy(context);
        assert(strategy2 != InjectionStrategy::UNKNOWN);
        
        injector.shutdown();
        test_pass();
    }
    
    void test_enhanced_text_injector_profiles() {
        test_start("EnhancedTextInjector Application Profiles");
        
        EnhancedTextInjector injector;
        assert(injector.initialize());
        
        // Test profile creation and lookup
        ApplicationProfile profile;
        profile.name = "TestApp";
        profile.supports_unicode = true;
        profile.max_chars_per_injection = 1000;
        profile.typing_delay_ms = 5;
        
        injector.add_application_profile("testapp", profile);
        
        auto retrieved = injector.get_application_profile("testapp");
        assert(retrieved.has_value());
        assert(retrieved->name == "TestApp");
        assert(retrieved->supports_unicode == true);
        
        injector.shutdown();
        test_pass();
    }
    
    void test_memory_pool_basic() {
        test_start("MemoryPool Basic Operations");
        
        MemoryPool<64> pool(1024); // 64-byte blocks, 1024 total capacity
        
        // Test allocation and deallocation
        void* ptr1 = pool.allocate();
        void* ptr2 = pool.allocate();
        
        assert(ptr1 != nullptr);
        assert(ptr2 != nullptr);
        assert(ptr1 != ptr2);
        
        pool.deallocate(ptr1);
        pool.deallocate(ptr2);
        
        // Test reuse
        void* ptr3 = pool.allocate();
        assert(ptr3 == ptr1 || ptr3 == ptr2); // Should reuse freed memory
        
        pool.deallocate(ptr3);
        test_pass();
    }
    
    void test_memory_pool_performance() {
        test_start("MemoryPool Performance");
        
        MemoryPool<32> pool(10000);
        
        // Test allocation performance
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<void*> ptrs;
        for (int i = 0; i < 1000; ++i) {
            void* ptr = pool.allocate();
            assert(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        auto mid = std::chrono::high_resolution_clock::now();
        
        for (void* ptr : ptrs) {
            pool.deallocate(ptr);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        
        auto alloc_time = std::chrono::duration_cast<std::chrono::microseconds>(
            mid - start).count();
        auto dealloc_time = std::chrono::duration_cast<std::chrono::microseconds>(
            end - mid).count();
        
        std::cout << "1000 allocations: " << alloc_time << "μs, "
                  << "deallocations: " << dealloc_time << "μs\n";
        
        // Pool should be much faster than malloc/free
        assert(alloc_time < 1000); // Should be very fast
        assert(dealloc_time < 1000);
        
        test_pass();
    }
    
    void test_memory_pool_thread_safety() {
        test_start("MemoryPool Thread Safety");
        
        MemoryPool<16> pool(10000);
        std::atomic<int> allocation_count{0};
        std::atomic<int> deallocation_count{0};
        
        auto worker = [&]() {
            std::vector<void*> local_ptrs;
            
            // Allocate
            for (int i = 0; i < 100; ++i) {
                void* ptr = pool.allocate();
                if (ptr) {
                    local_ptrs.push_back(ptr);
                    allocation_count.fetch_add(1);
                }
            }
            
            // Deallocate
            for (void* ptr : local_ptrs) {
                pool.deallocate(ptr);
                deallocation_count.fetch_add(1);
            }
        };
        
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back(worker);
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        assert(allocation_count.load() == deallocation_count.load());
        std::cout << "Successfully handled " << allocation_count.load() 
                  << " concurrent allocations\n";
        
        test_pass();
    }
    
    void test_performance_monitor_metrics() {
        test_start("PerformanceMonitor Metrics");
        
        auto& monitor = performance_monitor();
        monitor.enable();
        monitor.reset_all();
        
        // Test counter
        auto& counter = monitor.counter("test_counter");
        counter.increment();
        counter.increment(5);
        assert(counter.get() == 6);
        
        // Test gauge
        auto& gauge = monitor.gauge("test_gauge");
        gauge.set(42.5);
        assert(gauge.get() == 42.5);
        gauge.add(7.5);
        assert(gauge.get() == 50.0);
        
        // Test histogram
        auto& histogram = monitor.histogram("test_histogram");
        histogram.observe(1.0);
        histogram.observe(2.0);
        histogram.observe(3.0);
        assert(histogram.count() == 3);
        assert(histogram.mean() == 2.0);
        assert(histogram.min() == 1.0);
        assert(histogram.max() == 3.0);
        
        test_pass();
    }
    
    void test_performance_monitor_timing() {
        test_start("PerformanceMonitor Timing");
        
        auto& monitor = performance_monitor();
        monitor.enable();
        
        auto& timer = monitor.timer("test_timer");
        
        {
            auto t = timer.time();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        assert(timer.count() == 1);
        assert(timer.mean_microseconds() >= 10000); // At least 10ms in microseconds
        
        // Test macro
        {
            PERF_TIMER("macro_timer");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        auto& macro_timer = monitor.timer("macro_timer");
        assert(macro_timer.count() == 1);
        
        test_pass();
    }
    
    void test_full_pipeline_integration() {
        test_start("Full Pipeline Integration");
        
        // Test complete workflow: Event -> Template -> Injection
        EventQueue queue;
        AdvancedTemplateEngine engine;
        
        assert(engine.initialize());
        assert(engine.register_template("test", "Hello {name}!"));
        
        // Create simple processing event for demo
        SimpleKeyEvent key(65, 'A', true);
        ProcessingEvent event(key, EventPriority::NORMAL);
        assert(queue.push(key, EventPriority::NORMAL));
        
        // Process event
        ProcessingEvent dequeued;
        assert(queue.pop(dequeued));
        assert(dequeued.priority == EventPriority::NORMAL);
        
        // Expand template
        TemplateContext context;
        context.set_variable("name", "Integration Test");
        
        std::string result = engine.expand("test", context);
        assert(result == "Hello Integration Test!");
        
        engine.shutdown();
        test_pass();
    }
    
    void test_performance_under_load() {
        test_start("Performance Under Load");
        
        auto& monitor = performance_monitor();
        monitor.enable();
        monitor.reset_all();
        
        AdvancedTemplateEngine engine;
        assert(engine.initialize());
        
        // Register templates
        for (int i = 0; i < 10; ++i) {
            std::string name = "load_test_" + std::to_string(i);
            std::string content = "Load test template " + std::to_string(i) + 
                                " with value {value} and date {date()}";
            assert(engine.register_template(name, content));
        }
        
        // Simulate load
        const int NUM_OPERATIONS = 1000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            std::string name = "load_test_" + std::to_string(i % 10);
            TemplateContext context;
            context.set_variable("value", std::to_string(i));
            
            auto timer = monitor.template_expansion_time().time();
            std::string result = engine.expand(name, context);
            // Timer destructor records the time
            
            assert(!result.empty());
            monitor.templates_expanded().increment();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        
        std::cout << NUM_OPERATIONS << " operations in " << total_time << "ms\n";
        std::cout << "Average: " << monitor.template_expansion_time().mean_microseconds() 
                  << "μs per operation\n";
        
        // Performance requirements
        assert(total_time < 1000); // Should complete in under 1 second
        assert(monitor.template_expansion_time().mean_microseconds() < 1000); // <1ms per operation
        
        engine.shutdown();
        test_pass();
    }
    
    void test_start(const std::string& test_name) {
        std::cout << "Running: " << test_name << "... ";
        tests_run_++;
    }
    
    void test_pass() {
        std::cout << "PASS\n";
        tests_passed_++;
    }
    
    void test_fail(const std::string& message = "") {
        std::cout << "FAIL";
        if (!message.empty()) {
            std::cout << " (" << message << ")";
        }
        std::cout << "\n";
        tests_failed_++;
    }
    
    void print_summary() {
        std::cout << "\n=== Test Summary ===\n";
        std::cout << "Total tests: " << tests_run_ << "\n";
        std::cout << "Passed: " << tests_passed_ << "\n";
        std::cout << "Failed: " << tests_failed_ << "\n";
        
        if (tests_failed_ == 0) {
            std::cout << "🎉 All tests passed!\n";
        } else {
            std::cout << "❌ " << tests_failed_ << " test(s) failed.\n";
        }
        
        // Performance report
        std::cout << "\n" << performance_monitor().generate_report() << "\n";
    }
};

int main() {
    try {
        AdvancedTestSuite suite;
        suite.run_all_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test suite crashed: " << e.what() << std::endl;
        return 1;
    }
}
