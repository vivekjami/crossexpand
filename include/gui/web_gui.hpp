#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>

namespace crossexpand {

// HTTP request structure
struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> parameters;
};

// HTTP response structure
struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    
    void set_content_type(const std::string& type) {
        headers["Content-Type"] = type;
    }
    
    void set_json_content(const std::string& json) {
        set_content_type("application/json");
        body = json;
    }
    
    void set_html_content(const std::string& html) {
        set_content_type("text/html; charset=utf-8");
        body = html;
    }
    
    void set_error(int code, const std::string& message) {
        status_code = code;
        status_text = message;
        set_json_content("{\"error\": \"" + message + "\"}");
    }
};

// HTTP request handler function type
using HttpHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

// Simple HTTP server for the web GUI
class WebServer {
private:
    std::atomic<bool> running_;
    std::thread server_thread_;
    int port_;
    int server_socket_;
    std::unordered_map<std::string, HttpHandler> handlers_;
    std::string static_files_directory_;

public:
    WebServer(int port = 8080);
    ~WebServer();
    
    // Server lifecycle
    bool start();
    void stop();
    bool is_running() const;
    
    // Route registration
    void register_handler(const std::string& path, HttpHandler handler);
    void register_static_directory(const std::string& directory);
    
    // Built-in handlers
    void setup_default_routes();
    
    // Utility functions
    std::string get_server_url() const;
    
private:
    void server_loop();
    void handle_client(int client_socket);
    HttpRequest parse_request(const std::string& request_data);
    std::string build_response(const HttpResponse& response);
    void handle_request(const HttpRequest& request, HttpResponse& response);
    std::string url_decode(const std::string& encoded);
    std::unordered_map<std::string, std::string> parse_query_string(const std::string& query);
    std::string get_mime_type(const std::string& filepath);
    bool serve_static_file(const std::string& filepath, HttpResponse& response);
};

// Web-based GUI manager
class WebGUI {
private:
    std::unique_ptr<WebServer> server_;
    std::string templates_directory_;
    bool auto_open_browser_;

public:
    WebGUI(int port = 8080);
    ~WebGUI();
    
    // GUI lifecycle
    bool initialize();
    void shutdown();
    bool is_running() const;
    
    // Configuration
    void set_auto_open_browser(bool auto_open);
    void set_templates_directory(const std::string& directory);
    
    // Web interface management
    std::string get_gui_url() const;
    bool open_in_browser() const;
    
private:
    void setup_api_routes();
    void setup_static_routes();
    
    // API handlers
    void handle_api_status(const HttpRequest& request, HttpResponse& response);
    void handle_api_templates(const HttpRequest& request, HttpResponse& response);
    void handle_api_plugins(const HttpRequest& request, HttpResponse& response);
    void handle_api_performance(const HttpRequest& request, HttpResponse& response);
    void handle_api_config(const HttpRequest& request, HttpResponse& response);
    
    // Template expansion API
    void handle_api_expand(const HttpRequest& request, HttpResponse& response);
    void handle_api_preview(const HttpRequest& request, HttpResponse& response);
    
    // Plugin management API
    void handle_api_plugin_list(const HttpRequest& request, HttpResponse& response);
    void handle_api_plugin_enable(const HttpRequest& request, HttpResponse& response);
    void handle_api_plugin_config(const HttpRequest& request, HttpResponse& response);
    
    // Main page handler
    void handle_main_page(const HttpRequest& request, HttpResponse& response);
    
    // Utility functions
    std::string load_template(const std::string& template_name);
    std::string generate_main_html();
    std::string generate_dashboard_html();
    std::string generate_plugin_management_html();
};

} // namespace crossexpand
