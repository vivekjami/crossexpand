#include "core/advanced_template_engine.hpp"
#include "utils/logger.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <pwd.h>
#include <uuid/uuid.h>

namespace crossexpand {

// AdvancedTemplate Implementation
AdvancedTemplate::AdvancedTemplate(const std::string& source) 
    : source_(source), is_compiled_(false) {
    LOG_DEBUG("Created advanced template with {} characters", source.length());
}

bool AdvancedTemplate::compile() {
    try {
        size_t pos = 0;
        root_ = parse(source_, pos);
        extract_variables(*root_);
        is_compiled_ = true;
        LOG_DEBUG("Successfully compiled template");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Template compilation failed: {}", e.what());
        is_compiled_ = false;
        return false;
    }
}

std::string AdvancedTemplate::execute(const Context& context) const {
    if (!is_compiled_) {
        LOG_ERROR("Cannot execute uncompiled template");
        return "";
    }
    
    try {
        return execute_node(*root_, context);
    } catch (const std::exception& e) {
        LOG_ERROR("Template execution failed: {}", e.what());
        return "";
    }
}

std::shared_ptr<TemplateNode> AdvancedTemplate::parse(const std::string& text, size_t& pos) {
    auto root = std::make_shared<TemplateNode>(NodeType::LITERAL);
    std::string current_literal;
    
    while (pos < text.length()) {
        if (text.substr(pos, 2) == "{%") {
            // Control structure
            if (!current_literal.empty()) {
                auto literal_node = std::make_shared<TemplateNode>(NodeType::LITERAL, current_literal);
                root->children.push_back(literal_node);
                current_literal.clear();
            }
            
            // Find the matching %}
            size_t end_pos = text.find("%}", pos + 2);
            if (end_pos == std::string::npos) {
                throw std::runtime_error("Unclosed control structure");
            }
            
            std::string control = text.substr(pos + 2, end_pos - pos - 2);
            pos = end_pos + 2;
            
            if (control.substr(0, 2) == "if") {
                root->children.push_back(parse_conditional(text, pos));
            } else if (control.substr(0, 3) == "for") {
                root->children.push_back(parse_loop(text, pos));
            }
        } else if (text.substr(pos, 2) == "{{") {
            // Variable or function
            if (!current_literal.empty()) {
                auto literal_node = std::make_shared<TemplateNode>(NodeType::LITERAL, current_literal);
                root->children.push_back(literal_node);
                current_literal.clear();
            }
            
            size_t end_pos = text.find("}}", pos + 2);
            if (end_pos == std::string::npos) {
                throw std::runtime_error("Unclosed variable");
            }
            
            std::string var_content = text.substr(pos + 2, end_pos - pos - 2);
            pos = end_pos + 2;
            
            if (var_content.find('(') != std::string::npos) {
                // Function call
                auto func_node = std::make_shared<TemplateNode>(NodeType::FUNCTION_CALL, var_content);
                root->children.push_back(func_node);
            } else {
                // Simple variable
                auto var_node = std::make_shared<TemplateNode>(NodeType::VARIABLE, var_content);
                root->children.push_back(var_node);
            }
        } else {
            current_literal += text[pos];
            pos++;
        }
    }
    
    if (!current_literal.empty()) {
        auto literal_node = std::make_shared<TemplateNode>(NodeType::LITERAL, current_literal);
        root->children.push_back(literal_node);
    }
    
    return root;
}

std::shared_ptr<TemplateNode> AdvancedTemplate::parse_conditional(const std::string& text, size_t& pos) {
    // Find the condition in the opening tag
    size_t if_start = text.rfind("{%", pos);
    size_t if_end = text.find("%}", if_start);
    std::string if_tag = text.substr(if_start + 2, if_end - if_start - 2);
    
    auto cond_node = std::make_shared<TemplateNode>(NodeType::CONDITIONAL);
    
    // Extract condition (after "if ")
    if (if_tag.length() > 3) {
        cond_node->attributes["condition"] = if_tag.substr(3);
    }
    
    // Parse the content until {% endif %}
    std::string content;
    int depth = 1;
    
    while (pos < text.length() && depth > 0) {
        if (text.substr(pos, 2) == "{%") {
            size_t tag_end = text.find("%}", pos + 2);
            if (tag_end != std::string::npos) {
                std::string tag = text.substr(pos + 2, tag_end - pos - 2);
                
                if (tag.substr(0, 2) == "if") {
                    depth++;
                } else if (tag == "endif") {
                    depth--;
                    if (depth == 0) {
                        pos = tag_end + 2;
                        break;
                    }
                }
            }
        }
        
        if (depth > 0) {
            content += text[pos];
            pos++;
        }
    }
    
    // Parse the content as child nodes
    size_t content_pos = 0;
    cond_node->children.push_back(parse(content, content_pos));
    
    return cond_node;
}

std::string AdvancedTemplate::execute_node(const TemplateNode& node, const Context& context) const {
    switch (node.type) {
        case NodeType::LITERAL:
            return node.content;
            
        case NodeType::VARIABLE: {
            auto it = context.find(node.content);
            if (it != context.end()) {
                return it->second;
            }
            return "{" + node.content + "}"; // Keep original if not found
        }
        
        case NodeType::CONDITIONAL:
            return execute_conditional(node, context);
            
        case NodeType::FUNCTION_CALL:
            return execute_function(node, context);
            
        default: {
            std::string result;
            for (const auto& child : node.children) {
                result += execute_node(*child, context);
            }
            return result;
        }
    }
}

std::string AdvancedTemplate::execute_conditional(const TemplateNode& node, const Context& context) const {
    auto cond_it = node.attributes.find("condition");
    if (cond_it == node.attributes.end()) {
        return "";
    }
    
    bool condition_result = evaluate_condition(cond_it->second, context);
    
    if (condition_result && !node.children.empty()) {
        return execute_node(*node.children[0], context);
    }
    
    return "";
}

bool AdvancedTemplate::evaluate_condition(const std::string& condition, const Context& context) const {
    // Simple condition evaluation - can be enhanced
    std::regex var_regex(R"(\b(\w+)\b)");
    std::smatch match;
    
    if (std::regex_search(condition, match, var_regex)) {
        std::string var_name = match[1].str();
        auto it = context.find(var_name);
        if (it != context.end()) {
            const std::string& value = it->second;
            return !value.empty() && value != "0" && value != "false";
        }
    }
    
    return false;
}

void AdvancedTemplate::extract_variables(const TemplateNode& node) {
    if (node.type == NodeType::VARIABLE) {
        if (std::find(required_variables_.begin(), required_variables_.end(), node.content) == required_variables_.end()) {
            required_variables_.push_back(node.content);
        }
    }
    
    for (const auto& child : node.children) {
        extract_variables(*child);
    }
}

const std::vector<std::string>& AdvancedTemplate::get_required_variables() const {
    return required_variables_;
}

// SystemVariables Implementation
std::string SystemVariables::get_current_date(const std::string& format) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), format.c_str());
    return oss.str();
}

