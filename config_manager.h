/////////////////////////////////////////////////////////////////////////////////////
// Autor: Sergi Serrano Pérez , WeBlackbug 1987 - 2024 Canovelles - Granollers..   //
// Archivo: config_manager.h                                                       //
// Licencia: Libre distribución.                                                   //
/////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <string>

struct AppConfig {
    std::string host;
    std::string user;
    std::string pass;
    std::string localDir;
    std::string remoteDir;
};

class ConfigManager {
public:
    static AppConfig LoadConfig(const std::string& filename);
    static void SaveConfig(const std::string& filename, const AppConfig& config);
};
