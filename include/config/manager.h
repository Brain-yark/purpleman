#pragma once

#include <cstdint>
#include <string>

class ConfigManager {
public:
    ConfigManager();

    std::string bindAddress;
    uint16_t bindPort;
    bool httpsEnabled;
    bool usbAutoDetect;
    bool useDomainFronting;
    std::string frontingDomain;
    bool enableNgrok;
    std::string ngrokAuthToken;
    std::string ngrokRegion;
    std::string ngrokBinaryPath;

    bool SetConfigKey(const std::string& key, const std::string& value);
    bool Save(const std::string& path) const;
    bool Load(const std::string& path);
    std::string ToJSON() const;
};
