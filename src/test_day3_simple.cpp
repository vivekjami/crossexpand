#include <iostream>
#include "core/template_engine.hpp"
#include "utils/logger.hpp"

using namespace crossexpand;

int main() {
    std::cout << "Testing Day 3 components..." << std::endl;
    
    try {
        // Test basic template engine
        TemplateEngine engine;
        engine.AddTemplate("/test", Template("Hello {name}!"));
        
        Context ctx{{"name", "Day3"}};
        std::string result = engine.Expand("/test", ctx);
        
        std::cout << "Template expansion result: " << result << std::endl;
        
        if (result == "Hello Day3!") {
            std::cout << "✅ Day 3 basic components working!" << std::endl;
            return 0;
        } else {
            std::cout << "❌ Template expansion failed" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
