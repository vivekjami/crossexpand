#include "gui/web_gui.hpp"
#include "utils/logger.hpp"
#include "utils/performance_monitor.hpp"
#include "core/plugin_system.hpp"
#include "core/advanced_template_engine.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace crossexpand {

WebServer::WebServer(int port) : running_(false), port_(port), server_socket_(-1) {
    static_files_directory_ = "./web";
}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start() {
    if (running_.load()) {
        return true;
    }
    
    // Create socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        LOG_ERROR("Failed to create server socket");
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_WARNING("Failed to set socket options");
    }
    
    // Bind socket
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);
    
    if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to bind socket to port {}", port_);
        close(server_socket_);
        return false;
    }
    
    // Listen for connections
    if (listen(server_socket_, 10) < 0) {
        LOG_ERROR("Failed to listen on socket");
        close(server_socket_);
        return false;
    }
    
    running_.store(true);
    server_thread_ = std::thread(&WebServer::server_loop, this);
    
    LOG_INFO("Web server started on port {}", port_);
    return true;
}

void WebServer::stop() {
    if (running_.load()) {
        running_.store(false);
        
        if (server_socket_ >= 0) {
            close(server_socket_);
            server_socket_ = -1;
        }
        
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        
        LOG_INFO("Web server stopped");
    }
}

bool WebServer::is_running() const {
    return running_.load();
}

void WebServer::register_handler(const std::string& path, HttpHandler handler) {
    handlers_[path] = handler;
}

void WebServer::register_static_directory(const std::string& directory) {
    static_files_directory_ = directory;
}

void WebServer::setup_default_routes() {
    // Default route for root - Simple dashboard
    register_handler("/", [this](const HttpRequest& req, HttpResponse& res) {
        std::string html = R"HTML(<!DOCTYPE html>
<html>
<head>
    <title>CrossExpand Dashboard</title>
    <meta charset="utf-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1000px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }
        .header { border-bottom: 2px solid #007acc; padding-bottom: 10px; margin-bottom: 20px; }
        .nav a { margin-right: 20px; text-decoration: none; color: #007acc; }
        .status { background: #e8f5e8; padding: 10px; border-radius: 4px; margin: 20px 0; }
        .btn { padding: 8px 16px; margin: 5px; background: #007acc; color: white; border: none; border-radius: 4px; cursor: pointer; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>CrossExpand - Text Expansion Engine</h1>
            <p>Day 3 Complete Implementation</p>
        </div>
        
        <div class="nav">
            <a href="/">Dashboard</a>
            <a href="/templates">Templates</a>
            <a href="/plugins">Plugins</a>
            <a href="/api/status">API Status</a>
        </div>
        
        <div class="status">
            <strong>System Status:</strong> Running ✅
        </div>
        
        <div>
            <h2>Quick Actions</h2>
            <button class="btn" onclick="testAPI()">Test API</button>
            <button class="btn" onclick="reloadPlugins()">Reload Plugins</button>
            <button class="btn" onclick="showStatus()">Show Status</button>
        </div>
        
        <div id="output" style="margin-top: 20px; padding: 10px; background: #f8f9fa; border-radius: 4px; display: none;">
            <h3>Output</h3>
            <pre id="output-content"></pre>
        </div>
    </div>
    
    <script>
        function testAPI() {
            fetch('/api/expand', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    template: 'Hello {name}!',
                    context: { name: 'World' }
                })
            })
            .then(r => r.json())
            .then(data => showOutput(JSON.stringify(data, null, 2)))
            .catch(err => showOutput('Error: ' + err));
        }
        
        function reloadPlugins() {
            fetch('/api/plugins/reload', { method: 'POST' })
            .then(r => r.json())
            .then(data => showOutput('Plugins reloaded: ' + data.message))
            .catch(err => showOutput('Error: ' + err));
        }
        
        function showStatus() {
            fetch('/api/status')
            .then(r => r.json())
            .then(data => showOutput(JSON.stringify(data, null, 2)))
            .catch(err => showOutput('Error: ' + err));
        }
        
        function showOutput(text) {
            document.getElementById('output-content').textContent = text;
            document.getElementById('output').style.display = 'block';
        }
    </script>
</body>
</html>)HTML";
        res.set_html_content(html);
    });
}

std::string WebServer::get_server_url() const {
    return "http://localhost:" + std::to_string(port_);
}

void WebServer::server_loop() {
    while (running_.load()) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (running_.load()) {
                LOG_WARNING("Failed to accept client connection");
            }
            continue;
        }
        
        // Handle client in separate thread
        std::thread([this, client_socket]() {
            handle_client(client_socket);
        }).detach();
    }
}

