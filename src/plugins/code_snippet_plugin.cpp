#include "core/plugin_system.hpp"
#include "utils/logger.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <algorithm>

namespace crossexpand {
namespace plugins {

class CodeSnippetPlugin : public ITemplateProviderPlugin {
private:
    struct CodeSnippet {
        std::string name;
        std::string content;
        std::string language;
        std::string description;
        std::vector<std::string> tags;
    };
    
    std::unordered_map<std::string, std::vector<CodeSnippet>> snippets_by_category_;
    std::unordered_map<std::string, CodeSnippet> snippets_by_name_;
    bool enabled_;
    std::string config_;

public:
    CodeSnippetPlugin() : enabled_(true) {
        initialize_default_snippets();
    }
    
    // IPlugin interface
    bool initialize() override {
        LOG_INFO("CodeSnippetPlugin initialized");
        return true;
    }
    
    void shutdown() override {
        LOG_INFO("CodeSnippetPlugin shutdown");
    }
    
    PluginInfo get_info() const override {
        PluginInfo info;
        info.name = "Code Snippet Manager";
        info.description = "Manages code snippets for various programming languages";
        info.version = "1.0.0";
        info.author = "CrossExpand Team";
        info.type = PluginType::TEMPLATE_PROVIDER;
        info.api_version = PLUGIN_API_VERSION;
        return info;
    }
    
    bool is_enabled() const override {
        return enabled_;
    }
    
    void set_enabled(bool enabled) override {
        enabled_ = enabled;
    }
    
    std::string get_config() const override {
        nlohmann::json config;
        config["enabled"] = enabled_;
        
        // Export all snippets
        nlohmann::json snippets_json = nlohmann::json::array();
        for (const auto& [name, snippet] : snippets_by_name_) {
            nlohmann::json snippet_json;
            snippet_json["name"] = snippet.name;
            snippet_json["content"] = snippet.content;
            snippet_json["language"] = snippet.language;
            snippet_json["description"] = snippet.description;
            snippet_json["tags"] = snippet.tags;
            snippets_json.push_back(snippet_json);
        }
        config["snippets"] = snippets_json;
        
        return config.dump();
    }
    
    bool set_config(const std::string& config_str) override {
        try {
            auto config = nlohmann::json::parse(config_str);
            
            if (config.contains("enabled")) {
                enabled_ = config["enabled"];
            }
            
            if (config.contains("snippets")) {
                // Clear existing snippets
                snippets_by_category_.clear();
                snippets_by_name_.clear();
                
                // Load snippets from config
                for (const auto& snippet_json : config["snippets"]) {
                    CodeSnippet snippet;
                    snippet.name = snippet_json["name"];
                    snippet.content = snippet_json["content"];
                    snippet.language = snippet_json["language"];
                    snippet.description = snippet_json.value("description", "");
                    snippet.tags = snippet_json.value("tags", std::vector<std::string>());
                    
                    add_snippet(snippet);
                }
            }
            
            config_ = config_str;
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse CodeSnippetPlugin config: {}", e.what());
            return false;
        }
    }
    
    bool has_config_ui() const override {
        return true;
    }
    
    std::string get_config_ui_html() const override {
        return "Code Snippet Manager - Configuration UI (HTML simplified for compilation)";
    }
    
    // ITemplateProviderPlugin interface
    std::vector<std::string> get_template_categories() const override {
        std::vector<std::string> categories;
        for (const auto& [category, snippets] : snippets_by_category_) {
            categories.push_back(category);
        }
        return categories;
    }
    
    std::vector<std::string> get_templates(const std::string& category) const override {
        auto it = snippets_by_category_.find(category);
        if (it != snippets_by_category_.end()) {
            std::vector<std::string> names;
            for (const auto& snippet : it->second) {
                names.push_back(snippet.name);
            }
            return names;
        }
        return {};
    }
    
    std::string get_template_content(const std::string& template_name) const override {
        auto it = snippets_by_name_.find(template_name);
        if (it != snippets_by_name_.end()) {
            return it->second.content;
        }
        return "";
    }
    
