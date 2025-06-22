#include "gui/web_gui.hpp"
#include "utils/logger.hpp"
#include "utils/performance_monitor.hpp"
#include "core/plugin_system.hpp"
#include "core/advanced_template_engine.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace crossexpand {

// External instances
extern std::unique_ptr<PluginManager> g_plugin_manager;
extern std::unique_ptr<AdvancedTemplateEngine> g_advanced_template_engine;

WebGUI::WebGUI(int port) : auto_open_browser_(true) {
    server_ = std::make_unique<WebServer>(port);
    templates_directory_ = "./web/templates";
}

WebGUI::~WebGUI() {
    shutdown();
}

bool WebGUI::initialize() {
    LOG_INFO("Initializing Web GUI...");
    
    // Note: Global components are initialized in main_day3.cpp before this is called
    
    // Setup routes
    setup_api_routes();
    setup_static_routes();
    
    // Start server
    if (!server_->start()) {
        LOG_ERROR("Failed to start web server");
        return false;
    }
    
    LOG_INFO("Web GUI initialized successfully at {}", get_gui_url());
    
    if (auto_open_browser_) {
        open_in_browser();
    }
    
    return true;
}

void WebGUI::shutdown() {
    if (server_) {
        server_->stop();
    }
    LOG_INFO("Web GUI shutdown complete");
}

bool WebGUI::is_running() const {
    return server_ && server_->is_running();
}

void WebGUI::set_auto_open_browser(bool auto_open) {
    auto_open_browser_ = auto_open;
}

void WebGUI::set_templates_directory(const std::string& directory) {
    templates_directory_ = directory;
}

std::string WebGUI::get_gui_url() const {
    return server_ ? server_->get_server_url() : "";
}

bool WebGUI::open_in_browser() const {
    std::string url = get_gui_url();
    if (url.empty()) {
        return false;
    }
    
    std::string command;
#ifdef __linux__
    command = "xdg-open '" + url + "' 2>/dev/null &";
#elif __APPLE__
    command = "open '" + url + "'";
#elif _WIN32
    command = "start '" + url + "'";
#else
    LOG_WARNING("Cannot auto-open browser on this platform");
    return false;
#endif
    
    int result = std::system(command.c_str());
    if (result == 0) {
        LOG_INFO("Opened browser to {}", url);
        return true;
    } else {
        LOG_WARNING("Failed to open browser automatically");
        return false;
    }
}

void WebGUI::setup_api_routes() {
    // API status endpoint
    server_->register_handler("/api/status", [this](const HttpRequest& req, HttpResponse& res) {
        handle_api_status(req, res);
    });
    
    // Template management endpoints
    server_->register_handler("/api/templates", [this](const HttpRequest& req, HttpResponse& res) {
        handle_api_templates(req, res);
    });
    
    server_->register_handler("/api/expand", [this](const HttpRequest& req, HttpResponse& res) {
        handle_api_expand(req, res);
    });
    
    server_->register_handler("/api/preview", [this](const HttpRequest& req, HttpResponse& res) {
        handle_api_preview(req, res);
    });
    
    // Plugin management endpoints
    server_->register_handler("/api/plugins", [this](const HttpRequest& req, HttpResponse& res) {
        handle_api_plugins(req, res);
    });
    
    server_->register_handler("/api/plugins/reload", [this](const HttpRequest& req, HttpResponse& res) {
        if (req.method == "POST") {
            try {
                if (g_plugin_manager) {
                    g_plugin_manager->scan_for_plugins();
                }
                nlohmann::json response;
                response["success"] = true;
                response["message"] = "Plugins reloaded successfully";
                res.set_json_content(response.dump());
            } catch (const std::exception& e) {
                nlohmann::json error;
                error["success"] = false;
                error["error"] = e.what();
                res.set_json_content(error.dump());
                res.status_code = 500;
            }
        } else {
            res.set_error(405, "Method Not Allowed");
        }
    });
    
    // Performance monitoring endpoints
    server_->register_handler("/api/performance", [this](const HttpRequest& req, HttpResponse& res) {
        handle_api_performance(req, res);
    });
    
    // Configuration endpoints
    server_->register_handler("/api/config", [this](const HttpRequest& req, HttpResponse& res) {
        handle_api_config(req, res);
    });
    
    server_->register_handler("/api/config/export", [this](const HttpRequest& req, HttpResponse& res) {
        try {
            nlohmann::json config;
            config["version"] = "1.0.0";
            config["timestamp"] = std::time(nullptr);
            
            // Export plugin configuration
            if (g_plugin_manager) {
                config["plugins"] = nlohmann::json::array();
                for (const auto& plugin_name : g_plugin_manager->get_loaded_plugins()) {
                    auto plugin = g_plugin_manager->get_plugin(plugin_name);
                    if (plugin) {
                        nlohmann::json plugin_config;
                        plugin_config["name"] = plugin_name;
                        plugin_config["enabled"] = g_plugin_manager->is_plugin_enabled(plugin_name);
                        plugin_config["config"] = plugin->get_config();
                        config["plugins"].push_back(plugin_config);
                    }
                }
            }
            
            res.headers["Content-Disposition"] = "attachment; filename=crossexpand-config.json";
            res.set_json_content(config.dump(2));
        } catch (const std::exception& e) {
            res.set_error(500, "Failed to export configuration");
        }
    });
}

