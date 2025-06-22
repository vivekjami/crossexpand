#include "core/plugin_system.hpp"
#include "utils/logger.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace crossexpand {

PluginManager::PluginManager() : auto_load_enabled_(true) {
    // Add default plugin directories
    add_plugin_directory("./plugins");
    add_plugin_directory("/usr/local/lib/crossexpand/plugins");
    add_plugin_directory("~/.local/share/crossexpand/plugins");
    
    LOG_INFO("PluginManager initialized");
}

PluginManager::~PluginManager() {
    unload_all_plugins();
    LOG_INFO("PluginManager destroyed");
}

void PluginManager::add_plugin_directory(const std::string& directory) {
    if (std::find(plugin_directories_.begin(), plugin_directories_.end(), directory) == plugin_directories_.end()) {
        plugin_directories_.push_back(directory);
        LOG_DEBUG("Added plugin directory: {}", directory);
    }
}

void PluginManager::scan_for_plugins() {
    LOG_INFO("Scanning for plugins...");
    
    for (const auto& directory : plugin_directories_) {
        if (!std::filesystem::exists(directory)) {
            continue;
        }
        
        try {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    std::string filepath = entry.path().string();
                    
                    // Check for plugin file extensions (C++17 compatible)
                    auto ends_with = [](const std::string& str, const std::string& suffix) {
                        return str.length() >= suffix.length() && 
                               str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
                    };
                    
                    if (ends_with(filepath, ".so") || ends_with(filepath, ".dll") || ends_with(filepath, ".dylib")) {
                        if (auto_load_enabled_) {
                            load_plugin(filepath);
                        } else {
                            LOG_DEBUG("Found plugin (not auto-loading): {}", filepath);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Error scanning plugin directory {}: {}", directory, e.what());
        }
    }
    
    LOG_INFO("Plugin scan complete. Found {} plugins", plugins_.size());
}

bool PluginManager::load_plugin(const std::string& plugin_path) {
    LOG_DEBUG("Loading plugin: {}", plugin_path);
    
    // Check if plugin is already loaded
    std::string plugin_name = std::filesystem::path(plugin_path).stem().string();
    if (is_plugin_loaded(plugin_name)) {
        LOG_WARNING("Plugin {} is already loaded", plugin_name);
        return false;
    }
    
    // Load the shared library
    void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
    if (!handle) {
        LOG_ERROR("Failed to load plugin {}: {}", plugin_path, dlerror());
        return false;
    }
    
    // Clear any existing error
    dlerror();
    
    // Load required functions
    typedef const char* (*GetPluginNameFunc)();
    typedef const char* (*GetPluginVersionFunc)();
    typedef int (*GetApiVersionFunc)();
    typedef IPlugin* (*CreatePluginFunc)();
    typedef void (*DestroyPluginFunc)(IPlugin*);
    
    auto get_plugin_name = (GetPluginNameFunc)dlsym(handle, "get_plugin_name");
    auto get_plugin_version = (GetPluginVersionFunc)dlsym(handle, "get_plugin_version");
    auto get_api_version = (GetApiVersionFunc)dlsym(handle, "get_api_version");
    auto create_plugin = (CreatePluginFunc)dlsym(handle, "create_plugin");
    auto destroy_plugin = (DestroyPluginFunc)dlsym(handle, "destroy_plugin");
    
    const char* dlsym_error = dlerror();
    if (dlsym_error || !get_plugin_name || !get_plugin_version || !get_api_version || !create_plugin || !destroy_plugin) {
        LOG_ERROR("Failed to load plugin functions from {}: {}", plugin_path, dlsym_error ? dlsym_error : "Missing required functions");
        dlclose(handle);
        return false;
    }
    
    // Check API version compatibility
    int api_version = get_api_version();
    if (api_version != PLUGIN_API_VERSION) {
        LOG_ERROR("Plugin {} has incompatible API version {} (expected {})", plugin_path, api_version, PLUGIN_API_VERSION);
        dlclose(handle);
        return false;
    }
    
    // Create plugin instance
    std::unique_ptr<IPlugin> plugin_instance(create_plugin());
    if (!plugin_instance) {
        LOG_ERROR("Failed to create plugin instance for {}", plugin_path);
        dlclose(handle);
        return false;
    }
    
    // Get plugin info
    PluginInfo info = plugin_instance->get_info();
    plugin_name = info.name; // Use the plugin's reported name
    
    // Check dependencies
    if (!check_dependencies(info)) {
        LOG_ERROR("Plugin {} has unmet dependencies", plugin_name);
        dlclose(handle);
        return false;
    }
    
    // Initialize plugin
    if (!plugin_instance->initialize()) {
        LOG_ERROR("Failed to initialize plugin {}", plugin_name);
        dlclose(handle);
        return false;
    }
    
    // Create plugin entry
    PluginEntry entry;
    entry.path = plugin_path;
    entry.handle = handle;
    entry.factory = [create_plugin]() { return std::unique_ptr<IPlugin>(create_plugin()); };
    entry.instance = std::move(plugin_instance);
    entry.loaded = true;
    entry.enabled = true;
    
    plugins_[plugin_name] = std::move(entry);
    
    LOG_INFO("Successfully loaded plugin: {} v{} by {}", info.name, info.version, info.author);
    return true;
}

bool PluginManager::unload_plugin(const std::string& plugin_name) {
    auto it = plugins_.find(plugin_name);
    if (it == plugins_.end()) {
        LOG_WARNING("Plugin {} not found", plugin_name);
        return false;
    }
    
    auto& entry = it->second;
    
    // Shutdown plugin if it's loaded
    if (entry.instance) {
        entry.instance->shutdown();
        entry.instance.reset();
    }
    
    // Unload shared library
    if (entry.handle) {
        dlclose(entry.handle);
    }
    
    plugins_.erase(it);
    LOG_INFO("Unloaded plugin: {}", plugin_name);
    return true;
}

void PluginManager::unload_all_plugins() {
    LOG_INFO("Unloading all plugins...");
    
    for (auto& [name, entry] : plugins_) {
        if (entry.instance) {
            entry.instance->shutdown();
            entry.instance.reset();
        }
        
        if (entry.handle) {
            dlclose(entry.handle);
        }
    }
    
    plugins_.clear();
    LOG_INFO("All plugins unloaded");
}

bool PluginManager::enable_plugin(const std::string& plugin_name) {
    auto it = plugins_.find(plugin_name);
    if (it == plugins_.end()) {
        LOG_WARNING("Plugin {} not found", plugin_name);
        return false;
    }
    
    auto& entry = it->second;
    if (!entry.loaded) {
        LOG_WARNING("Plugin {} is not loaded", plugin_name);
        return false;
    }
    
    entry.enabled = true;
    if (entry.instance) {
        entry.instance->set_enabled(true);
    }
    
    LOG_INFO("Enabled plugin: {}", plugin_name);
    return true;
}

bool PluginManager::disable_plugin(const std::string& plugin_name) {
    auto it = plugins_.find(plugin_name);
    if (it == plugins_.end()) {
        LOG_WARNING("Plugin {} not found", plugin_name);
        return false;
    }
    
    auto& entry = it->second;
    entry.enabled = false;
    if (entry.instance) {
        entry.instance->set_enabled(false);
    }
    
    LOG_INFO("Disabled plugin: {}", plugin_name);
    return true;
}

bool PluginManager::is_plugin_loaded(const std::string& plugin_name) const {
    auto it = plugins_.find(plugin_name);
    return it != plugins_.end() && it->second.loaded;
}

bool PluginManager::is_plugin_enabled(const std::string& plugin_name) const {
    auto it = plugins_.find(plugin_name);
    return it != plugins_.end() && it->second.loaded && it->second.enabled;
}

std::vector<std::string> PluginManager::get_loaded_plugins() const {
    std::vector<std::string> result;
    for (const auto& [name, entry] : plugins_) {
        if (entry.loaded) {
            result.push_back(name);
        }
    }
    return result;
}

std::vector<PluginInfo> PluginManager::get_plugin_info_list() const {
    std::vector<PluginInfo> result;
    for (const auto& [name, entry] : plugins_) {
        if (entry.loaded && entry.instance) {
            result.push_back(entry.instance->get_info());
        }
    }
    return result;
}

IPlugin* PluginManager::get_plugin(const std::string& plugin_name) const {
    auto it = plugins_.find(plugin_name);
    if (it != plugins_.end() && it->second.loaded && it->second.enabled) {
        return it->second.instance.get();
    }
    return nullptr;
}

void PluginManager::set_auto_load(bool enabled) {
    auto_load_enabled_ = enabled;
    LOG_INFO("Auto-load plugins: {}", enabled ? "enabled" : "disabled");
}

bool PluginManager::save_plugin_config() const {
    try {
        nlohmann::json config;
        config["auto_load"] = auto_load_enabled_;
        
        nlohmann::json plugins_array = nlohmann::json::array();
        for (const auto& [name, entry] : plugins_) {
            nlohmann::json plugin_config;
            plugin_config["name"] = name;
            plugin_config["path"] = entry.path;
            plugin_config["enabled"] = entry.enabled;
            
            if (entry.instance) {
                plugin_config["config"] = entry.instance->get_config();
            }
            
            plugins_array.push_back(plugin_config);
        }
        config["plugins"] = plugins_array;
        
        std::ofstream file("plugins_config.json");
        file << config.dump(2);
        
        LOG_INFO("Plugin configuration saved");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save plugin configuration: {}", e.what());
        return false;
    }
}

bool PluginManager::load_plugin_config() {
    try {
        std::ifstream file("plugins_config.json");
        if (!file.is_open()) {
            LOG_INFO("No plugin configuration file found, using defaults");
            return true;
        }
        
        nlohmann::json config;
        file >> config;
        
        if (config.contains("auto_load")) {
            auto_load_enabled_ = config["auto_load"];
        }
        
        if (config.contains("plugins")) {
            for (const auto& plugin_config : config["plugins"]) {
                std::string name = plugin_config["name"];
                std::string path = plugin_config["path"];
                bool enabled = plugin_config.value("enabled", true);
                
                if (load_plugin(path)) {
                    if (!enabled) {
                        disable_plugin(name);
                    }
                    
                    // Apply saved configuration
                    if (plugin_config.contains("config")) {
                        auto it = plugins_.find(name);
                        if (it != plugins_.end() && it->second.instance) {
                            it->second.instance->set_config(plugin_config["config"]);
                        }
                    }
                }
            }
        }
        
        LOG_INFO("Plugin configuration loaded");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load plugin configuration: {}", e.what());
        return false;
    }
}

bool PluginManager::check_dependencies(const PluginInfo& plugin_info) const {
    for (const std::string& dependency : plugin_info.dependencies) {
        if (!is_plugin_loaded(dependency)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> PluginManager::get_missing_dependencies(const PluginInfo& plugin_info) const {
    std::vector<std::string> missing;
    for (const std::string& dependency : plugin_info.dependencies) {
        if (!is_plugin_loaded(dependency)) {
            missing.push_back(dependency);
        }
    }
    return missing;
}

} // namespace crossexpand
