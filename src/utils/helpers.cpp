#include "utils/helpers.h"
#include <fstream>
#include <sstream>
#include <vector>

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

std::string Base64Encode(const std::string& input) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    for (size_t i = 0; i < input.size(); i += 3) {
        unsigned char b0 = static_cast<unsigned char>(input[i]);
        unsigned char b1 = (i + 1 < input.size()) ? static_cast<unsigned char>(input[i + 1]) : 0;
        unsigned char b2 = (i + 2 < input.size()) ? static_cast<unsigned char>(input[i + 2]) : 0;

        unsigned char c0 = static_cast<unsigned char>(b0 >> 2);
        unsigned char c1 = static_cast<unsigned char>(((b0 & 0x03) << 4) | (b1 >> 4));
        unsigned char c2 = static_cast<unsigned char>(((b1 & 0x0F) << 2) | (b2 >> 6));
        unsigned char c3 = static_cast<unsigned char>(b2 & 0x3F);

        output.push_back(alphabet[c0]);
        output.push_back(alphabet[c1]);
        output.push_back((i + 1 < input.size()) ? alphabet[c2] : '=');
        output.push_back((i + 2 < input.size()) ? alphabet[c3] : '=');
    }

    return output;
}

std::string Base64Decode(const std::string& input) {
    static const int decodeTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };

    std::string cleaned;
    cleaned.reserve(input.size());
    for (unsigned char c : input) {
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            continue;
        }
        cleaned.push_back(static_cast<char>(c));
    }

    std::string output;
    output.reserve(cleaned.size() / 4 * 3);

    size_t i = 0;
    while (i + 4 <= cleaned.size()) {
        int a = decodeTable[static_cast<unsigned char>(cleaned[i])];
        int b = decodeTable[static_cast<unsigned char>(cleaned[i + 1])];
        int c = decodeTable[static_cast<unsigned char>(cleaned[i + 2])];
        int d = decodeTable[static_cast<unsigned char>(cleaned[i + 3])];
        if (a < 0 || b < 0) break;

        output.push_back(static_cast<char>((a << 2) | (b >> 4)));
        if (c >= 0) {
            output.push_back(static_cast<char>(((b & 0x0F) << 4) | (c >> 2)));
        }
        if (d >= 0) {
            output.push_back(static_cast<char>(((c & 0x03) << 6) | d));
        }
        i += 4;
    }

    return output;
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
