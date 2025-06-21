#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <unordered_set>

namespace crossexpand {

using Context = std::unordered_map<std::string, std::string>;

struct Template {
    std::string text;
    std::vector<std::string> variables;
    std::string description;
    
    Template() = default;
    Template(const std::string& t) : text(t) {}
    Template(const std::string& t, const std::vector<std::string>& vars) 
        : text(t), variables(vars) {}
};

class TemplateEngine {
public:
    TemplateEngine();
    ~TemplateEngine() = default;
    
    // Template management
    void AddTemplate(const std::string& shortcut, const Template& tmpl);
    bool RemoveTemplate(const std::string& shortcut);
    bool HasTemplate(const std::string& shortcut) const;
    
    // Expansion
    std::string Expand(const std::string& shortcut, const Context& context = {}) const;
    
    // Variable management
    void SetVariable(const std::string& name, const std::string& value);
    std::string GetVariable(const std::string& name) const;
    
    // Statistics
    size_t GetTemplateCount() const;
    void ClearCache();

private:
    mutable std::shared_mutex cache_mutex_;
    std::unordered_map<std::string, Template> templates_;
    std::unordered_map<std::string, std::string> global_variables_;
    
    std::string ExpandVariables(const std::string& text, const Context& context) const;
    bool DetectCycle(const std::string& text, std::unordered_set<std::string>& visited) const;
};

} // namespace crossexpand
