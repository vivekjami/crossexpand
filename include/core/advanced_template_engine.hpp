#pragma once

#include "core/template_engine.hpp"
#include <regex>
#include <variant>
#include <functional>

namespace crossexpand {

// Advanced template node types
enum class NodeType {
    LITERAL,
    VARIABLE,
    CONDITIONAL,
    LOOP,
    FUNCTION_CALL,
    FORM_INPUT
};

struct TemplateNode {
    NodeType type;
    std::string content;
    std::vector<std::shared_ptr<TemplateNode>> children;
    std::unordered_map<std::string, std::string> attributes;
    
    TemplateNode(NodeType t, const std::string& c = "") 
        : type(t), content(c) {}
};

// Advanced template with compiled AST
class AdvancedTemplate {
private:
    std::string source_;
    std::shared_ptr<TemplateNode> root_;
    std::vector<std::string> required_variables_;
    bool is_compiled_;

public:
    AdvancedTemplate(const std::string& source);
    ~AdvancedTemplate() = default;
    
    // Compilation
    bool compile();
    bool is_compiled() const { return is_compiled_; }
    
    // Execution
    std::string execute(const Context& context) const;
    
    // Metadata
    const std::vector<std::string>& get_required_variables() const;
    std::string get_source() const { return source_; }
    
    // Validation
    bool validate() const;
    std::vector<std::string> get_validation_errors() const;

private:
    std::shared_ptr<TemplateNode> parse(const std::string& text, size_t& pos);
    std::shared_ptr<TemplateNode> parse_conditional(const std::string& text, size_t& pos);
    std::shared_ptr<TemplateNode> parse_loop(const std::string& text, size_t& pos);
    std::shared_ptr<TemplateNode> parse_variable(const std::string& text, size_t& pos);
    std::shared_ptr<TemplateNode> parse_function(const std::string& text, size_t& pos);
    std::shared_ptr<TemplateNode> parse_form(const std::string& text, size_t& pos);
    
    std::string execute_node(const TemplateNode& node, const Context& context) const;
    std::string execute_conditional(const TemplateNode& node, const Context& context) const;
    std::string execute_loop(const TemplateNode& node, const Context& context) const;
    std::string execute_function(const TemplateNode& node, const Context& context) const;
    
    bool evaluate_condition(const std::string& condition, const Context& context) const;
    void extract_variables(const TemplateNode& node);
};

// System variable providers
class SystemVariables {
public:
    static std::string get_current_date(const std::string& format = "%Y-%m-%d");
    static std::string get_current_time(const std::string& format = "%H:%M:%S");
    static std::string get_current_datetime(const std::string& format = "%Y-%m-%d %H:%M:%S");
    static std::string get_username();
    static std::string get_hostname();
    static std::string get_clipboard();
    static std::string get_random_uuid();
    static std::string get_random_number(int min = 0, int max = 100);
    
    // Register all system variables in context
    static void populate_context(Context& context);
};

// Function registry for template functions
using TemplateFunction = std::function<std::string(const std::vector<std::string>&, const Context&)>;

class FunctionRegistry {
private:
    std::unordered_map<std::string, TemplateFunction> functions_;

public:
    FunctionRegistry();
    
    void register_function(const std::string& name, TemplateFunction func);
    bool has_function(const std::string& name) const;
    std::string call_function(const std::string& name, 
                             const std::vector<std::string>& args, 
                             const Context& context) const;
    
    std::vector<std::string> get_function_names() const;

private:
    void register_builtin_functions();
};

// Enhanced template engine with advanced features
class AdvancedTemplateEngine : public TemplateEngine {
private:
    std::unordered_map<std::string, std::unique_ptr<AdvancedTemplate>> compiled_templates_;
    std::unique_ptr<FunctionRegistry> function_registry_;
    mutable std::shared_mutex advanced_mutex_;

public:
    AdvancedTemplateEngine();
    ~AdvancedTemplateEngine() = default;
    
    // Advanced template management
    bool add_advanced_template(const std::string& shortcut, const std::string& source);
    bool compile_template(const std::string& shortcut);
    bool compile_all_templates();
    
    // Enhanced expansion with system variables
    std::string expand_advanced(const std::string& shortcut, const Context& context = {}) const;
    
    // Template analysis
    std::vector<std::string> get_required_variables(const std::string& shortcut) const;
    bool validate_template(const std::string& shortcut) const;
    std::vector<std::string> get_validation_errors(const std::string& shortcut) const;
    
    // Function management
    void register_custom_function(const std::string& name, TemplateFunction func);
    std::vector<std::string> get_available_functions() const;
    
    // Performance
    struct CompilationStats {
        size_t total_templates;
        size_t compiled_templates;
        size_t failed_compilations;
        std::chrono::milliseconds total_compile_time;
        std::chrono::milliseconds average_compile_time;
    };
    
    CompilationStats get_compilation_stats() const;
};

} // namespace crossexpand
