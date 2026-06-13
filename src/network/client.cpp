#include "network/client.h"
#include "log/logger.h"
#include <ws2tcpip.h>
#include <string>

NetworkClient::NetworkClient() : socket_(INVALID_SOCKET) {}

NetworkClient::~NetworkClient() {
    Close();
}

bool NetworkClient::ConnectTCP(const std::string& server, uint16_t port, uint32_t timeoutMs) {
    Close();

    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET) {
        logger::Error("NetworkClient failed to create TCP socket");
        return false;
    }

    if (!SetTimeout(timeoutMs)) {
        logger::Warning("NetworkClient failed to configure socket timeout");
    }

    addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    int resolveResult = getaddrinfo(server.c_str(), std::to_string(port).c_str(), &hints, &result);
    if (resolveResult != 0 || result == nullptr) {
        logger::Error("NetworkClient failed to resolve server: " + server);
        Close();
        return false;
    }

    int connectResult = connect(socket_, result->ai_addr, static_cast<int>(result->ai_addrlen));
    freeaddrinfo(result);

    if (connectResult == SOCKET_ERROR) {
        logger::Warning("NetworkClient failed to connect to " + server + ":" + std::to_string(port));
        Close();
        return false;
    }

    logger::Info("NetworkClient connected to " + server + ":" + std::to_string(port));
    return true;
}

bool NetworkClient::Send(const std::string& data) {
    if (socket_ == INVALID_SOCKET) return false;
    int sent = static_cast<int>(send(socket_, data.c_str(), static_cast<int>(data.size()), 0));
    if (sent != static_cast<int>(data.size())) {
        logger::Warning("NetworkClient failed to send full payload");
        return false;
    }
    return true;
}

int NetworkClient::Receive(char* buffer, int bufferSize) {
    if (socket_ == INVALID_SOCKET) return SOCKET_ERROR;
    return recv(socket_, buffer, bufferSize, 0);
}

// ADD THIS NEW METHOD:
int NetworkClient::Receive(char* buffer, int bufferSize, int timeoutMs) {
    if (socket_ == INVALID_SOCKET) return -2;
    
    // Wait for data with timeout using select()
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socket_, &readSet);
    
    timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    
    int selectResult = select(0, &readSet, NULL, NULL, &timeout);
    
    if (selectResult > 0) {
        // Data available, receive it
        return recv(socket_, buffer, bufferSize, 0);
    }
    else if (selectResult == 0) {
        // Timeout - no data available
        return -1;
    }
    else {
        // Error
        return -2;
    }
}

void NetworkClient::Close() {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
}

SOCKET NetworkClient::Socket() const {
    return socket_;
}

bool NetworkClient::IsConnected() const {
    return socket_ != INVALID_SOCKET;
}

bool NetworkClient::SetTimeout(uint32_t timeoutMs) {
    if (socket_ == INVALID_SOCKET) return false;
    DWORD timeout = static_cast<DWORD>(timeoutMs);
    int result = setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    result |= setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    return result == 0;
}