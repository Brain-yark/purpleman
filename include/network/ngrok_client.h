#pragma once

#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class NgrokClient {
private:
    std::string authToken;
    std::string apiUrl;
    std::string localAddress;
    uint16_t localPort;
    std::string tunnelName;
    CURL* curlHandle;
    
    // Response callback for CURL
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    
public:
    NgrokClient(const std::string& token = "", const std::string& addr = "127.0.0.1", uint16_t port = 8443);
    ~NgrokClient();
    
    // Initialize ngrok connection
    bool Initialize();
    
    // Create TCP tunnel
    bool CreateTCPTunnel(const std::string& name = "purpleman-c2");
    
    // Create HTTPS tunnel
    bool CreateHTTPSTunnel(const std::string& name = "purpleman-https");
    
    // Get tunnel information
    json GetTunnelInfo(const std::string& name);
    
    // Get all active tunnels
    json ListTunnels();
    
    // Get public URL for tunnel
    std::string GetPublicURL(const std::string& name);
    
    // Get public host and port
    bool GetPublicEndpoint(std::string& host, uint16_t& port);
    
    // Stop tunnel
    bool StopTunnel(const std::string& name);
    
    // Stop all tunnels
    bool StopAllTunnels();
    
    // Check tunnel status
    bool IsConnected(const std::string& name);
    
    // Set auth token
    void SetAuthToken(const std::string& token) { authToken = token; }
    
    // Set local address and port
    void SetLocalEndpoint(const std::string& addr, uint16_t port) {
        localAddress = addr;
        localPort = port;
    }
    
    // Get tunnel status
    std::string GetStatus(const std::string& name);
};