void WebGUI::setup_static_routes() {
    server_->setup_default_routes();
    
    // Additional static routes can be added here
    server_->register_handler("/templates", [this](const HttpRequest& req, HttpResponse& res) {
        res.set_html_content(generate_dashboard_html());
    });
    
    server_->register_handler("/plugins", [this](const HttpRequest& req, HttpResponse& res) {
        res.set_html_content(generate_plugin_management_html());
    });
}

void WebGUI::handle_api_status(const HttpRequest& request, HttpResponse& response) {
    try {
        nlohmann::json status;
        status["status"] = "running";
        status["version"] = "3.0.0";
        status["uptime_ms"] = performance_monitor().uptime().count();
        
        // Template statistics
        status["templates"] = 0; // TODO: Get actual count from template engine
        status["total_expansions"] = performance_monitor().counter("templates_expanded").get();
        status["avg_response_time"] = performance_monitor().timer("template_expansion").mean_microseconds();
        
        // Plugin statistics
        if (g_plugin_manager) {
            status["loaded_plugins"] = g_plugin_manager->get_loaded_plugins().size();
        } else {
            status["loaded_plugins"] = 0;
        }
        
        // Performance metrics
        auto& monitor = performance_monitor();
        status["memory_usage_mb"] = monitor.gauge("memory_usage").get();
        status["cpu_usage_percent"] = monitor.gauge("cpu_usage").get();
        
        response.set_json_content(status.dump());
    } catch (const std::exception& e) {
        response.set_error(500, "Failed to get status: " + std::string(e.what()));
    }
}

void WebGUI::handle_api_templates(const HttpRequest& request, HttpResponse& response) {
    try {
        nlohmann::json templates_json = nlohmann::json::array();
        
        // Get templates from plugin providers
        if (g_plugin_manager) {
            auto providers = g_plugin_manager->get_plugins_of_type<ITemplateProviderPlugin>();
            for (auto provider : providers) {
                auto categories = provider->get_template_categories();
                for (const auto& category : categories) {
                    auto templates = provider->get_templates(category);
                    for (const auto& template_name : templates) {
                        nlohmann::json template_info;
                        template_info["name"] = template_name;
                        template_info["category"] = category;
                        template_info["content"] = provider->get_template_content(template_name);
                        template_info["provider"] = provider->get_info().name;
                        templates_json.push_back(template_info);
                    }
                }
            }
        }
        
        response.set_json_content(templates_json.dump());
    } catch (const std::exception& e) {
        response.set_error(500, "Failed to get templates: " + std::string(e.what()));
    }
}

void WebGUI::handle_api_plugins(const HttpRequest& request, HttpResponse& response) {
    try {
        nlohmann::json plugins_json = nlohmann::json::array();
        
        if (g_plugin_manager) {
            auto plugin_infos = g_plugin_manager->get_plugin_info_list();
            for (const auto& info : plugin_infos) {
                nlohmann::json plugin_json;
                plugin_json["name"] = info.name;
                plugin_json["description"] = info.description;
                plugin_json["version"] = info.version;
                plugin_json["author"] = info.author;
                plugin_json["type"] = static_cast<int>(info.type);
                plugin_json["enabled"] = g_plugin_manager->is_plugin_enabled(info.name);
                plugin_json["loaded"] = g_plugin_manager->is_plugin_loaded(info.name);
                
                auto plugin = g_plugin_manager->get_plugin(info.name);
                if (plugin) {
                    plugin_json["has_config_ui"] = plugin->has_config_ui();
                }
                
                plugins_json.push_back(plugin_json);
            }
        }
        
        response.set_json_content(plugins_json.dump());
    } catch (const std::exception& e) {
        response.set_error(500, "Failed to get plugins: " + std::string(e.what()));
    }
}

