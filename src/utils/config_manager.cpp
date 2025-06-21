#include "utils/config_manager.hpp"
#include <fstream>
#include <filesystem>
#include <pwd.h>
#include <unistd.h>

namespace crossexpand {

ConfigManager::ConfigManager() {
    CreateDefaultConfig();
}

bool ConfigManager::LoadConfig(const std::string& config_path) {
    std::string path = config_path.empty() ? GetDefaultConfigPath() : config_path;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_WARNING("Config file not found: {}, creating default", path);
        return SaveConfig(path);
    }
    
    try {
        nlohmann::json json;
        file >> json;
        
        if (DeserializeFromJson(json)) {
            LOG_INFO("Configuration loaded from: {}", path);
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse config file: {}", e.what());
    }
    
    return false;
}

bool ConfigManager::SaveConfig(const std::string& config_path) {
    std::string path = config_path.empty() ? GetDefaultConfigPath() : config_path;
    
    // Create directory if it doesn't exist
    std::filesystem::path file_path(path);
    std::filesystem::create_directories(file_path.parent_path());
    
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file for writing: {}", path);
        return false;
    }
    
    try {
        nlohmann::json json = SerializeToJson();
        file << json.dump(2);
        LOG_INFO("Configuration saved to: {}", path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save config file: {}", e.what());
        return false;
    }
}

const std::unordered_map<std::string, Template>& ConfigManager::GetTemplates() const {
    return templates_;
}

void ConfigManager::SetTemplates(const std::unordered_map<std::string, Template>& templates) {
    templates_ = templates;
}

const std::unordered_map<std::string, std::string>& ConfigManager::GetVariables() const {
    return variables_;
}

void ConfigManager::SetVariables(const std::unordered_map<std::string, std::string>& variables) {
    variables_ = variables;
}

const AppSettings& ConfigManager::GetSettings() const {
    return settings_;
}

void ConfigManager::SetSettings(const AppSettings& settings) {
    settings_ = settings;
}

std::string ConfigManager::GetDefaultConfigPath() const {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    
    return std::string(home) + "/.config/crossexpand/config.json";
}

void ConfigManager::CreateDefaultConfig() {
    // Default templates
    templates_["/hello"] = Template("Hello, World!");
    templates_["/email"] = Template("john.doe@company.com");
    templates_["/sig"] = Template("Best regards,\n{name}\n{title}\n{company}");
    templates_["/addr"] = Template("{company}\n{address}\n{city}, {state} {zip}");
    
    // Default variables
    variables_["name"] = "John Doe";
    variables_["title"] = "Software Engineer";
    variables_["company"] = "Tech Company Inc.";
    variables_["address"] = "123 Main St";
    variables_["city"] = "Anytown";
    variables_["state"] = "ST";
    variables_["zip"] = "12345";
    
    // Default settings
    settings_ = AppSettings();
}

nlohmann::json ConfigManager::SerializeToJson() const {
    nlohmann::json json;
    
    // Version
    json["version"] = "1.0";
    
    // Templates
    nlohmann::json templates_json;
    for (const auto& [shortcut, tmpl] : templates_) {
        nlohmann::json tmpl_json;
        tmpl_json["text"] = tmpl.text;
        if (!tmpl.description.empty()) {
            tmpl_json["description"] = tmpl.description;
        }
        if (!tmpl.variables.empty()) {
            tmpl_json["variables"] = tmpl.variables;
        }
        templates_json[shortcut] = tmpl_json;
    }
    json["templates"] = templates_json;
    
    // Variables
    json["variables"] = variables_;
    
    // Settings
    nlohmann::json settings_json;
    settings_json["expansion_delay_ms"] = settings_.expansion_delay_ms;
    settings_json["max_template_length"] = settings_.max_template_length;
    settings_json["enable_logging"] = settings_.enable_logging;
    settings_json["log_level"] = static_cast<int>(settings_.log_level);
    if (!settings_.log_file.empty()) {
        settings_json["log_file"] = settings_.log_file;
    }
    json["settings"] = settings_json;
    
    return json;
}

bool ConfigManager::DeserializeFromJson(const nlohmann::json& json) {
    try {
        // Templates
        if (json.contains("templates")) {
            templates_.clear();
            for (const auto& [shortcut, tmpl_json] : json["templates"].items()) {
                Template tmpl;
                tmpl.text = tmpl_json["text"];
                if (tmpl_json.contains("description")) {
                    tmpl.description = tmpl_json["description"];
                }
                if (tmpl_json.contains("variables")) {
                    tmpl.variables = tmpl_json["variables"];
                }
                templates_[shortcut] = tmpl;
            }
        }
        
        // Variables
        if (json.contains("variables")) {
            variables_ = json["variables"];
        }
        
        // Settings
        if (json.contains("settings")) {
            const auto& settings_json = json["settings"];
            if (settings_json.contains("expansion_delay_ms")) {
                settings_.expansion_delay_ms = settings_json["expansion_delay_ms"];
            }
            if (settings_json.contains("max_template_length")) {
                settings_.max_template_length = settings_json["max_template_length"];
            }
            if (settings_json.contains("enable_logging")) {
                settings_.enable_logging = settings_json["enable_logging"];
            }
            if (settings_json.contains("log_level")) {
                settings_.log_level = static_cast<LogLevel>(settings_json["log_level"].get<int>());
            }
            if (settings_json.contains("log_file")) {
                settings_.log_file = settings_json["log_file"];
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to deserialize config: {}", e.what());
        return false;
    }
}

} // namespace crossexpand
