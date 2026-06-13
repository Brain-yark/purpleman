#include "log/logger.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace logger {

static std::mutex loggerMutex;
static std::ofstream loggerFile;
static bool loggerEnabled = false;
static bool consoleEnabled = false;

static const char* LevelName(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warning: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "UNKNOWN";
}

bool Initialize(const std::string& filename, bool consoleOutput) {
    std::lock_guard<std::mutex> lock(loggerMutex);

    try {
        loggerFile.open(filename, std::ios::app | std::ios::out);
    } catch (const std::exception& ex) {
        std::cerr << "[!] Logger initialize exception: " << ex.what() << std::endl;
        loggerFile.clear();
        return false;
    } catch (...) {
        std::cerr << "[!] Logger initialize unknown exception" << std::endl;
        loggerFile.clear();
        return false;
    }

    if (!loggerFile) {
        return false;
    }

    loggerEnabled = true;
    consoleEnabled = consoleOutput;
    Log(Level::Info, "Logger initialized");
    return true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(loggerMutex);
    if (!loggerEnabled) return;
    Log(Level::Info, "Logger shutting down");
    loggerFile.close();
    loggerEnabled = false;
}

void Log(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(loggerMutex);
    if (!loggerEnabled) return;

    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;

    std::ostringstream line;
    line << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S")
         << "." << std::setw(3) << std::setfill('0') << ms
         << " [" << LevelName(level) << "] " << message;

    loggerFile << line.str() << std::endl;
    loggerFile.flush();

    if (consoleEnabled) {
        std::cout << line.str() << std::endl;
    }
}

void Debug(const std::string& message) {
    Log(Level::Debug, message);
}

void Info(const std::string& message) {
    Log(Level::Info, message);
}

void Warning(const std::string& message) {
    Log(Level::Warning, message);
}

void Error(const std::string& message) {
    Log(Level::Error, message);
}

} // namespace logger
