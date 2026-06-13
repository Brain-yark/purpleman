#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>

class CLIManager {
public:
    struct Handlers {
        std::function<void()> listImplants;
        std::function<void(const std::string&)> implantInfo;
        std::function<void(const std::string&)> interactShell;
        std::function<void()> checkUSBStatus;
        std::function<void()> checkUSBResults;
        std::function<void()> showStatistics;
        std::function<void(const std::string&, const std::string&)> setConfig;
        std::function<void()> saveConfig;
        std::function<void(const std::string&, const std::string&, const std::string&)> executeCommand;
        std::function<void()> shutdown;
    };

    explicit CLIManager(Handlers handlers = {});
    bool Run(std::istream& in = std::cin, std::ostream& out = std::cout);
    bool ProcessInput(const std::string& input, std::ostream& out = std::cout);

private:
    void PrintHelp(std::ostream& out) const;
    static std::vector<std::string> Tokenize(const std::string& input);

    Handlers handlers_;
    std::atomic<bool> running_;
};
