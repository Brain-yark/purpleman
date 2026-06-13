#pragma once

#include <string>
#include <winsock2.h>
#include <cstdint>

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    bool ConnectTCP(const std::string& server, uint16_t port, uint32_t timeoutMs);
    bool Send(const std::string& data);
    int Receive(char* buffer, int bufferSize);
    int Receive(char* buffer, int bufferSize, int timeoutMs);  // ← ADD THIS
    void Close();

    SOCKET Socket() const;
    bool IsConnected() const;

private:
    SOCKET socket_;
    bool SetTimeout(uint32_t timeoutMs);
};