#include "utils/helpers.h"
#include <fstream>

namespace utils {

std::string ExtractJSONValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\"";
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) {
        searchKey = "\"" + key + "\": \"";
        pos = json.find(searchKey);
    }
    if (pos == std::string::npos) {
        return std::string();
    }

    pos += searchKey.size();
    auto endPos = json.find('"', pos);
    if (endPos == std::string::npos) {
        return std::string();
    }

    return json.substr(pos, endPos - pos);
}

std::string XOREncrypt(const std::string& data, const std::string& key) {
    std::string result = data;
    for (size_t i = 0; i < result.size(); i++) {
        result[i] ^= key[i % key.size()];
    }
    return result;
}

bool WriteTextFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << content;
    return out.good();
}

bool ReadTextFile(const std::string& path, std::string& content) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    content.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}

bool ParseJSONBool(const std::string& json, const std::string& key, bool defaultValue) {
    std::string searchKey = "\"" + key + "\":";
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) {
        searchKey = "\"" + key + "\": ";
        pos = json.find(searchKey);
    }
    if (pos == std::string::npos) {
        return defaultValue;
    }

    pos += searchKey.size();
    auto endPos = json.find_first_of(",}\n", pos);
    if (endPos == std::string::npos) {
        endPos = json.size();
    }

    std::string token = json.substr(pos, endPos - pos);
    if (token.find("true") != std::string::npos) return true;
    if (token.find("false") != std::string::npos) return false;
    return defaultValue;
}

int ParseJSONInt(const std::string& json, const std::string& key, int defaultValue) {
    std::string searchKey = "\"" + key + "\":";
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) {
        searchKey = "\"" + key + "\": ";
        pos = json.find(searchKey);
    }
    if (pos == std::string::npos) {
        return defaultValue;
    }

    pos += searchKey.size();
    while (pos < json.size() && isspace(static_cast<unsigned char>(json[pos]))) {
        pos++;
    }

    auto endPos = json.find_first_not_of("0123456789", pos);
    if (endPos == std::string::npos) {
        endPos = json.size();
    }

    if (pos >= endPos) {
        return defaultValue;
    }

    try {
        return std::stoi(json.substr(pos, endPos - pos));
    } catch (...) {
        return defaultValue;
    }
}

} // namespace utils
