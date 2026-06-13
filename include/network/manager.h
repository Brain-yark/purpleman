#pragma once

#include <string>
#include <winsock2.h>
#include <thread>
#include <functional>
#include <atomic>
#include <vector>

class NetworkManager {
public:
    using AcceptCallback = std::function<void(SOCKET, sockaddr_in)>;
    using UDPCallback = std::function<void(const char*, int, const sockaddr_in&)>;

    NetworkManager();
    ~NetworkManager();

    SOCKET CreateTCPSocket();
    bool BindAndListen(SOCKET sock, const std::string& bindAddress, uint16_t port);
    SOCKET CreateUDPSocket(uint16_t port);

    // Start accept loop on existing listener socket. The callback is invoked
    // on each accepted connection.
    bool StartAcceptLoop(SOCKET listener, AcceptCallback cb);

    // Start UDP monitoring loop on the provided socket; callback on packet.
    bool StartUDPMonitor(SOCKET udpSocket, UDPCallback cb);

    // Stop any background network threads
    void StopAll();

private:
    std::atomic<bool> running_;
    std::vector<std::thread> threads_;
};
