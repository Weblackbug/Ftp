#include <windows.h>
#include <commctrl.h>
#include <string>
#include <process.h>
#include <filesystem>
#include "resource.h"
#include "ftp_uploader.h"
#include "config_manager.h"
#include "miniz.h"
#include <chrono>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

// Global Variables
HINSTANCE hInst;
HWND hMainWindow;
HWND hUploadBtn;
HWND hProgressBar;
HWND hStatusLabel;

// Configuration and State
AppConfig g_config;
std::string g_selectedZip = "";
std::string g_downloadZipPath = "";

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK SettingsDlg(HWND, UINT, WPARAM, LPARAM);
unsigned __stdcall UploadThread(void* pArgs);
unsigned __stdcall DownloadThread(void* pArgs);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    
    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES; // Progress bar needs this
    if (!InitCommonControlsEx(&icex)) {
        MessageBoxA(NULL, "InitCommonControlsEx Failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Load Config
    g_config = ConfigManager::LoadConfig("config.json");

    // Register Class
    WNDCLASSEXA wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BLOG_ICON)); // Use custom blog icon
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BLOG_ICON)); // Small icon

    if (!RegisterClassExA(&wcex)) {
        MessageBoxA(NULL, ("RegisterClassEx Failed! Error: " + std::to_string(GetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Create Window (Centered, 400x250)
    int width = 400;
    int height = 250;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    hMainWindow = CreateWindowA("FtpUploaderClass", "FTP Uploader Rev 1.53", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height, NULL, NULL, hInstance, NULL);

    if (!hMainWindow) {
        MessageBoxA(NULL, ("CreateWindow Failed! Error: " + std::to_string(GetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hMainWindow, nCmdShow);
    UpdateWindow(hMainWindow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
    {
        // Button
        hUploadBtn = CreateWindowA("BUTTON", "Subir blog", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            140, 150, 100, 30, hWnd, (HMENU)1, hInst, NULL);

        // Status Label
        hStatusLabel = CreateWindowA("STATIC", "Listo.", WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, 120, 360, 20, hWnd, (HMENU)2, hInst, NULL);

        // Progress Bar
        hProgressBar = CreateWindowExA(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
            10, 80, 360, 30, hWnd, (HMENU)3, hInst, NULL);
        SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDM_HELP_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUT), hWnd, About);
            break;
        case IDM_CONFIG_SETTINGS:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hWnd, SettingsDlg);
            break;
        case IDM_FILE_OPEN_ZIP:
        {
            char filename[MAX_PATH] = { 0 };
            OPENFILENAMEA ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = "ZIP Files\0*.zip\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            
            if (GetOpenFileNameA(&ofn)) {
                g_selectedZip = filename;
                SetWindowTextA(hStatusLabel, ("ZIP seleccionado: " + std::string(filename)).c_str());
            }
        }
        break;
        case IDM_FILE_DOWNLOAD_ZIP:
        {
            char filename[MAX_PATH] = { 0 };
            OPENFILENAMEA ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = "ZIP Files\0*.zip\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = "zip";
            
            if (GetSaveFileNameA(&ofn)) {
                g_downloadZipPath = filename;
                _beginthreadex(NULL, 0, DownloadThread, NULL, 0, NULL);
            }
        }
        break;
        case IDM_FILE_EXIT:
            DestroyWindow(hWnd);
            break;
        case 1: // Upload Button
            _beginthreadex(NULL, 0, UploadThread, NULL, 0, NULL);
            break;
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// ...

// Helper to zip directory recursively
bool CreateZipFromDirectory(const std::string& sourceDir, const std::string& zipFile) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_writer_init_file(&zip_archive, zipFile.c_str(), 0)) {
        return false;
    }

    // Iterate recursively
    for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
        if (entry.is_regular_file()) {
            std::string filePath = entry.path().string();
            std::string relativePath = std::filesystem::relative(entry.path(), sourceDir).string();
            
            // Replace backslashes with forward slashes for zip standard
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

            if (!mz_zip_writer_add_file(&zip_archive, relativePath.c_str(), filePath.c_str(), NULL, 0, MZ_DEFAULT_LEVEL)) {
                mz_zip_writer_end(&zip_archive);
                return false;
            }
        }
    }

    if (!mz_zip_writer_finalize_archive(&zip_archive)) {
        mz_zip_writer_end(&zip_archive);
        return false;
    }

    mz_zip_writer_end(&zip_archive);
    return true;
}

unsigned __stdcall DownloadThread(void* pArgs) {
    EnableWindow(hUploadBtn, FALSE);
    SetWindowTextA(hStatusLabel, "Preparando descarga...");
    
    // Set Marquee Progress Bar - REMOVED per user request
    // SetWindowLong(hProgressBar, GWL_STYLE, GetWindowLong(hProgressBar, GWL_STYLE) | PBS_MARQUEE);
    // SendMessage(hProgressBar, PBM_SETMARQUEE, (WPARAM)TRUE, (LPARAM)30);

    FtpUploader uploader(g_config.host, g_config.user, g_config.pass);
    
    // Use absolute path for temp dir to avoid ambiguity
    std::filesystem::path tempPath = std::filesystem::absolute("temp_download_blog");
    std::string tempDir = tempPath.string();
    
    // Create temp dir
    if (std::filesystem::exists(tempPath)) std::filesystem::remove_all(tempPath);
    std::filesystem::create_directory(tempPath);

    auto startTime = std::chrono::steady_clock::now();
    auto lastUpdate = std::chrono::steady_clock::now();

    auto callback = [&](const UploadStats& stats) -> bool {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
        
        // Throttle updates to ~100ms (10fps) unless it's the first few files
        if (elapsed < 100 && stats.uploadedFiles > 5) return true;

        lastUpdate = now;
        
        // Calculate ETR
        auto totalElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        std::string etrStr = "--:--";
        
        if (totalElapsed > 0 && stats.uploadedFiles > 0) {
            double filesPerSec = (double)stats.uploadedFiles / totalElapsed;
            if (filesPerSec > 0) {
                long long remainingFiles = 0; // Unknown for download usually, but if we knew total...
                // For download, psftp doesn't give total files easily upfront without a separate listing.
                // So we might just show elapsed time or speed.
                // However, if we assume user wants "Time Remaining", we need total.
                // In DownloadDirectory, we don't know totalFiles initially.
                // Let's just show Elapsed Time for Download, and ETR for Upload (where we know total).
                
                long long minutes = totalElapsed / 60;
                long long seconds = totalElapsed % 60;
                std::stringstream ss;
                ss << std::setw(2) << std::setfill('0') << minutes << ":" << std::setw(2) << std::setfill('0') << seconds;
                etrStr = ss.str();
            }
        }

        std::string status = "Descargando: " + stats.currentFile + " (T: " + etrStr + ")";
        SetWindowTextA(hStatusLabel, status.c_str());
        return true;
    };



// ...

    if (uploader.DownloadDirectory("/", tempDir, callback)) {
        SetWindowTextA(hStatusLabel, "Comprimiendo ZIP (Nativo)...");
        
        // Zip using miniz (Native)
        bool success = CreateZipFromDirectory(tempDir, g_downloadZipPath);
        int result = success ? 0 : 1;

        // Stop Marquee - REMOVED
        // SendMessage(hProgressBar, PBM_SETMARQUEE, (WPARAM)FALSE, 0);
        // SetWindowLong(hProgressBar, GWL_STYLE, GetWindowLong(hProgressBar, GWL_STYLE) & ~PBS_MARQUEE);
        SendMessage(hProgressBar, PBM_SETPOS, 100, 0);

        if (result == 0) {
            MessageBoxA(hMainWindow, "Descarga completada con exito!", "Info", MB_OK | MB_ICONINFORMATION);
            SetWindowTextA(hStatusLabel, "Descarga completada.");
        } else {
            MessageBoxA(hMainWindow, "Error al comprimir ZIP (tar failed).", "Error", MB_OK | MB_ICONERROR);
            SetWindowTextA(hStatusLabel, "Error en compresion.");
        }
    } else {
        // Stop Marquee - REMOVED
        // SendMessage(hProgressBar, PBM_SETMARQUEE, (WPARAM)FALSE, 0);
        // SetWindowLong(hProgressBar, GWL_STYLE, GetWindowLong(hProgressBar, GWL_STYLE) & ~PBS_MARQUEE);
        SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

        std::string err = "Error: " + uploader.GetLastErrorStr();
        MessageBoxA(hMainWindow, err.c_str(), "Error", MB_OK | MB_ICONERROR);
        SetWindowTextA(hStatusLabel, "Error en descarga.");
    }

    // Cleanup
    // std::filesystem::remove_all(tempPath);
    EnableWindow(hUploadBtn, TRUE);
    return 0;
}

unsigned __stdcall UploadThread(void* pArgs) {
    EnableWindow(hUploadBtn, FALSE);
    SetWindowTextA(hStatusLabel, "Preparando subida...");

    FtpUploader uploader(g_config.host, g_config.user, g_config.pass);
    std::string uploadDir = g_config.localDir;
    bool isZip = !g_selectedZip.empty();
    std::string tempDir = "temp_upload_zip";

    if (isZip) {
        SetWindowTextA(hStatusLabel, "Descomprimiendo ZIP...");
        // Create temp dir
        std::filesystem::create_directory(tempDir);
        
        // Unzip using tar
        std::string cmd = "tar -xf \"" + g_selectedZip + "\" -C \"" + tempDir + "\"";
        system(cmd.c_str());
        
        uploadDir = tempDir;
    }

    auto startTime = std::chrono::steady_clock::now();
    auto lastUpdate = std::chrono::steady_clock::now();

    auto callback = [&](const UploadStats& stats) -> bool {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();

        // Throttle updates to ~100ms
        if (elapsed < 100 && stats.uploadedFiles < stats.totalFiles) return true;

        lastUpdate = now;

        // Calculate ETR
        auto totalElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        std::string timeStr = "Calculando...";

        if (totalElapsed > 2 && stats.uploadedFiles > 0) { // Wait 2s for stable speed
            double filesPerSec = (double)stats.uploadedFiles / totalElapsed;
            if (filesPerSec > 0) {
                long long remainingFiles = stats.totalFiles - stats.uploadedFiles;
                long long remainingSeconds = (long long)(remainingFiles / filesPerSec);
                
                long long minutes = remainingSeconds / 60;
                long long seconds = remainingSeconds % 60;
                
                std::stringstream ss;
                ss << "ETR: " << std::setw(2) << std::setfill('0') << minutes << ":" << std::setw(2) << std::setfill('0') << seconds;
                timeStr = ss.str();
            }
        }

        PostMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, stats.totalFiles));
        PostMessage(hProgressBar, PBM_SETPOS, stats.uploadedFiles, 0);

        std::string status = "Subiendo: " + stats.currentFile + " (" + std::to_string(stats.uploadedFiles) + "/" + std::to_string(stats.totalFiles) + ") - " + timeStr;
        SetWindowTextA(hStatusLabel, status.c_str());

        return true; // Continue
    };

    // Check if remote directory exists
    SetWindowTextA(hStatusLabel, "Verificando directorio remoto...");
    if (!uploader.RemoteDirectoryExists(g_config.remoteDir)) {
        std::string msg = "El directorio remoto '" + g_config.remoteDir + "' no existe.\n¿Desea crearlo?";
        int ret = MessageBoxA(hMainWindow, msg.c_str(), "Directorio no encontrado", MB_YESNO | MB_ICONQUESTION);
        if (ret == IDNO) {
            SetWindowTextA(hStatusLabel, "Subida cancelada.");
            EnableWindow(hUploadBtn, TRUE);
            return 0;
        }
        // If YES, UploadDirectory will create it (it uses mkdir -p)
    }

    if (uploader.UploadDirectory(uploadDir, g_config.remoteDir, callback)) {
        MessageBoxA(hMainWindow, "Subida completada con exito!", "Info", MB_OK | MB_ICONINFORMATION);
        SetWindowTextA(hStatusLabel, "Completado.");
    } else {
        std::string err = "Error: " + uploader.GetLastErrorStr();
        MessageBoxA(hMainWindow, err.c_str(), "Error", MB_OK | MB_ICONERROR);
        SetWindowTextA(hStatusLabel, "Error.");
    }

    if (isZip) {
        // Cleanup
        std::filesystem::remove_all(tempDir);
        g_selectedZip = ""; // Reset selection after upload
    }

    EnableWindow(hUploadBtn, TRUE);
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK SettingsDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        SetDlgItemTextA(hDlg, IDC_EDIT_HOST, g_config.host.c_str());
        SetDlgItemTextA(hDlg, IDC_EDIT_USER, g_config.user.c_str());
        SetDlgItemTextA(hDlg, IDC_EDIT_PASS, g_config.pass.c_str());
        SetDlgItemTextA(hDlg, IDC_EDIT_DIR, g_config.localDir.c_str());
        SetDlgItemTextA(hDlg, IDC_EDIT_REMOTE_DIR, g_config.remoteDir.c_str());
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CHECK_SHOW_PASS) {
            BOOL checked = IsDlgButtonChecked(hDlg, IDC_CHECK_SHOW_PASS);
            HWND hPass = GetDlgItem(hDlg, IDC_EDIT_PASS);
            
            // Toggle password char
            // Using EM_SETPASSWORDCHAR is simpler than changing styles on the fly
            // Default password char is usually a bullet (0x25CF) or asterisk
            // But if we just want to show/hide, 0 removes it.
            // To restore, we can use '*' or just reset the style?
            // Let's try removing/adding ES_PASSWORD style.
            
            /*
            LONG style = GetWindowLong(hPass, GWL_STYLE);
            if (checked) style &= ~ES_PASSWORD;
            else style |= ES_PASSWORD;
            SetWindowLong(hPass, GWL_STYLE, style);
            SetWindowPos(hPass, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            InvalidateRect(hPass, NULL, TRUE);
            */
            
            // Simpler approach:
            SendMessage(hPass, EM_SETPASSWORDCHAR, checked ? 0 : (WPARAM)'*', 0);
            InvalidateRect(hPass, NULL, TRUE);
            return (INT_PTR)TRUE;
        }

        if (LOWORD(wParam) == IDOK) {
            char buf[1024];
            GetDlgItemTextA(hDlg, IDC_EDIT_HOST, buf, 1024); g_config.host = buf;
            GetDlgItemTextA(hDlg, IDC_EDIT_USER, buf, 1024); g_config.user = buf;
            GetDlgItemTextA(hDlg, IDC_EDIT_PASS, buf, 1024); g_config.pass = buf;
            GetDlgItemTextA(hDlg, IDC_EDIT_DIR, buf, 1024); g_config.localDir = buf;
            GetDlgItemTextA(hDlg, IDC_EDIT_REMOTE_DIR, buf, 1024); g_config.remoteDir = buf;
            
            ConfigManager::SaveConfig("config.json", g_config);
            EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