void WebGUI::handle_api_performance(const HttpRequest& request, HttpResponse& response) {
    try {
        auto& monitor = performance_monitor();
        nlohmann::json perf_data;
        
        // System metrics
        perf_data["uptime_ms"] = monitor.uptime().count();
        perf_data["memory_usage_mb"] = monitor.gauge("memory_usage").get();
        perf_data["cpu_usage_percent"] = monitor.gauge("cpu_usage").get();
        
        // Operation counters
        perf_data["templates_expanded"] = monitor.counter("templates_expanded").get();
        perf_data["events_processed"] = monitor.counter("events_processed").get();
        perf_data["text_injections"] = monitor.counter("text_injections").get();
        
        // Timing metrics
        auto& template_timer = monitor.timer("template_expansion");
        perf_data["template_expansion"] = nlohmann::json{
            {"count", template_timer.count()},
            {"mean_us", template_timer.mean_microseconds()},
            {"p95_us", template_timer.p95_microseconds()},
            {"p99_us", template_timer.p99_microseconds()}
        };
        
        response.set_json_content(perf_data.dump());
    } catch (const std::exception& e) {
        response.set_error(500, "Failed to get performance data: " + std::string(e.what()));
    }
}

void WebGUI::handle_api_expand(const HttpRequest& request, HttpResponse& response) {
    if (request.method != "POST") {
        response.set_error(405, "Method Not Allowed");
        return;
    }
    
    try {
        auto request_data = nlohmann::json::parse(request.body);
        std::string template_text = request_data["template"];
        
        Context context;
        if (request_data.contains("context")) {
            for (const auto& [key, value] : request_data["context"].items()) {
                context[key] = value.get<std::string>();
            }
        }
        
        // Perform template expansion
        std::string result;
        if (g_advanced_template_engine) {
            // Use advanced template engine
            g_advanced_template_engine->add_advanced_template("temp_expand", template_text);
            result = g_advanced_template_engine->expand_advanced("temp_expand", context);
        } else {
            result = "Template engine not available";
        }
        
        // Record performance metrics
        performance_monitor().counter("templates_expanded").increment();
        
        nlohmann::json response_data;
        response_data["success"] = true;
        response_data["result"] = result;
        response_data["template"] = template_text;
        response_data["context"] = context;
        
        response.set_json_content(response_data.dump());
    } catch (const std::exception& e) {
        nlohmann::json error_response;
        error_response["success"] = false;
        error_response["error"] = e.what();
        response.set_json_content(error_response.dump());
        response.status_code = 400;
    }
}

void WebGUI::handle_api_preview(const HttpRequest& request, HttpResponse& response) {
    handle_api_expand(request, response); // Same functionality for now
}

void WebGUI::handle_api_config(const HttpRequest& request, HttpResponse& response) {
    if (request.method == "GET") {
        // Return current configuration
        nlohmann::json config;
        config["auto_open_browser"] = auto_open_browser_;
        config["templates_directory"] = templates_directory_;
        response.set_json_content(config.dump());
    } else if (request.method == "POST") {
        // Update configuration
        try {
            auto new_config = nlohmann::json::parse(request.body);
            
            if (new_config.contains("auto_open_browser")) {
                auto_open_browser_ = new_config["auto_open_browser"];
            }
            
            if (new_config.contains("templates_directory")) {
                templates_directory_ = new_config["templates_directory"];
            }
            
            nlohmann::json response_data;
            response_data["success"] = true;
            response_data["message"] = "Configuration updated";
            response.set_json_content(response_data.dump());
        } catch (const std::exception& e) {
            response.set_error(400, "Invalid configuration: " + std::string(e.what()));
        }
    } else {
        response.set_error(405, "Method Not Allowed");
    }
}

std::string WebGUI::generate_main_html() {
    return "Main HTML template"; // Simplified for now
}

