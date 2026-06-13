#include <iostream>
#include <string>

#include "config/manager.h"

static void Assert(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    ConfigManager config;
    config.bindAddress = "192.168.1.1";
    config.bindPort = 8443;
    config.httpsEnabled = false;
    config.usbAutoDetect = false;
    config.useDomainFronting = true;
    config.frontingDomain = "front.example.com";

    Assert(config.Save("config_test.json"), "ConfigManager should save file");

    ConfigManager loaded;
    Assert(loaded.Load("config_test.json"), "ConfigManager should load saved file");
    Assert(loaded.bindAddress == "192.168.1.1", "Loaded bind address should match");
    Assert(loaded.bindPort == 8443, "Loaded port should match");
    Assert(loaded.httpsEnabled == false, "Loaded https setting should match");
    Assert(loaded.usbAutoDetect == false, "Loaded usb_auto setting should match");
    Assert(loaded.frontingDomain == "front.example.com", "Loaded fronting domain should match");
    Assert(loaded.useDomainFronting == true, "Loaded domain fronting flag should match");

    Assert(config.SetConfigKey("port", "443"), "ConfigManager should accept port key");
    Assert(config.bindPort == 443, "ConfigManager should update port");
    Assert(config.SetConfigKey("usb_auto", "true"), "ConfigManager should accept usb_auto key");
    Assert(config.usbAutoDetect == true, "ConfigManager should update usb_auto flag");
    Assert(config.SetConfigKey("bind_address", "0.0.0.0"), "ConfigManager should accept bind_address key");

    std::cout << "All config tests passed." << std::endl;
    return EXIT_SUCCESS;
}
