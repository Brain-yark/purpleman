#include <iostream>
#include <string>

#include "utils/helpers.h"

static void Assert(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    std::string json = "{\"id\":\"abc123\", \"hostname\":\"host1\", \"os\":\"Windows\"}";
    Assert(utils::ExtractJSONValue(json, "id") == "abc123", "ExtractJSONValue should parse id");
    Assert(utils::ExtractJSONValue(json, "hostname") == "host1", "ExtractJSONValue should parse hostname");
    Assert(utils::ExtractJSONValue(json, "os") == "Windows", "ExtractJSONValue should parse os");
    Assert(utils::ExtractJSONValue(json, "missing").empty(), "ExtractJSONValue should return empty for missing keys");

    std::string plain = "UtilityTest";
    std::string key = "SecretKey";
    std::string encrypted = utils::XOREncrypt(plain, key);
    std::string decrypted = utils::XOREncrypt(encrypted, key);
    Assert(decrypted == plain, "XOREncrypt should round-trip correctly");

    std::string encoded = utils::Base64Encode("hello");
    std::string decoded = utils::Base64Decode(encoded);
    Assert(decoded == "hello", "Base64Encode/Base64Decode should round-trip text");

    std::string testFile = "utils_test_output.txt";
    Assert(utils::WriteTextFile(testFile, "hello world"), "WriteTextFile should succeed");

    std::cout << "All utility tests passed." << std::endl;
    return EXIT_SUCCESS;
}
