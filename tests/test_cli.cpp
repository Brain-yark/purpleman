#include <iostream>
#include <sstream>
#include <string>

#include "cli/console.h"

static void Assert(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    bool listCalled = false;
    bool infoCalled = false;
    bool usbStatusCalled = false;
    bool usbResultsCalled = false;
    bool statsCalled = false;
    bool saveCalled = false;
    bool shutdownCalled = false;
    std::string execTarget;
    std::string execCommand;
    std::string execChannel;

    CLIManager::Handlers handlers;
    handlers.listImplants = [&]() { listCalled = true; };
    handlers.implantInfo = [&](const std::string& id) {
        infoCalled = (id == "ABC");
    };
    handlers.interactShell = [&](const std::string&) {};
    handlers.checkUSBStatus = [&]() { usbStatusCalled = true; };
    handlers.checkUSBResults = [&]() { usbResultsCalled = true; };
    handlers.showStatistics = [&]() { statsCalled = true; };
    handlers.setConfig = [&](const std::string&, const std::string&) {};
    handlers.saveConfig = [&]() { saveCalled = true; };
    handlers.executeCommand = [&](const std::string& target,
                                  const std::string& command,
                                  const std::string& channel) {
        execTarget = target;
        execCommand = command;
        execChannel = channel;
    };
    handlers.shutdown = [&]() { shutdownCalled = true; };

    CLIManager cli(handlers);
    std::ostringstream out;

    Assert(cli.ProcessInput("list", out), "list command should continue");
    Assert(listCalled, "list handler should be called");

    Assert(cli.ProcessInput("info ABC", out), "info command should continue");
    Assert(infoCalled, "info handler should be called with ID ABC");

    Assert(cli.ProcessInput("exec ABC whoami usb", out), "exec command should continue");
    Assert(execTarget == "ABC", "exec target should be ABC");
    Assert(execCommand == "whoami", "exec command should preserve argument string");
    Assert(execChannel == "usb", "exec channel should be usb");

    Assert(cli.ProcessInput("usb_status", out), "usb_status command should continue");
    Assert(usbStatusCalled, "usb status handler should be called");

    Assert(cli.ProcessInput("usb_results", out), "usb_results command should continue");
    Assert(usbResultsCalled, "usb results handler should be called");

    Assert(cli.ProcessInput("stats", out), "stats command should continue");
    Assert(statsCalled, "stats handler should be called");

    Assert(cli.ProcessInput("save", out), "save command should continue");
    Assert(saveCalled, "save handler should be called");

    Assert(!cli.ProcessInput("quit", out), "quit command should stop CLI loop");
    Assert(shutdownCalled, "shutdown handler should be called");

    std::cout << "All CLI tests passed." << std::endl;
    return 0;
}