std::string SystemVariables::get_current_time(const std::string& format) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), format.c_str());
    return oss.str();
}

std::string SystemVariables::get_username() {
    const char* username = getenv("USER");
    if (!username) {
        struct passwd* pw = getpwuid(getuid());
        username = pw ? pw->pw_name : "unknown";
    }
    return std::string(username);
}

std::string SystemVariables::get_hostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "unknown";
}

std::string SystemVariables::get_random_uuid() {
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

std::string SystemVariables::get_random_number(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return std::to_string(dis(gen));
}

void SystemVariables::populate_context(Context& context) {
    context["current_date"] = get_current_date();
    context["current_time"] = get_current_time();
    context["current_datetime"] = get_current_date() + " " + get_current_time();
    context["username"] = get_username();
    context["hostname"] = get_hostname();
    context["random_uuid"] = get_random_uuid();
    context["random_number"] = get_random_number();
}

// FunctionRegistry Implementation
FunctionRegistry::FunctionRegistry() {
    register_builtin_functions();
    LOG_DEBUG("FunctionRegistry initialized with built-in functions");
}

void FunctionRegistry::register_function(const std::string& name, TemplateFunction func) {
    functions_[name] = func;
    LOG_DEBUG("Registered template function: {}", name);
}

void FunctionRegistry::register_builtin_functions() {
    // Date/time functions
    register_function("date", [](const std::vector<std::string>& args, const Context&) {
        std::string format = args.empty() ? "%Y-%m-%d" : args[0];
        return SystemVariables::get_current_date(format);
    });
    
    register_function("time", [](const std::vector<std::string>& args, const Context&) {
        std::string format = args.empty() ? "%H:%M:%S" : args[0];
        return SystemVariables::get_current_time(format);
    });
    
    // String functions
    register_function("upper", [](const std::vector<std::string>& args, const Context&) {
        if (args.empty()) return std::string("");
        std::string result = args[0];
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    });
    
    register_function("lower", [](const std::vector<std::string>& args, const Context&) {
        if (args.empty()) return std::string("");
        std::string result = args[0];
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    });
    
    // Random functions
    register_function("random", [](const std::vector<std::string>& args, const Context&) {
        int min = 0, max = 100;
        if (args.size() >= 2) {
            min = std::stoi(args[0]);
            max = std::stoi(args[1]);
        }
        return SystemVariables::get_random_number(min, max);
    });
    
    register_function("uuid", [](const std::vector<std::string>&, const Context&) {
        return SystemVariables::get_random_uuid();
    });
}

bool FunctionRegistry::has_function(const std::string& name) const {
    return functions_.find(name) != functions_.end();
}

std::string FunctionRegistry::call_function(const std::string& name, 
                                           const std::vector<std::string>& args, 
                                           const Context& context) const {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        return it->second(args, context);
    }
    return "[UNKNOWN_FUNCTION:" + name + "]";
}

