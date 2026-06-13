// Implant session definition extracted from c2_contoller.cpp
#pragma once

#include <string>
#include <queue>
#include <chrono>
#include <winsock2.h>

struct ImplantSession {
    std::string implantId;
    std::string hostname;
    std::string username;
    std::string ipAddress;
    std::string osVersion;
    std::string connectionType;  // "TCP", "HTTPS", "DNS", "USB", "AUDIO"
    SOCKET socket;
    std::chrono::system_clock::time_point lastSeen;
    std::chrono::system_clock::time_point connectedAt;
    bool isOnline;
    int sleepTime;
    int jitter;
    std::queue<std::string> pendingCommands;
    std::queue<std::string> commandResults;
};
