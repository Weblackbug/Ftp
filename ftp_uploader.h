#pragma once
#include <string>
#include <vector>
#include <functional>
#include <windows.h>

struct UploadStats {
    int totalFiles;
    int uploadedFiles;
    std::string currentFile;
};

using ProgressCallback = std::function<bool(const UploadStats&)>;

class FtpUploader {
public:
    FtpUploader(const std::string& server, const std::string& user, const std::string& pass);
    ~FtpUploader();

    bool UploadDirectory(const std::string& localDir, const std::string& remoteDir, ProgressCallback callback);
    bool RemoteDirectoryExists(const std::string& remoteDir);
    std::string GetLastErrorStr() const { return m_lastError; }

    // Download
    bool DownloadDirectory(const std::string& remoteDir, const std::string& localDir, ProgressCallback callback);

private:
    std::string m_server;
    std::string m_user;
    std::string m_pass;
    std::string m_lastError;

    bool UploadRecursive(const std::string& localPath, const std::string& remotePath, UploadStats& stats, ProgressCallback callback);
    bool ExecutePscpUpload(const std::string& localFile, const std::string& remotePath);
    bool ExecutePlinkMkdir(const std::string& remotePath);
    
    // Batch optimization
    struct FileTask {
        std::string localPath;
        std::string remotePath;
    };
    void CollectFiles(const std::string& localDir, const std::string& remoteDir, std::vector<std::string>& directories, std::vector<FileTask>& files, UploadStats& stats);
    bool ExecuteBatchUpload(const std::vector<std::string>& directories, const std::vector<FileTask>& files, UploadStats& stats, ProgressCallback callback);
};
