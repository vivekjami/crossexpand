#include "core/template_engine.hpp"
#include "utils/logger.hpp"
#include <regex>
#include <algorithm>

namespace crossexpand {

TemplateEngine::TemplateEngine() {
    LOG_DEBUG("TemplateEngine initialized");
}

void TemplateEngine::AddTemplate(const std::string& shortcut, const Template& tmpl) {
    std::lock_guard<std::shared_mutex> lock(cache_mutex_);
    templates_[shortcut] = tmpl;
    LOG_DEBUG("Added template: {}", shortcut);
}

bool TemplateEngine::RemoveTemplate(const std::string& shortcut) {
    std::lock_guard<std::shared_mutex> lock(cache_mutex_);
    auto it = templates_.find(shortcut);
    if (it != templates_.end()) {
        templates_.erase(it);
        LOG_DEBUG("Removed template: {}", shortcut);
        return true;
    }
    return false;
}

bool TemplateEngine::HasTemplate(const std::string& shortcut) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return templates_.find(shortcut) != templates_.end();
}

std::string TemplateEngine::Expand(const std::string& shortcut, const Context& context) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto it = templates_.find(shortcut);
    if (it == templates_.end()) {
        LOG_WARNING("Template not found: {}", shortcut);
        return "";
    }
    
    const auto& tmpl = it->second;
    
    // Cycle detection
    std::unordered_set<std::string> visited;
    if (DetectCycle(tmpl.text, visited)) {
        LOG_ERROR("Cycle detected in template: {}", shortcut);
        return "";
    }
    
    // Expand variables
    std::string result = ExpandVariables(tmpl.text, context);
    
    LOG_DEBUG("Expanded template '{}' to '{}'", shortcut, result);
    return result;
}

void TemplateEngine::SetVariable(const std::string& name, const std::string& value) {
    std::lock_guard<std::shared_mutex> lock(cache_mutex_);
    global_variables_[name] = value;
    LOG_DEBUG("Set variable '{}' = '{}'", name, value);
}

std::string TemplateEngine::GetVariable(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = global_variables_.find(name);
    return it != global_variables_.end() ? it->second : "";
}

size_t TemplateEngine::GetTemplateCount() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return templates_.size();
}

void TemplateEngine::ClearCache() {
    std::lock_guard<std::shared_mutex> lock(cache_mutex_);
    templates_.clear();
    global_variables_.clear();
    LOG_INFO("Template cache cleared");
}

std::string TemplateEngine::ExpandVariables(const std::string& text, const Context& context) const {
    std::string result = text;
    
    // Replace variables in format {variable_name}
    std::regex var_regex(R"(\{([^}]+)\})");
    std::smatch match;
    
    while (std::regex_search(result, match, var_regex)) {
        std::string var_name = match[1].str();
        std::string replacement;
        
        // Check context first, then global variables
        auto ctx_it = context.find(var_name);
        if (ctx_it != context.end()) {
            replacement = ctx_it->second;
        } else {
            auto global_it = global_variables_.find(var_name);
            if (global_it != global_variables_.end()) {
                replacement = global_it->second;
            } else {
                LOG_WARNING("Variable not found: {}", var_name);
                replacement = "{" + var_name + "}"; // Keep original if not found
            }
        }
        
        result = std::regex_replace(result, var_regex, replacement, std::regex_constants::format_first_only);
    }
    
    return result;
}

bool TemplateEngine::DetectCycle(const std::string& text, std::unordered_set<std::string>& visited) const {
    // Simple cycle detection for template references
    std::regex template_regex(R"(/\w+)");
    std::sregex_iterator iter(text.begin(), text.end(), template_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::string referenced_template = iter->str();
        if (visited.find(referenced_template) != visited.end()) {
            return true; // Cycle detected
        }
        
        visited.insert(referenced_template);
        
        auto tmpl_it = templates_.find(referenced_template);
        if (tmpl_it != templates_.end()) {
            if (DetectCycle(tmpl_it->second.text, visited)) {
                return true;
            }
        }
        
        visited.erase(referenced_template);
    }
    
    return false;
}

} // namespace crossexpand
