#include "https_server.h"
#include <esp_https_server.h>
#include "ssl_cert.h"
#include "cert_data.h" // Include the DER data
#include <algorithm> // For std::min

// Static member initialization
HTTPSServerWrapper::RequestCallback HTTPSServerWrapper::_requestHandler = nullptr;
static httpd_handle_t _server_handle = nullptr;

HTTPSServerWrapper::HTTPSServerWrapper(uint16_t port) : _port(port), _running(false), _server(nullptr), _cert(nullptr) {
}

HTTPSServerWrapper::~HTTPSServerWrapper() {
    stop();
}

// Native ESP-IDF Request Handler
static esp_err_t native_handler(httpd_req_t *req) {
    // Read body
    char buf[512];
    String body = "";
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, std::min((size_t)remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        body += String(buf).substring(0, ret);
        remaining -= ret;
    }

    // Extract method and path
    String method = (req->method == HTTP_GET) ? "GET" : 
                   (req->method == HTTP_POST) ? "POST" : 
                   (req->method == HTTP_PUT) ? "PUT" : "UNKNOWN";
    String path = String(req->uri);

    Serial.printf("[HTTPS] Request: %s %s\n", method.c_str(), path.c_str());

    // Call the C++ callback
    if (HTTPSServerWrapper::_requestHandler) {
        String response = HTTPSServerWrapper::_requestHandler(path, method, body);
        
        // Determine content type
        if (response.startsWith("<?xml")) {
            httpd_resp_set_type(req, "text/xml");
        } else {
            httpd_resp_set_type(req, "application/json");
        }
        
        // Send response
        httpd_resp_send(req, response.c_str(), response.length());
    } else {
        httpd_resp_send_404(req);
    }

    return ESP_OK;
}

bool HTTPSServerWrapper::begin(const char* cert, const char* key) {
    if (_running) {
        Serial.println("[HTTPS] Server already running");
        return false;
    }

    Serial.printf("[HTTPS] Starting Native HTTPS server on port %d...\n", _port);
    
    // Debug prints for DER
    Serial.printf("[HTTPS] Cert DER Len: %d\n", server_cert_der_len);
    Serial.printf("[HTTPS] Key DER Len: %d\n", server_key_der_len);

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    
    // Use DER data directly
    conf.cacert_pem = server_cert_der;
    conf.cacert_len = server_cert_der_len;
    conf.prvtkey_pem = server_key_der;
    conf.prvtkey_len = server_key_der_len;
    
    conf.port_secure = _port;
    conf.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;

    // Start the server
    esp_err_t ret = httpd_ssl_start(&_server_handle, &conf);
    if (ESP_OK != ret) {
        Serial.printf("[HTTPS] Error starting server: %d\n", ret);
        return false;
    }

    // Register URI handlers
    // We register a wildcard handler to catch everything
    httpd_uri_t uri_handler = {
        .uri       = "*",
        .method    = HTTP_GET,
        .handler   = native_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(_server_handle, &uri_handler);
    
    // Also register POST/PUT
    uri_handler.method = HTTP_POST;
    httpd_register_uri_handler(_server_handle, &uri_handler);
    
    uri_handler.method = HTTP_PUT;
    httpd_register_uri_handler(_server_handle, &uri_handler);

    _running = true;
    Serial.println("[HTTPS] Native HTTPS server started successfully");
    return true;
}

void HTTPSServerWrapper::stop() {
    if (_server_handle) {
        httpd_ssl_stop(_server_handle);
        _server_handle = nullptr;
    }
    _running = false;
    Serial.println("[HTTPS] HTTPS server stopped");
}

void HTTPSServerWrapper::handleClient() {
    // Native server runs in its own task, no need to loop
}

void HTTPSServerWrapper::setRequestHandler(RequestCallback callback) {
    _requestHandler = callback;
}

// Unused legacy method
void HTTPSServerWrapper::_handleRequest(void* req, void* res) {}