// NetworkManager implementation
#include "network/manager.h"
#include "log/logger.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <thread>
#include <cstring>

NetworkManager::NetworkManager() : running_(false) {}

NetworkManager::~NetworkManager() {
    StopAll();
}

SOCKET NetworkManager::CreateTCPSocket() {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    return s;
}

bool NetworkManager::BindAndListen(SOCKET sock, const std::string& bindAddress, uint16_t port) {
    if (sock == INVALID_SOCKET) return false;

    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (bindAddress == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, bindAddress.c_str(), &serverAddr.sin_addr);
    }

    if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        return false;
    }

    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        return false;
    }

    return true;
}

SOCKET NetworkManager::CreateUDPSocket(uint16_t port) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return s;

    sockaddr_in udpAddr = {0};
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_port = htons(port);
    udpAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (sockaddr*)&udpAddr, sizeof(udpAddr)) != 0) {
        // bind failed
        return INVALID_SOCKET;
    }

    return s;
}

bool NetworkManager::StartAcceptLoop(SOCKET listener, AcceptCallback cb) {
    if (listener == INVALID_SOCKET) return false;
    if (!cb) return false;

    running_ = true;
    threads_.emplace_back([listener, cb, this]() {
        while (running_) {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(listener, (sockaddr*)&clientAddr, &addrLen);
            if (clientSocket == INVALID_SOCKET) {
                if (running_) logger::Warning("NetworkManager accept failed");
                continue;
            }
            cb(clientSocket, clientAddr);
        }
    });

    return true;
}

bool NetworkManager::StartUDPMonitor(SOCKET udpSocket, UDPCallback cb) {
    if (udpSocket == INVALID_SOCKET) return false;
    if (!cb) return false;

    running_ = true;
    threads_.emplace_back([udpSocket, cb, this]() {
        char buffer[2048];
        while (running_) {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            int received = recvfrom(udpSocket, buffer, sizeof(buffer), 0,
                                   (sockaddr*)&clientAddr, &addrLen);
            if (received > 0) {
                cb(buffer, received, clientAddr);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    return true;
}

void NetworkManager::StopAll() {
    running_ = false;
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}
