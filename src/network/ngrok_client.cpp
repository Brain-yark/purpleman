#include "network/ngrok_client.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <log/logger.h>

size_t NgrokClient::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

NgrokClient::NgrokClient(const std::string& token, const std::string& addr, uint16_t port)
    : authToken(token), localAddress(addr), localPort(port), apiUrl("http://127.0.0.1:4040/api"),
      tunnelName("purpleman-c2"), curlHandle(nullptr) {
    curlHandle = curl_easy_init();
}

NgrokClient::~NgrokClient() {
    if (curlHandle) {
        curl_easy_cleanup(curlHandle);
    }
}

bool NgrokClient::Initialize() {
    if (!curlHandle) {
        logger::Error("CURL handle not initialized");
        return false;
    }

    // Test connection to ngrok API
    std::string response;
    curl_easy_setopt(curlHandle, CURLOPT_URL, (apiUrl + "/tunnels").c_str());
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curlHandle);
    
    if (res == CURLE_OK) {
        logger::Info("Ngrok API connection successful");
        return true;
    } else {
        logger::Error("Ngrok API connection failed: " + std::string(curl_easy_strerror(res)));
        logger::Info("Make sure ngrok is running: ngrok tcp 8443");
        return false;
    }
}

bool NgrokClient::CreateTCPTunnel(const std::string& name) {
    if (!curlHandle) return false;

    std::string endpoint = "tcp://" + localAddress + ":" + std::to_string(localPort);
    
    json payload = {
        {"addr", localPort},
        {"proto", "tcp"},
        {"name", name}
    };

    std::string jsonStr = payload.dump();
    std::string response;

    curl_easy_setopt(curlHandle, CURLOPT_URL, (apiUrl + "/tunnels").c_str());
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curlHandle);
    curl_slist_free_all(headers);

    if (res == CURLE_OK) {
        try {
            auto responseJson = json::parse(response);
            if (responseJson.contains("public_url")) {
                logger::Info("TCP Tunnel created: " + responseJson["public_url"].get<std::string>());
                tunnelName = name;
                return true;
            }
        } catch (const std::exception& e) {
            logger::Error("Failed to parse ngrok response: " + std::string(e.what()));
        }
    } else {
        logger::Error("Tunnel creation failed: " + std::string(curl_easy_strerror(res)));
    }

    return false;
}

bool NgrokClient::CreateHTTPSTunnel(const std::string& name) {
    if (!curlHandle) return false;

    json payload = {
        {"addr", localPort},
        {"proto", "tls"},
        {"name", name}
    };

    std::string jsonStr = payload.dump();
    std::string response;

    curl_easy_setopt(curlHandle, CURLOPT_URL, (apiUrl + "/tunnels").c_str());
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &response);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curlHandle);
    curl_slist_free_all(headers);

    if (res == CURLE_OK) {
        try {
            auto responseJson = json::parse(response);
            if (responseJson.contains("public_url")) {
                logger::Info("HTTPS Tunnel created: " + responseJson["public_url"].get<std::string>());
                tunnelName = name;
                return true;
            }
        } catch (const std::exception& e) {
            logger::Error("Failed to parse ngrok response: " + std::string(e.what()));
        }
    }

    return false;
}

json NgrokClient::GetTunnelInfo(const std::string& name) {
    std::string response;

    curl_easy_setopt(curlHandle, CURLOPT_URL, (apiUrl + "/tunnels/" + name).c_str());
    curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curlHandle);

    if (res == CURLE_OK) {
        try {
            return json::parse(response);
        } catch (...) {
            return json();
        }
    }

    return json();
}

json NgrokClient::ListTunnels() {
    std::string response;

    curl_easy_setopt(curlHandle, CURLOPT_URL, (apiUrl + "/tunnels").c_str());
    curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curlHandle);

    if (res == CURLE_OK) {
        try {
            return json::parse(response);
        } catch (...) {
            return json();
        }
    }

    return json();
}

std::string NgrokClient::GetPublicURL(const std::string& name) {
    auto info = GetTunnelInfo(name);
    
    if (info.contains("public_url")) {
        return info["public_url"].get<std::string>();
    }

    return "";
}

bool NgrokClient::GetPublicEndpoint(std::string& host, uint16_t& port) {
    auto info = GetTunnelInfo(tunnelName);
    
    if (!info.contains("public_url")) {
        logger::Error("Tunnel not found or not active");
        return false;
    }

    std::string publicUrl = info["public_url"].get<std::string>();
    
    // Parse URL format: tcp://hostname:port or tls://hostname:port
    size_t protocolEnd = publicUrl.find("://");
    if (protocolEnd == std::string::npos) return false;

    size_t hostStart = protocolEnd + 3;
    size_t portStart = publicUrl.find_last_of(":");
    
    if (portStart == std::string::npos || portStart <= hostStart) return false;

    host = publicUrl.substr(hostStart, portStart - hostStart);
    
    try {
        port = std::stoi(publicUrl.substr(portStart + 1));
        return true;
    } catch (...) {
        return false;
    }
}

bool NgrokClient::StopTunnel(const std::string& name) {
    curl_easy_setopt(curlHandle, CURLOPT_URL, (apiUrl + "/tunnels/" + name).c_str());
    curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

    CURLcode res = curl_easy_perform(curlHandle);
    
    if (res == CURLE_OK) {
        logger::Info("Tunnel stopped: " + name);
        return true;
    }

    return false;
}

bool NgrokClient::StopAllTunnels() {
    auto tunnels = ListTunnels();
    
    if (!tunnels.contains("tunnels")) return false;

    bool allStopped = true;
    for (auto& tunnel : tunnels["tunnels"]) {
        if (tunnel.contains("name")) {
            if (!StopTunnel(tunnel["name"].get<std::string>())) {
                allStopped = false;
            }
        }
    }

    return allStopped;
}

bool NgrokClient::IsConnected(const std::string& name) {
    auto info = GetTunnelInfo(name);
    
    if (info.contains("status")) {
        std::string status = info["status"].get<std::string>();
        return status == "online";
    }

    return false;
}

std::string NgrokClient::GetStatus(const std::string& name) {
    auto info = GetTunnelInfo(name);
    
    if (info.contains("status")) {
        return info["status"].get<std::string>();
    }

    return "unknown";
}

