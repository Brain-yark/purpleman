#pragma once

#include <string>

namespace utils {

std::string ExtractJSONValue(const std::string& json, const std::string& key);
std::string XOREncrypt(const std::string& data, const std::string& key = "OfflineC2Key");
std::string Base64Encode(const std::string& input);
std::string Base64Decode(const std::string& input);
bool WriteTextFile(const std::string& path, const std::string& content);
bool ReadTextFile(const std::string& path, std::string& content);
bool ParseJSONBool(const std::string& json, const std::string& key, bool defaultValue = false);
int ParseJSONInt(const std::string& json, const std::string& key, int defaultValue = 0);

} // namespace utils
