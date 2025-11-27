/////////////////////////////////////////////////////////////////////////////////////
// Autor: Sergi Serrano Pérez , WeBlackbug 1987 - 2024 Canovelles - Granollers..   //
// Archivo: ftp_uploader.cpp                                                       //
// Licencia: Libre distribución.                                                   //
/////////////////////////////////////////////////////////////////////////////////////

#include "ftp_uploader.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace fs = std::filesystem;

FtpUploader::FtpUploader(const std::string& server, const std::string& user, const std::string& pass)
    : m_server(server), m_user(user), m_pass(pass) {}

FtpUploader::~FtpUploader() {
}

bool FtpUploader::UploadDirectory(const std::string& localDir, const std::string& remoteDir, ProgressCallback callback) {
    UploadStats stats = { 0, 0, "" };
    std::vector<std::string> directories;
    std::vector<FileTask> files;

    // 1. Recollir tota la feina
    directories.push_back(remoteDir); // Assegurar que el directori base es crea
    CollectFiles(localDir, remoteDir, directories, files, stats);

    // 2. Executar Lot
    return ExecuteBatchUpload(directories, files, stats, callback);
}

void FtpUploader::CollectFiles(const std::string& localDir, const std::string& remoteDir, std::vector<std::string>& directories, std::vector<FileTask>& files, UploadStats& stats) {
    for (const auto& entry : fs::directory_iterator(localDir)) {
        std::string filename = entry.path().filename().string();
        std::string currentRemotePath = remoteDir + (remoteDir.back() == '/' ? "" : "/") + filename;

        if (entry.is_directory()) {
            directories.push_back(currentRemotePath);
            CollectFiles(entry.path().string(), currentRemotePath, directories, files, stats);
        } else if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".zip" || ext == ".md" || ext == ".bat" || ext == ".exe") continue;

            files.push_back({ entry.path().string(), currentRemotePath });
            stats.totalFiles++;
        }
    }
}

