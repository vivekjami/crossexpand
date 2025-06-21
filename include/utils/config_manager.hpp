#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "core/template_engine.hpp"
#include "utils/logger.hpp"

namespace crossexpand {

struct AppSettings {
    int expansion_delay_ms = 50;
    int max_template_length = 10000;
    bool enable_logging = true;
    LogLevel log_level = LogLevel::INFO;
    std::string log_file = "";
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager() = default;
    
    // Configuration loading/saving
    bool LoadConfig(const std::string& config_path = "");
    bool SaveConfig(const std::string& config_path = "");
    
    // Template access
    const std::unordered_map<std::string, Template>& GetTemplates() const;
    void SetTemplates(const std::unordered_map<std::string, Template>& templates);
    
    // Variables access
    const std::unordered_map<std::string, std::string>& GetVariables() const;
    void SetVariables(const std::unordered_map<std::string, std::string>& variables);
    
    // Settings access
    const AppSettings& GetSettings() const;
    void SetSettings(const AppSettings& settings);
    
    // Default config path
    std::string GetDefaultConfigPath() const;

private:
    std::unordered_map<std::string, Template> templates_;
    std::unordered_map<std::string, std::string> variables_;
    AppSettings settings_;
    
    void CreateDefaultConfig();
    nlohmann::json SerializeToJson() const;
    bool DeserializeFromJson(const nlohmann::json& json);
};

} // namespace crossexpand
