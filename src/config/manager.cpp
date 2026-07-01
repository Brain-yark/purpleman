#include "config/manager.h"
#include "utils/helpers.h"

ConfigManager::ConfigManager()
    : bindAddress("0.0.0.0"), bindPort(443), httpsEnabled(true),
      usbAutoDetect(true), useDomainFronting(false), frontingDomain(),
      enableNgrok(false), ngrokAuthToken(), ngrokRegion("us"),
      ngrokBinaryPath("ngrok") {
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
    if (key == "ngrok") {
        enableNgrok = (value == "1" || value == "true" || value == "yes");
        return true;
    }
    if (key == "ngrok_auth_token") {
        ngrokAuthToken = value;
        return true;
    }
    if (key == "ngrok_region") {
        ngrokRegion = value;
        return true;
    }
    if (key == "ngrok_binary") {
        ngrokBinaryPath = value;
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
    value = utils::ExtractJSONValue(contents, "ngrok_auth_token");
    if (!value.empty()) ngrokAuthToken = value;
    value = utils::ExtractJSONValue(contents, "ngrok_region");
    if (!value.empty()) ngrokRegion = value;
    value = utils::ExtractJSONValue(contents, "ngrok_binary");
    if (!value.empty()) ngrokBinaryPath = value;
    httpsEnabled = utils::ParseJSONBool(contents, "https", httpsEnabled);
    usbAutoDetect = utils::ParseJSONBool(contents, "usb_auto", usbAutoDetect);
    enableNgrok = utils::ParseJSONBool(contents, "ngrok", enableNgrok);
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
    json += "  \"usb_auto\": " + std::string(usbAutoDetect ? "true" : "false") + ",\n";
    json += "  \"ngrok\": " + std::string(enableNgrok ? "true" : "false") + ",\n";
    json += "  \"ngrok_auth_token\": \"" + ngrokAuthToken + "\",\n";
    json += "  \"ngrok_region\": \"" + ngrokRegion + "\",\n";
    json += "  \"ngrok_binary\": \"" + ngrokBinaryPath + "\"\n";
    json += "}\n";
    return json;
}