void WebServer::handle_client(int client_socket) {
    char buffer[4096];
    ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        try {
            HttpRequest request = parse_request(std::string(buffer));
            HttpResponse response;
            
            handle_request(request, response);
            
            std::string response_str = build_response(response);
            send(client_socket, response_str.c_str(), response_str.length(), 0);
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling HTTP request: {}", e.what());
        }
    }
    
    close(client_socket);
}

HttpRequest WebServer::parse_request(const std::string& request_data) {
    HttpRequest request;
    std::istringstream stream(request_data);
    std::string line;
    
    // Parse request line
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        std::string path_and_query;
        line_stream >> request.method >> path_and_query;
        
        // Split path and query string
        size_t query_pos = path_and_query.find('?');
        if (query_pos != std::string::npos) {
            request.path = path_and_query.substr(0, query_pos);
            request.query_string = path_and_query.substr(query_pos + 1);
            request.parameters = parse_query_string(request.query_string);
        } else {
            request.path = path_and_query;
        }
        
        request.path = url_decode(request.path);
    }
    
    // Parse headers
    while (std::getline(stream, line) && !line.empty() && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t\r") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            request.headers[key] = value;
        }
    }
    
    // Parse body (for POST requests)
    if (request.method == "POST") {
        std::string body;
        while (std::getline(stream, line)) {
            body += line + "\n";
        }
        request.body = body;
    }
    
    return request;
}

std::string WebServer::build_response(const HttpResponse& response) {
    std::ostringstream stream;
    
    // Status line
    stream << "HTTP/1.1 " << response.status_code << " " << response.status_text << "\r\n";
    
    // Headers
    for (const auto& [key, value] : response.headers) {
        stream << key << ": " << value << "\r\n";
    }
    
    // Content-Length
    stream << "Content-Length: " << response.body.length() << "\r\n";
    stream << "Connection: close\r\n";
    stream << "\r\n";
    
    // Body
    stream << response.body;
    
    return stream.str();
}

void WebServer::handle_request(const HttpRequest& request, HttpResponse& response) {
    // Check for registered handlers first
    auto handler_it = handlers_.find(request.path);
    if (handler_it != handlers_.end()) {
        handler_it->second(request, response);
        return;
    }
    
    // Try to serve static files
    if (serve_static_file(request.path, response)) {
        return;
    }
    
    // Default 404 response
    response.set_error(404, "Not Found");
}

std::string WebServer::url_decode(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            int value;
            std::istringstream hex_stream(encoded.substr(i + 1, 2));
            if (hex_stream >> std::hex >> value) {
                decoded += static_cast<char>(value);
                i += 2;
            } else {
                decoded += encoded[i];
            }
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}

std::unordered_map<std::string, std::string> WebServer::parse_query_string(const std::string& query) {
    std::unordered_map<std::string, std::string> params;
    std::istringstream stream(query);
    std::string pair;
    
    while (std::getline(stream, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = url_decode(pair.substr(0, eq_pos));
            std::string value = url_decode(pair.substr(eq_pos + 1));
            params[key] = value;
        }
    }
    
    return params;
}

std::string WebServer::get_mime_type(const std::string& filepath) {
    // C++17 compatible version - use suffix check instead of ends_with
    auto has_suffix = [](const std::string& str, const std::string& suffix) {
        return str.length() >= suffix.length() && 
               str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    };
    
    if (has_suffix(filepath, ".html") || has_suffix(filepath, ".htm")) return "text/html";
    if (has_suffix(filepath, ".css")) return "text/css";
    if (has_suffix(filepath, ".js")) return "application/javascript";
    if (has_suffix(filepath, ".json")) return "application/json";
    if (has_suffix(filepath, ".png")) return "image/png";
    if (has_suffix(filepath, ".jpg") || has_suffix(filepath, ".jpeg")) return "image/jpeg";
    if (has_suffix(filepath, ".gif")) return "image/gif";
    if (has_suffix(filepath, ".svg")) return "image/svg+xml";
    return "text/plain";
}

bool WebServer::serve_static_file(const std::string& filepath, HttpResponse& response) {
    std::string full_path = static_files_directory_ + filepath;
    
    // Security check - prevent directory traversal
    if (filepath.find("..") != std::string::npos) {
        response.set_error(403, "Forbidden");
        return true;
    }
    
    if (std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path)) {
        std::ifstream file(full_path, std::ios::binary);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            response.headers["Content-Type"] = get_mime_type(filepath);
            response.body = content;
            return true;
        }
    }
    
    return false;
}

} // namespace crossexpand
