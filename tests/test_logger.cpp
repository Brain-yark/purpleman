#include <iostream>
#include <string>

#include "log/logger.h"
#include "utils/helpers.h"

static void Assert(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    const std::string logFile = "logger_test.log";

    Assert(logger::Initialize(logFile, false), "Logger should initialize");
    logger::Info("Logger unit test message");
    logger::Shutdown();

    std::string contents;
    Assert(utils::ReadTextFile(logFile, contents), "Logger should write file");
    Assert(contents.find("Logger unit test message") != std::string::npos,
           "Logger should contain test message");

    std::cout << "All logger tests passed." << std::endl;
    return EXIT_SUCCESS;
}
