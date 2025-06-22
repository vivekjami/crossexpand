#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

namespace crossexpand {

// Plugin interface version for compatibility checking
constexpr int PLUGIN_API_VERSION = 1;

// Plugin types
enum class PluginType {
    TEMPLATE_PROVIDER,
    TEXT_PROCESSOR,
    OUTPUT_FORMATTER,
    CONTEXT_ANALYZER,
    DATA_SOURCE
};

// Plugin metadata
struct PluginInfo {
    std::string name;
    std::string description;
    std::string version;
    std::string author;
    PluginType type;
    int api_version;
    std::vector<std::string> dependencies;
};

// Base plugin interface
class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    // Plugin lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // Plugin information
    virtual PluginInfo get_info() const = 0;
    
    // Plugin capabilities
    virtual bool is_enabled() const = 0;
    virtual void set_enabled(bool enabled) = 0;
    
    // Configuration
    virtual std::string get_config() const = 0;
    virtual bool set_config(const std::string& config) = 0;
    virtual bool has_config_ui() const = 0;
    virtual std::string get_config_ui_html() const = 0;
};

// Template provider plugin interface
class ITemplateProviderPlugin : public IPlugin {
public:
    virtual std::vector<std::string> get_template_categories() const = 0;
    virtual std::vector<std::string> get_templates(const std::string& category) const = 0;
    virtual std::string get_template_content(const std::string& template_name) const = 0;
    virtual bool add_template(const std::string& name, const std::string& content, const std::string& category) = 0;
    virtual bool remove_template(const std::string& name) = 0;
    virtual bool update_template(const std::string& name, const std::string& content) = 0;
};

// Text processor plugin interface
class ITextProcessorPlugin : public IPlugin {
public:
    virtual std::string process_text(const std::string& input, const std::unordered_map<std::string, std::string>& context) = 0;
    virtual std::vector<std::string> get_supported_formats() const = 0;
    virtual bool can_process(const std::string& text) const = 0;
};

// Output formatter plugin interface
class IOutputFormatterPlugin : public IPlugin {
public:
    virtual std::string format_output(const std::string& text, const std::string& format, const std::unordered_map<std::string, std::string>& options) = 0;
    virtual std::vector<std::string> get_supported_formats() const = 0;
    virtual std::string get_format_description(const std::string& format) const = 0;
};

// Context analyzer plugin interface
class IContextAnalyzerPlugin : public IPlugin {
public:
    virtual std::unordered_map<std::string, std::string> analyze_context() = 0;
    virtual std::vector<std::string> get_suggested_templates(const std::unordered_map<std::string, std::string>& context) = 0;
    virtual double get_confidence_score(const std::string& template_name, const std::unordered_map<std::string, std::string>& context) = 0;
};

// Data source plugin interface
class IDataSourcePlugin : public IPlugin {
public:
    virtual std::vector<std::string> get_available_data_sets() const = 0;
    virtual std::unordered_map<std::string, std::string> get_data(const std::string& data_set, const std::unordered_map<std::string, std::string>& parameters) = 0;
    virtual bool is_data_available(const std::string& data_set) const = 0;
    virtual std::string get_data_schema(const std::string& data_set) const = 0;
};

// Plugin factory function type
using PluginFactory = std::function<std::unique_ptr<IPlugin>()>;

// Plugin registry entry
struct PluginEntry {
    std::string path;
    void* handle;
    PluginFactory factory;
    std::unique_ptr<IPlugin> instance;
    bool loaded;
    bool enabled;
};

// Plugin manager class
class PluginManager {
private:
    std::unordered_map<std::string, PluginEntry> plugins_;
    std::vector<std::string> plugin_directories_;
    bool auto_load_enabled_;

public:
    PluginManager();
    ~PluginManager();
    
    // Plugin discovery and loading
    void add_plugin_directory(const std::string& directory);
    void scan_for_plugins();
    bool load_plugin(const std::string& plugin_path);
    bool unload_plugin(const std::string& plugin_name);
    void unload_all_plugins();
    
    // Plugin management
    bool enable_plugin(const std::string& plugin_name);
    bool disable_plugin(const std::string& plugin_name);
    bool is_plugin_loaded(const std::string& plugin_name) const;
    bool is_plugin_enabled(const std::string& plugin_name) const;
    
    // Plugin access
    std::vector<std::string> get_loaded_plugins() const;
    std::vector<PluginInfo> get_plugin_info_list() const;
    IPlugin* get_plugin(const std::string& plugin_name) const;
    
    template<typename T>
    std::vector<T*> get_plugins_of_type() const {
        std::vector<T*> result;
        for (const auto& [name, entry] : plugins_) {
            if (entry.loaded && entry.enabled && entry.instance) {
                if (auto typed_plugin = dynamic_cast<T*>(entry.instance.get())) {
                    result.push_back(typed_plugin);
                }
            }
        }
        return result;
    }
    
    // Configuration
    void set_auto_load(bool enabled);
    bool save_plugin_config() const;
    bool load_plugin_config();
    
    // Plugin dependencies
    bool check_dependencies(const PluginInfo& plugin_info) const;
    std::vector<std::string> get_missing_dependencies(const PluginInfo& plugin_info) const;
};

// Plugin export macros for plugin developers
#define PLUGIN_EXPORT extern "C"
#define PLUGIN_API __attribute__((visibility("default")))

// Required functions that every plugin must implement
PLUGIN_EXPORT PLUGIN_API const char* get_plugin_name();
PLUGIN_EXPORT PLUGIN_API const char* get_plugin_version();
PLUGIN_EXPORT PLUGIN_API int get_api_version();
PLUGIN_EXPORT PLUGIN_API IPlugin* create_plugin();
PLUGIN_EXPORT PLUGIN_API void destroy_plugin(IPlugin* plugin);

} // namespace crossexpand
