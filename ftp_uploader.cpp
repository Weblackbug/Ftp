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

    // 1. Collect all work
    directories.push_back(remoteDir); // Ensure base dir is created
    CollectFiles(localDir, remoteDir, directories, files, stats);

    // 2. Execute Batch
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

    // 1. Create Directories (Batching Plink commands)
    // Windows command line limit is ~8191 chars. Let's be safe with 6000.
    std::string currentCmdGroup;
    
    // Ensure base directory exists first
    // Extract base remote dir from the first file or just use the passed remoteDir?
    // The passed 'directories' vector contains full paths.
    // We should probably add the base remoteDir to the list or just mkdir it.
    // However, ExecuteBatchUpload doesn't take remoteDir as arg, it takes lists.
    // Wait, UploadDirectory calls CollectFiles then ExecuteBatchUpload.
    // I should modify UploadDirectory to pass remoteDir to ExecuteBatchUpload or just add it to 'directories'.
    
    // Let's modify UploadDirectory to add the base dir to 'directories' list.
    // But wait, ExecuteBatchUpload is here.
    // Let's look at UploadDirectory in this file.
    // It calls CollectFiles.
    
    // Actually, simpler to just modify UploadDirectory to push remoteDir to 'directories' vector.
    // But I am editing ExecuteBatchUpload here.
    // Let's go to UploadDirectory instead.
    
    for (const auto& dir : directories) {
        std::string mkdirCmd = "mkdir -p \"" + dir + "\"; ";
        if (currentCmdGroup.length() + mkdirCmd.length() > 6000) {
            // Execute current chunk
             std::stringstream cmd;
             cmd << "plink.exe -ssh -pw \"" << m_pass << "\" -batch -hostkey \"" << fingerprint << "\" \"" << m_user << "@" << host << "\" \"" << currentCmdGroup << "\"";
             // We don't check result strictly as dirs might exist
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

    // 2. Generate PSFTP Script
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

    // 3. Execute PSFTP
    std::stringstream psftpCmd;
    psftpCmd << "psftp.exe -pw \"" << m_pass << "\" -batch -hostkey \"" << fingerprint << "\" -b " << scriptFilename << " \"" << m_user << "@" << host << "\"";
    
    // Capture output to update progress
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
    si.hStdError = hStdOutWr; // Capture stderr too

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
                
                // Parse line to find what's happening
                // psftp output for put: "remote:/path/to/file" or "local:..." -> "remote:..."
                // Simple heuristic: if line contains " => ", it's a transfer
                if (line.find(" => ") != std::string::npos) {
                    // Extract filename?
                    // Format: "local_file => remote_file"
                    // Let's just take the current file from our list if we can track it, 
                    // or just increment counter.
                    // Since psftp executes in order, we can try to match, but simple counter is safer.
                    stats.uploadedFiles++;
                    // Try to find the filename in the line for display
                    size_t arrowPos = line.find(" => ");
                    if (arrowPos != std::string::npos) {
                        std::string remotePart = line.substr(arrowPos + 4);
                        // Extract just the filename
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

    // Generate PSFTP Script for Download
    std::string scriptFilename = "download_script.scr";
    std::ofstream script(scriptFilename);
    if (!script.is_open()) {
        m_lastError = "Failed to create download script";
        return false;
    }

    // Use lcd to set local dir, then cd to remote and mget -r * to download contents
    script << "lcd \"" << localDir << "\"" << std::endl;
    script << "cd \"" << remoteDir << "\"" << std::endl;
    script << "mget -r *" << std::endl;
    script.close();

    // Execute PSFTP
    std::stringstream psftpCmd;
    psftpCmd << "psftp.exe -pw \"" << m_pass << "\" -batch -hostkey \"" << fingerprint << "\" -b " << scriptFilename << " \"" << m_user << "@" << host << "\"";

    // Capture output
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

                // Parse output for progress
                // psftp download output: "remote:/path/to/file => local:file"
                // or "remote:file => local:file"
                if (line.find(" => ") != std::string::npos) {
                    stats.uploadedFiles++; // Reuse this counter for downloaded files
                    
                    size_t arrowPos = line.find(" => ");
                    if (arrowPos != std::string::npos) {
                        std::string remotePart = line.substr(0, arrowPos);
                        // Remove "remote:" prefix if present (psftp sometimes adds it)
                        if (remotePart.find("remote:") == 0) remotePart = remotePart.substr(7);

                        // Extract filename
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

    // Debug logging
    std::ofstream log("dir_check_debug.txt", std::ios::app);
    log << "Cmd: " << cmd.str() << "\n";
    log << "Result: " << result << "\n";
    log.close();

    // Check if result contains the directory name (or part of it) and NOT "No such file"
    // Also handle "cannot access" etc.
    // Simplest: if it returns the directory name, it exists.
    // But ls -d /hola returns /hola
    // ls -d /invalid returns "ls: cannot access '/invalid': No such file or directory"
    
    // Check if result contains "No such file" or "cannot access"
    if (result.find("No such file") != std::string::npos || result.find("cannot access") != std::string::npos || result.find("does not exist") != std::string::npos) {
        return false;
    }
    
    // If result is empty, something is wrong (connection failed?), assume false
    if (result.empty()) return false;

    return true;
}

// Unused in batch mode but kept for interface compatibility if needed, or can be removed.
bool FtpUploader::UploadRecursive(const std::string& localPath, const std::string& remotePath, UploadStats& stats, ProgressCallback callback) {
    return true; 
}
bool FtpUploader::ExecutePscpUpload(const std::string& localFile, const std::string& remotePath) { return true; }
bool FtpUploader::ExecutePlinkMkdir(const std::string& remotePath) { return true; }
