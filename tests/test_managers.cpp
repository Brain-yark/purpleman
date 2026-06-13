#include <winsock2.h>
#include <windows.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "implant/manager.h"
#include "network/manager.h"
#include "usb/manager.h"

static void Assert(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    ImplantManager manager;
    auto session = std::make_shared<ImplantSession>();
    session->implantId = "unit-test-1";
    session->hostname = "localhost";
    session->username = "tester";
    session->ipAddress = "127.0.0.1";
    session->connectionType = "USB";
    session->isOnline = false;
    session->sleepTime = 10;
    session->jitter = 5;

    Assert(manager.Add(session), "ImplantManager should add a session");
    Assert(manager.Size() == 1, "ImplantManager size should be 1 after add");
    Assert(manager.Get("unit-test-1") != nullptr, "ImplantManager should retrieve added session");
    Assert(manager.FindByPrefix("unit")->implantId == "unit-test-1", "ImplantManager should find by prefix");
    Assert(manager.Remove("unit-test-1"), "ImplantManager should remove session");
    Assert(manager.Size() == 0, "ImplantManager size should be 0 after remove");

    std::string plain = "HelloUnitTest";
    std::string key = "OfflineC2Key";
    std::string encrypted = USBManager::XOREncrypt(plain, key);
    std::string decrypted = USBManager::XOREncrypt(encrypted, key);
    Assert(decrypted == plain, "USBManager XOREncrypt should round-trip correctly");

    USBManager usb;
    usb.SetUSBDriveLetter("E:");
    Assert(usb.GetUSBDriveLetter() == "E:", "USBManager should preserve the set drive letter");
    Assert(usb.HasAuthorizedUSB(), "USBManager should have authorized USB after setting drive letter");

    auto driveInfo = usb.GetDriveInfo();
    Assert(driveInfo.authorized, "USBManager GetDriveInfo should report authorized drive");
    Assert(driveInfo.driveLetter == "E:", "USBManager GetDriveInfo should preserve drive letter");

    auto results = usb.PollResults();
    Assert(results.empty(), "USBManager PollResults should return empty for a drive with no data files");

    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    Assert(wsaResult == 0, "WSAStartup should succeed for network tests");

    NetworkManager network;
    SOCKET tcpSocket = network.CreateTCPSocket();
    Assert(tcpSocket != INVALID_SOCKET, "NetworkManager should create a valid TCP socket");
    closesocket(tcpSocket);

    SOCKET udpSocket = network.CreateUDPSocket(0);
    Assert(udpSocket != INVALID_SOCKET, "NetworkManager should create a valid UDP socket");
    closesocket(udpSocket);

    WSACleanup();

    std::cout << "All unit tests passed." << std::endl;
    return EXIT_SUCCESS;
}
