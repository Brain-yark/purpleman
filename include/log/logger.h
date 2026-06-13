#pragma once

#include <string>

namespace logger {

enum class Level {
    Debug,
    Info,
    Warning,
    Warn,
    Error
};

bool Initialize(const std::string& filename = "purpleman.log", bool consoleOutput = true);
void Shutdown();
void Log(Level level, const std::string& message);
void Debug(const std::string& message);
void Info(const std::string& message);
void Warning(const std::string& message);
void Error(const std::string& message);

} // namespace logger
