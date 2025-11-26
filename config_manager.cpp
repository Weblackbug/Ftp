#include "config_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>

// Simple trim
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Simple unquote
static std::string unquote(std::string str) {
    str = trim(str);
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

AppConfig ConfigManager::LoadConfig(const std::string& filename) {
    AppConfig config;
    // Defaults
    config.host = "access999719206.webspace-data.io";
    config.user = "u115817696";
    config.pass = "SspSdcAsdMsd-1972";
    config.pass = "SspSdcAsdMsd-1972";
    config.localDir = "C:\\xampp\\htdocs\\canal_k";
    config.remoteDir = "/";

    std::ifstream file(filename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t sep = line.find(':');
            if (sep != std::string::npos) {
                std::string key = trim(line.substr(0, sep));
                std::string val = unquote(line.substr(sep + 1));
                
                // Remove trailing comma if present (pseudo-json)
                if (!val.empty() && val.back() == ',') val.pop_back();
                val = unquote(val); // Unquote again if needed

                if (key == "\"host\"") config.host = val;
                else if (key == "\"user\"") config.user = val;
                else if (key == "\"pass\"") config.pass = val;
                else if (key == "\"localDir\"") config.localDir = val;
                else if (key == "\"remoteDir\"") config.remoteDir = val;
            }
        }
    }
    return config;
}

void ConfigManager::SaveConfig(const std::string& filename, const AppConfig& config) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "{\n";
        file << "  \"host\": \"" << config.host << "\",\n";
        file << "  \"user\": \"" << config.user << "\",\n";
        file << "  \"pass\": \"" << config.pass << "\",\n";
        // Escape backslashes for JSON validity (simple version)
        std::string safeDir = config.localDir;
        size_t pos = 0;
        while ((pos = safeDir.find('\\', pos)) != std::string::npos) {
            safeDir.replace(pos, 1, "\\\\");
            pos += 2;
        }
        file << "  \"localDir\": \"" << safeDir << "\",\n";
        file << "  \"remoteDir\": \"" << config.remoteDir << "\"\n";
        file << "}\n";
    }
}