std::string WebGUI::generate_dashboard_html() {
    return R"(
<!DOCTYPE html>
<html>
<head>
    <title>CrossExpand - Templates</title>
    <meta charset="utf-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }
        .nav { margin-bottom: 20px; }
        .nav a { text-decoration: none; color: #007acc; margin-right: 20px; }
        .template-list { display: grid; gap: 15px; }
        .template-item { border: 1px solid #ddd; padding: 15px; border-radius: 4px; }
        .template-name { font-weight: bold; color: #007acc; }
        .template-category { color: #666; font-size: 0.9em; }
        .template-content { background: #f8f9fa; padding: 10px; margin-top: 10px; font-family: monospace; }
    </style>
</head>
<body>
    <div class="container">
        <div class="nav">
            <a href="/">← Back to Dashboard</a>
        </div>
        <h1>Template Management</h1>
        <div id="template-list" class="template-list">
            Loading templates...
        </div>
    </div>
    
    <script>
        async function loadTemplates() {
            try {
                const response = await fetch('/api/templates');
                const templates = await response.json();
                
                const container = document.getElementById('template-list');
                container.innerHTML = '';
                
                if (templates.length === 0) {
                    container.innerHTML = '<p>No templates found. Load some plugins to see templates.</p>';
                    return;
                }
                
                templates.forEach(template => {
                    const item = document.createElement('div');
                    item.className = 'template-item';
                    item.innerHTML = `
                        <div class="template-name">${template.name}</div>
                        <div class="template-category">Category: ${template.category} | Provider: ${template.provider}</div>
                        <div class="template-content">${template.content}</div>
                    `;
                    container.appendChild(item);
                });
            } catch (err) {
                document.getElementById('template-list').innerHTML = '<p>Error loading templates: ' + err.message + '</p>';
            }
        }
        
        loadTemplates();
    </script>
</body>
</html>
    )";
}

std::string WebGUI::generate_plugin_management_html() {
    return R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>CrossExpand - Plugins</title>
    <meta charset="utf-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }
        .nav { margin-bottom: 20px; }
        .nav a { text-decoration: none; color: #007acc; margin-right: 20px; }
        .plugin-list { display: grid; gap: 15px; }
        .plugin-item { border: 1px solid #ddd; padding: 15px; border-radius: 4px; }
        .plugin-name { font-weight: bold; color: #007acc; }
        .plugin-info { color: #666; font-size: 0.9em; margin: 5px 0; }
        .plugin-status { display: inline-block; padding: 3px 8px; border-radius: 3px; font-size: 0.8em; }
        .status-enabled { background: #d4edda; color: #155724; }
        .status-disabled { background: #f8d7da; color: #721c24; }
        .plugin-actions { margin-top: 10px; }
        .btn { padding: 5px 10px; margin-right: 5px; border: none; border-radius: 3px; cursor: pointer; }
        .btn-primary { background: #007acc; color: white; }
        .btn-danger { background: #dc3545; color: white; }
    </style>
</head>
<body>
    <div class="container">
        <div class="nav">
            <a href="/">← Back to Dashboard</a>
        </div>
        <h1>Plugin Management</h1>
        <div style="margin-bottom: 20px;">
            <button class="btn btn-primary" onclick="reloadPlugins()">Reload All Plugins</button>
        </div>
        <div id="plugin-list" class="plugin-list">
            Loading plugins...
        </div>
    </div>
    
    <script>
        async function loadPlugins() {
            try {
                const response = await fetch('/api/plugins');
                const plugins = await response.json();
                
                const container = document.getElementById('plugin-list');
                container.innerHTML = '';
                
                if (plugins.length === 0) {
                    container.innerHTML = '<p>No plugins loaded. Check the plugins directory and reload.</p>';
                    return;
                }
                
                plugins.forEach(plugin => {
                    const item = document.createElement('div');
                    item.className = 'plugin-item';
                    
                    const statusClass = plugin.enabled ? 'status-enabled' : 'status-disabled';
                    const statusText = plugin.enabled ? 'Enabled' : 'Disabled';
                    
                    item.innerHTML = 
                        '<div class="plugin-name">' + plugin.name + '</div>' +
                        '<div class="plugin-info">Version: ' + plugin.version + ' | Author: ' + plugin.author + '</div>' +
                        '<div class="plugin-info">' + plugin.description + '</div>' +
                        '<div class="plugin-info">' +
                            '<span class="plugin-status ' + statusClass + '">' + statusText + '</span>' +
                        '</div>' +
                        '<div class="plugin-actions">' +
                            '<button class="btn btn-primary" onclick="togglePlugin(\'' + plugin.name + '\', ' + (!plugin.enabled) + ')">' +
                                (plugin.enabled ? 'Disable' : 'Enable') +
                            '</button>' +
                            (plugin.has_config_ui ? '<button class="btn btn-primary" onclick="configurePlugin(\'' + plugin.name + '\')">Configure</button>' : '') +
                        '</div>';
                    container.appendChild(item);
                });
            } catch (err) {
                document.getElementById('plugin-list').innerHTML = '<p>Error loading plugins: ' + err.message + '</p>';
            }
        }
        
        async function reloadPlugins() {
            try {
                const response = await fetch('/api/plugins/reload', { method: 'POST' });
                const result = await response.json();
                alert(result.message);
                loadPlugins();
            } catch (err) {
                alert('Failed to reload plugins: ' + err.message);
            }
        }
        
        async function togglePlugin(name, enable) {
            try {
                const action = enable ? 'enable' : 'disable';
                const response = await fetch('/api/plugins/' + name + '/' + action, { method: 'POST' });
                const result = await response.json();
                loadPlugins();
            } catch (err) {
                alert('Failed to toggle plugin: ' + err.message);
            }
        }
        
        function configurePlugin(name) {
            alert('Plugin configuration UI would open here for: ' + name);
        }
        
        loadPlugins();
    </script>
</body>
</html>
    )HTML";
}

} // namespace crossexpand