    bool add_template(const std::string& name, const std::string& content, const std::string& category) override {
        CodeSnippet snippet;
        snippet.name = name;
        snippet.content = content;
        snippet.language = category;
        snippet.description = "User-defined snippet";
        
        add_snippet(snippet);
        return true;
    }
    
    bool remove_template(const std::string& name) override {
        auto it = snippets_by_name_.find(name);
        if (it != snippets_by_name_.end()) {
            std::string language = it->second.language;
            snippets_by_name_.erase(it);
            
            // Remove from category
            auto& category_snippets = snippets_by_category_[language];
            category_snippets.erase(
                std::remove_if(category_snippets.begin(), category_snippets.end(),
                    [&name](const CodeSnippet& snippet) { return snippet.name == name; }),
                category_snippets.end()
            );
            
            return true;
        }
        return false;
    }
    
    bool update_template(const std::string& name, const std::string& content) override {
        auto it = snippets_by_name_.find(name);
        if (it != snippets_by_name_.end()) {
            it->second.content = content;
            
            // Update in category as well
            auto& category_snippets = snippets_by_category_[it->second.language];
            for (auto& snippet : category_snippets) {
                if (snippet.name == name) {
                    snippet.content = content;
                    break;
                }
            }
            return true;
        }
        return false;
    }

private:
    void add_snippet(const CodeSnippet& snippet) {
        snippets_by_name_[snippet.name] = snippet;
        snippets_by_category_[snippet.language].push_back(snippet);
    }
    
    void initialize_default_snippets() {
        // C++ snippets
        add_snippet({"class_template", R"(class {ClassName} {
private:
    {member_variables}

public:
    {ClassName}();
    ~{ClassName}();
    
    {member_functions}
};)", "cpp", "Basic C++ class template", {"class", "template", "oop"}});

        add_snippet({"for_loop", R"(for (size_t i = 0; i < {size}; ++i) {
    {loop_body}
})", "cpp", "Standard for loop", {"loop", "iteration"}});

        add_snippet({"smart_pointer", R"(auto {variable_name} = std::make_unique<{Type}>({args});)", "cpp", "Smart pointer creation", {"memory", "raii"}});

        // Python snippets
        add_snippet({"class_template", R"(class {ClassName}:
    def __init__(self{, args}):
        {initialization}
    
    def {method_name}(self{, params}):
        {method_body})", "python", "Basic Python class template", {"class", "template", "oop"}});

        add_snippet({"list_comprehension", R"([{expression} for {item} in {iterable} if {condition}])", "python", "List comprehension", {"list", "comprehension", "functional"}});

        add_snippet({"context_manager", R"(with {context_manager} as {variable}:
    {body})", "python", "Context manager usage", {"context", "with", "resource"}});

        // JavaScript snippets
        add_snippet({"function_template", R"(function {functionName}({parameters}) {
    {function_body}
    return {return_value};
})", "javascript", "JavaScript function template", {"function", "template"}});

        add_snippet({"promise_template", R"(const {promiseName} = new Promise((resolve, reject) => {
    {async_operation}
    if ({success_condition}) {
        resolve({result});
    } else {
        reject({error});
    }
});)", "javascript", "Promise template", {"async", "promise", "javascript"}});

        add_snippet({"arrow_function", R"(const {functionName} = ({parameters}) => {
    {function_body}
};)", "javascript", "Arrow function template", {"arrow", "function", "es6"}});
    }
};

// Plugin factory functions
extern "C" {
    PLUGIN_API const char* get_plugin_name() {
        return "Code Snippet Manager";
    }
    
    PLUGIN_API const char* get_plugin_version() {
        return "1.0.0";
    }
    
    PLUGIN_API int get_api_version() {
        return PLUGIN_API_VERSION;
    }
    
    PLUGIN_API IPlugin* create_plugin() {
        return new CodeSnippetPlugin();
    }
    
    PLUGIN_API void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace plugins
} // namespace crossexpand
