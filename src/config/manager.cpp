#include "config/manager.h"
#include "utils/helpers.h"

ConfigManager::ConfigManager()
    : bindAddress("0.0.0.0"), bindPort(443), httpsEnabled(true),
      usbAutoDetect(true), useDomainFronting(false), frontingDomain() {
}

bool ConfigManager::SetConfigKey(const std::string& key, const std::string& value) {
    if (key == "usb" || key == "usb_auto") {
        usbAutoDetect = (value == "1" || value == "true" || value == "yes");
        return true;
    }
    if (key == "fronting") {
        frontingDomain = value;
        useDomainFronting = true;
        return true;
    }
    if (key == "bind_address") {
        bindAddress = value;
        return true;
    }
    if (key == "port") {
        try {
            int portValue = std::stoi(value);
            if (portValue < 0 || portValue > 65535) return false;
            bindPort = static_cast<uint16_t>(portValue);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

bool ConfigManager::Save(const std::string& path) const {
    return utils::WriteTextFile(path, ToJSON());
}

bool ConfigManager::Load(const std::string& path) {
    std::string contents;
    if (!utils::ReadTextFile(path, contents)) {
        return false;
    }
    auto value = utils::ExtractJSONValue(contents, "bind_address");
    if (!value.empty()) bindAddress = value;
    value = utils::ExtractJSONValue(contents, "fronting");
    if (!value.empty()) {
        frontingDomain = value;
        useDomainFronting = true;
    }
    httpsEnabled = utils::ParseJSONBool(contents, "https", httpsEnabled);
    usbAutoDetect = utils::ParseJSONBool(contents, "usb_auto", usbAutoDetect);
    bindPort = static_cast<uint16_t>(utils::ParseJSONInt(contents, "port", bindPort));
    return true;
}

std::string ConfigManager::ToJSON() const {
    std::string json;
    json += "{\n";
    json += "  \"bind_address\": \"" + bindAddress + "\",\n";
    json += "  \"port\": " + std::to_string(bindPort) + ",\n";
    json += "  \"https\": " + std::string(httpsEnabled ? "true" : "false") + ",\n";
    json += "  \"fronting\": \"" + frontingDomain + "\",\n";
    json += "  \"usb_auto\": " + std::string(usbAutoDetect ? "true" : "false") + "\n";
    json += "}\n";
    return json;
}