bool FtpUploader::ExecuteBatchUpload(const std::vector<std::string>& directories, const std::vector<FileTask>& files, UploadStats& stats, ProgressCallback callback) {
    std::string host = m_server;
    if (host.find("sftp://") == 0) host = host.substr(7);
    std::string fingerprint = "ssh-ed25519 255 SHA256:1gx2w8Rtv3wCgi7Jh8myf/KVd72cRQbow03UP8P095Q";

    // 1. Crear Directoris (Agrupant comandes Plink)
    // El límit de la línia de comandes de Windows és ~8191 caràcters. Anem segurs amb 6000.
    std::string currentCmdGroup;
    
    // Assegurar que el directori base existeix primer
    
    
    for (const auto& dir : directories) {
        std::string mkdirCmd = "mkdir -p \"" + dir + "\"; ";
        if (currentCmdGroup.length() + mkdirCmd.length() > 6000) {
            // Executar fragment actual
             std::stringstream cmd;
             cmd << "plink.exe -ssh -pw \"" << m_pass << "\" -batch -hostkey \"" << fingerprint << "\" \"" << m_user << "@" << host << "\" \"" << currentCmdGroup << "\"";
             // No comprovem el resultat estrictament ja que els directoris podrien existir
             system(cmd.str().c_str()); 
             currentCmdGroup = "";
        }
        currentCmdGroup += mkdirCmd;
    }
    if (!currentCmdGroup.empty()) {
         std::stringstream cmd;
         cmd << "plink.exe -ssh -pw \"" << m_pass << "\" -batch -hostkey \"" << fingerprint << "\" \"" << m_user << "@" << host << "\" \"" << currentCmdGroup << "\"";
         system(cmd.str().c_str());
    }

    // 2. Generar Script PSFTP
    std::string scriptFilename = "upload_script.scr";
    std::ofstream script(scriptFilename);
    if (!script.is_open()) {
        m_lastError = "Failed to create batch script";
        return false;
    }

    for (const auto& file : files) {
        script << "put \"" << file.localPath << "\" \"" << file.remotePath << "\"" << std::endl;
    }
    script.close();

    // 3. Executar PSFTP
    std::stringstream psftpCmd;
    psftpCmd << "psftp.exe -pw \"" << m_pass << "\" -batch -hostkey \"" << fingerprint << "\" -b " << scriptFilename << " \"" << m_user << "@" << host << "\"";
    
    // Capturar sortida per actualitzar progrés
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdOutRd, hStdOutWr;
    if (!CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0)) return false;
    SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hStdOutWr;
    si.hStdError = hStdOutWr; // Capturar stderr també

    ZeroMemory(&pi, sizeof(pi));
    std::string cmdLine = psftpCmd.str();
    std::vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    if (CreateProcessA(NULL, cmdBuffer.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hStdOutWr);

        DWORD bytesRead;
        char buffer[1024];
        std::string lineBuffer;
        
        while (ReadFile(hStdOutRd, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            lineBuffer += buffer;
            
            size_t pos;
            while ((pos = lineBuffer.find('\n')) != std::string::npos) {
                std::string line = lineBuffer.substr(0, pos);
                lineBuffer.erase(0, pos + 1);
                
                // Analitzar línia per veure què passa
                // sortida psftp per put: "remote:/path/to/file" o "local:..." -> "remote:..."
                // Heurística simple: si la línia conté " => ", és una transferència
                if (line.find(" => ") != std::string::npos) {
                    // Extreure nom de fitxer?
                    // Format: "local_file => remote_file"
                    // Agafem el fitxer actual de la nostra llista si podem rastrejar-lo, 
                    // o simplement incrementem el comptador.
                    // Com que psftp executa en ordre, podem intentar coincidir, però un comptador simple és més segur.
                    stats.uploadedFiles++;
                    // Intentar trobar el nom del fitxer a la línia per mostrar
                    size_t arrowPos = line.find(" => ");
                    if (arrowPos != std::string::npos) {
                        std::string remotePart = line.substr(arrowPos + 4);
                        // Extreure només el nom del fitxer
                        size_t slashPos = remotePart.find_last_of('/');
                        if (slashPos != std::string::npos) {
                            stats.currentFile = remotePart.substr(slashPos + 1);
                        } else {
                            stats.currentFile = remotePart;
                        }
                    }
                    callback(stats);
                }
            }
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        CloseHandle(hStdOutWr);
        m_lastError = "Failed to start PSFTP";
        return false;
    }
    
    CloseHandle(hStdOutRd);
    fs::remove(scriptFilename);
    return true;
}

bool FtpUploader::DownloadDirectory(const std::string& remoteDir, const std::string& localDir, ProgressCallback callback) {
    std::string host = m_server;
    if (host.find("sftp://") == 0) host = host.substr(7);
    std::string fingerprint = "ssh-ed25519 255 SHA256:1gx2w8Rtv3wCgi7Jh8myf/KVd72cRQbow03UP8P095Q";

    // Generar Script PSFTP per Descàrrega
    std::string scriptFilename = "download_script.scr";
    std::ofstream script(scriptFilename);
    if (!script.is_open()) {
        m_lastError = "Failed to create download script";
        return false;
    }

    // Utilitzar lcd per establir directori local, després cd al remot i mget -r * per descarregar contingut
    script << "lcd \"" << localDir << "\"" << std::endl;
    script << "cd \"" << remoteDir << "\"" << std::endl;
    script << "mget -r *" << std::endl;
    script.close();

    // Executar PSFTP
    std::stringstream psftpCmd;
    psftpCmd << "psftp.exe -pw \"" << m_pass << "\" -batch -hostkey \"" << fingerprint << "\" -b " << scriptFilename << " \"" << m_user << "@" << host << "\"";

    // Capturar sortida
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdOutRd, hStdOutWr;
    if (!CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0)) return false;
    SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hStdOutWr;
    si.hStdError = hStdOutWr;

    ZeroMemory(&pi, sizeof(pi));
    std::string cmdLine = psftpCmd.str();
    std::vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    UploadStats stats = { 0, 0, "Descargando..." };

    if (CreateProcessA(NULL, cmdBuffer.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hStdOutWr);

        DWORD bytesRead;
        char buffer[1024];
        std::string lineBuffer;

        while (ReadFile(hStdOutRd, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            lineBuffer += buffer;

            size_t pos;
            while ((pos = lineBuffer.find('\n')) != std::string::npos) {
                std::string line = lineBuffer.substr(0, pos);
                lineBuffer.erase(0, pos + 1);

                // Analitzar sortida per progrés
                // psftp download output: "remote:/path/to/file => local:file"
                // or "remote:file => local:file"
                if (line.find(" => ") != std::string::npos) {
                    stats.uploadedFiles++; // Reutilitzar aquest comptador per fitxers descarregats
                    
                    size_t arrowPos = line.find(" => ");
                    if (arrowPos != std::string::npos) {
                        std::string remotePart = line.substr(0, arrowPos);
                        // Eliminar prefix "remote:" si està present (psftp de vegades l'afegeix)
                        if (remotePart.find("remote:") == 0) remotePart = remotePart.substr(7);

                        // Extreure nom de fitxer
                        size_t slashPos = remotePart.find_last_of('/');
                        if (slashPos != std::string::npos) {
                            stats.currentFile = remotePart.substr(slashPos + 1);
                        } else {
                            stats.currentFile = remotePart;
                        }
                    }
                    callback(stats);
                }
            }
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        CloseHandle(hStdOutWr);
        m_lastError = "Failed to start PSFTP for download";
        return false;
    }

    CloseHandle(hStdOutRd);
    fs::remove(scriptFilename);
    return true;
}

bool FtpUploader::RemoteDirectoryExists(const std::string& remoteDir) {
    std::string host = m_server;
    if (host.find("sftp://") == 0) host = host.substr(7);
    std::string fingerprint = "ssh-ed25519 255 SHA256:1gx2w8Rtv3wCgi7Jh8myf/KVd72cRQbow03UP8P095Q";

    // Command: ls -d "dir" 2>&1
    std::stringstream cmd;
    cmd << "plink.exe -ssh -pw \"" << m_pass << "\" -batch -hostkey \"" << fingerprint << "\" \"" << m_user << "@" << host << "\" \"ls -d \\\"" << remoteDir << "\\\" 2>&1\"";

    FILE* pipe = _popen(cmd.str().c_str(), "r");
    if (!pipe) return false;

    char buffer[128];
    std::string result = "";
    while (fgets(buffer, 128, pipe) != NULL) {
        result += buffer;
    }
    _pclose(pipe);

    // Registre de depuració
    std::ofstream log("dir_check_debug.txt", std::ios::app);
    log << "Cmd: " << cmd.str() << "\n";
    log << "Result: " << result << "\n";
    log.close();

    // Comprovar si el resultat conté el nom del directori (o part d'ell) i NO "No such file"
    // També gestionar "cannot access", etc.
    // Més simple: si retorna el nom del directori, existeix.
    // But ls -d /hola returns /hola
    // ls -d /invalid returns "ls: cannot access '/invalid': No such file or directory"
    
    // Comprovar si el resultat conté "No such file" o "cannot access"
    if (result.find("No such file") != std::string::npos || result.find("cannot access") != std::string::npos || result.find("does not exist") != std::string::npos) {
        return false;
    }
    
    // Si el resultat és buit, alguna cosa va malament (connexió fallida?), assumir fals
    if (result.empty()) return false;

    return true;
}

// No utilitzat en mode lot però mantingut per compatibilitat d'interfície si cal, o es pot eliminar.
bool FtpUploader::UploadRecursive(const std::string& localPath, const std::string& remotePath, UploadStats& stats, ProgressCallback callback) {
    return true; 
}
bool FtpUploader::ExecutePscpUpload(const std::string& localFile, const std::string& remotePath) { return true; }
bool FtpUploader::ExecutePlinkMkdir(const std::string& remotePath) { return true; }
