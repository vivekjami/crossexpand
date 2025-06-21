#include <iostream>
#include <cassert>
#include "core/template_engine.hpp"
#include "utils/config_manager.hpp"

using namespace crossexpand;

void TestTemplateEngine() {
    std::cout << "Testing TemplateEngine..." << std::endl;
    
    TemplateEngine engine;
    
    // Test basic template
    engine.AddTemplate("/hello", Template("Hello, World!"));
    assert(engine.HasTemplate("/hello"));
    assert(engine.Expand("/hello") == "Hello, World!");
    
    // Test variable substitution
    engine.SetVariable("name", "John");
    engine.AddTemplate("/greet", Template("Hello, {name}!"));
    assert(engine.Expand("/greet") == "Hello, John!");
    
    // Test context variables
    Context ctx{{"name", "Alice"}};
    assert(engine.Expand("/greet", ctx) == "Hello, Alice!");
    
    std::cout << "TemplateEngine tests passed!" << std::endl;
}

void TestConfigManager() {
    std::cout << "Testing ConfigManager..." << std::endl;
    
    ConfigManager config;
    
    // Test default templates
    const auto& templates = config.GetTemplates();
    assert(!templates.empty());
    assert(templates.find("/hello") != templates.end());
    
    // Test default variables
    const auto& variables = config.GetVariables();
    assert(!variables.empty());
    assert(variables.find("name") != variables.end());
    
    std::cout << "ConfigManager tests passed!" << std::endl;
}

int main() {
    try {
        TestTemplateEngine();
        TestConfigManager();
        
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