std::vector<std::string> FunctionRegistry::get_function_names() const {
    std::vector<std::string> names;
    names.reserve(functions_.size());
    for (const auto& pair : functions_) {
        names.push_back(pair.first);
    }
    return names;
}

// AdvancedTemplateEngine Implementation
AdvancedTemplateEngine::AdvancedTemplateEngine() 
    : function_registry_(std::make_unique<FunctionRegistry>()) {
    LOG_INFO("AdvancedTemplateEngine initialized");
}

bool AdvancedTemplateEngine::add_advanced_template(const std::string& shortcut, const std::string& source) {
    std::lock_guard<std::shared_mutex> lock(advanced_mutex_);
    
    auto advanced_template = std::make_unique<AdvancedTemplate>(source);
    
    if (advanced_template->compile()) {
        compiled_templates_[shortcut] = std::move(advanced_template);
        
        // Also add to base template engine for compatibility
        Template basic_template(source);
        TemplateEngine::AddTemplate(shortcut, basic_template);
        
        LOG_INFO("Added advanced template: {}", shortcut);
        return true;
    } else {
        LOG_ERROR("Failed to compile advanced template: {}", shortcut);
        return false;
    }
}

std::string AdvancedTemplateEngine::expand_advanced(const std::string& shortcut, const Context& context) const {
    std::shared_lock<std::shared_mutex> lock(advanced_mutex_);
    
    auto it = compiled_templates_.find(shortcut);
    if (it != compiled_templates_.end()) {
        // Create enhanced context with system variables
        Context enhanced_context = context;
        SystemVariables::populate_context(enhanced_context);
        
        return it->second->execute(enhanced_context);
    }
    
    // Fallback to basic template engine
    return TemplateEngine::Expand(shortcut, context);
}

bool AdvancedTemplateEngine::compile_template(const std::string& shortcut) {
    // Simple implementation - templates are automatically compiled when added
    std::shared_lock<std::shared_mutex> lock(advanced_mutex_);
    return compiled_templates_.find(shortcut) != compiled_templates_.end();
}

bool AdvancedTemplateEngine::compile_all_templates() {
    // All templates are automatically compiled when added
    return true;
}

std::vector<std::string> AdvancedTemplateEngine::get_required_variables(const std::string& shortcut) const {
    std::shared_lock<std::shared_mutex> lock(advanced_mutex_);
    auto it = compiled_templates_.find(shortcut);
    if (it != compiled_templates_.end()) {
        return it->second->get_required_variables();
    }
    return {};
}

bool AdvancedTemplateEngine::validate_template(const std::string& shortcut) const {
    std::shared_lock<std::shared_mutex> lock(advanced_mutex_);
    return compiled_templates_.find(shortcut) != compiled_templates_.end();
}

std::vector<std::string> AdvancedTemplateEngine::get_validation_errors(const std::string& shortcut) const {
    // Simple implementation - return empty for valid templates
    if (validate_template(shortcut)) {
        return {};
    }
    return {"Template not found: " + shortcut};
}

void AdvancedTemplateEngine::register_custom_function(const std::string& name, TemplateFunction func) {
    if (function_registry_) {
        function_registry_->register_function(name, func);
    }
}

std::vector<std::string> AdvancedTemplateEngine::get_available_functions() const {
    if (function_registry_) {
        return function_registry_->get_function_names();
    }
    return {};
}

AdvancedTemplateEngine::CompilationStats AdvancedTemplateEngine::get_compilation_stats() const {
    std::shared_lock<std::shared_mutex> lock(advanced_mutex_);
    CompilationStats stats;
    stats.total_templates = compiled_templates_.size();
    stats.compiled_templates = compiled_templates_.size();
    stats.failed_compilations = 0;
    stats.total_compile_time = std::chrono::milliseconds(0);
    stats.average_compile_time = std::chrono::milliseconds(0);
    return stats;
}

// Missing function implementations

std::shared_ptr<TemplateNode> AdvancedTemplate::parse_loop(const std::string& text, size_t& pos) {
    // Simple loop parsing - for demonstration purposes
    // In a full implementation, this would parse {for} ... {endfor} constructs
    auto node = std::make_shared<TemplateNode>(NodeType::LITERAL, "[LOOP_PLACEHOLDER]");
    return node;
}

std::string AdvancedTemplate::execute_function(const TemplateNode& node, const Context& context) const {
    // Simple function execution - for demonstration purposes
    if (node.content == "date") {
        return SystemVariables::get_current_date();
    } else if (node.content == "time") {
        return SystemVariables::get_current_time();
    } else if (node.content == "user") {
        return SystemVariables::get_username();
    }
    return "[UNKNOWN_FUNCTION]";
}

} // namespace crossexpand
