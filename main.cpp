/////////////////////////////////////////////////////////////////////////////////////
// Autor: Sergi Serrano Pérez , WeBlackbug 1987 - 2024 Canovelles - Granollers..   //
// Archivo: main.cpp                                                               //
// Licencia: Libre distribución.                                                   //
/////////////////////////////////////////////////////////////////////////////////////

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

// Variables Globals
HINSTANCE hInst;
HWND hMainWindow;
HWND hUploadBtn;
HWND hProgressBar;
HWND hStatusLabel;

// Configuració i Estat
AppConfig g_config;
std::string g_selectedZip = "";
std::string g_downloadZipPath = "";

// Declaracions anticipades
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK SettingsDlg(HWND, UINT, WPARAM, LPARAM);
unsigned __stdcall UploadThread(void* pArgs);
unsigned __stdcall DownloadThread(void* pArgs);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    
    // Inicialitzar Controls Comuns
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES; // La barra de progrés necessita això
    if (!InitCommonControlsEx(&icex)) {
        MessageBoxA(NULL, "InitCommonControlsEx Failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Carregar Configuració
    g_config = ConfigManager::LoadConfig("config.json");

    // Registrar Classe
    WNDCLASSEXA wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BLOG_ICON));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEA(IDR_MENU1);
    wcex.lpszClassName = "FtpUploaderClass";
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BLOG_ICON));

    if (!RegisterClassExA(&wcex)) {
        MessageBoxA(NULL, ("RegisterClassEx Failed! Error: " + std::to_string(GetLastError())).c_str(), "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Crear Finestra (Centrada, 400x250)
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
        // Botó
        hUploadBtn = CreateWindowA("BUTTON", "Subir blog", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            140, 150, 100, 30, hWnd, (HMENU)1, hInst, NULL);

        // Etiqueta d'Estat
        hStatusLabel = CreateWindowA("STATIC", "Listo.", WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, 120, 360, 20, hWnd, (HMENU)2, hInst, NULL);

        // Barra de Progrés
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
            {
                // Depuració: Comprovar si arribem aquí
                // MessageBoxA(hWnd, "Opening Settings Dialog...", "Debug", MB_OK);
                
                INT_PTR result = DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hWnd, SettingsDlg);
                
                if (result == -1) {
                    std::string err = "DialogBox Failed! Error: " + std::to_string(GetLastError());
                    MessageBoxA(hWnd, err.c_str(), "Error", MB_OK | MB_ICONERROR);
                }
            }
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
        case 1: // Botó de Pujada
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

// Ajudant per comprimir directori recursivament
bool CreateZipFromDirectory(const std::string& sourceDir, const std::string& zipFile) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_writer_init_file(&zip_archive, zipFile.c_str(), 0)) {
        return false;
    }

    // Iterar recursivament
    for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
        if (entry.is_regular_file()) {
            std::string filePath = entry.path().string();
            std::string relativePath = std::filesystem::relative(entry.path(), sourceDir).string();
            
            // Reemplaçar barres invertides per barres normals per l'estàndard zip
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
    
    // Establir Barra de Progrés Marquee - ELIMINAT per petició de l'usuari
    // SetWindowLong(hProgressBar, GWL_STYLE, GetWindowLong(hProgressBar, GWL_STYLE) | PBS_MARQUEE);
    // SendMessage(hProgressBar, PBM_SETMARQUEE, (WPARAM)TRUE, (LPARAM)30);

    FtpUploader uploader(g_config.host, g_config.user, g_config.pass);
    
    // Utilitzar ruta absoluta per al directori temporal per evitar ambigüitats
    std::filesystem::path tempPath = std::filesystem::absolute("temp_download_blog");
    std::string tempDir = tempPath.string();
    
    // Crear directori temporal
    if (std::filesystem::exists(tempPath)) std::filesystem::remove_all(tempPath);
    std::filesystem::create_directory(tempPath);

    auto startTime = std::chrono::steady_clock::now();
    auto lastUpdate = std::chrono::steady_clock::now();

    auto callback = [&](const UploadStats& stats) -> bool {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
        
        // Limitar actualitzacions a ~100ms (10fps) a menys que siguin els primers fitxers
        if (elapsed < 100 && stats.uploadedFiles > 5) return true;

        lastUpdate = now;
        
        // Calcular ETR
        auto totalElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        std::string etrStr = "--:--";
        
        if (totalElapsed > 0 && stats.uploadedFiles > 0) {
            double filesPerSec = (double)stats.uploadedFiles / totalElapsed;
            if (filesPerSec > 0) {
                long long remainingFiles = 0; // Desconegut per descàrrega normalment, però si sabéssim el total...
                // Per descàrrega, psftp no dóna el total de fitxers fàcilment per endavant sense un llistat separat.
                // Així que podríem mostrar només el temps transcorregut o la velocitat.
                // Tanmateix, si assumim que l'usuari vol "Temps Restant", necessitem el total.
                // A DownloadDirectory, no sabem totalFiles inicialment.
                // Mostrem només el Temps Transcorregut per Descàrrega, i ETR per Pujada (on sabem el total).
                
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
        
        // Comprimir usant miniz (Natiu)
        bool success = CreateZipFromDirectory(tempDir, g_downloadZipPath);
        int result = success ? 0 : 1;

        // Aturar Marquee - ELIMINAT
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
        // Aturar Marquee - ELIMINAT
        // SendMessage(hProgressBar, PBM_SETMARQUEE, (WPARAM)FALSE, 0);
        // SetWindowLong(hProgressBar, GWL_STYLE, GetWindowLong(hProgressBar, GWL_STYLE) & ~PBS_MARQUEE);
        SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

        std::string err = "Error: " + uploader.GetLastErrorStr();
        MessageBoxA(hMainWindow, err.c_str(), "Error", MB_OK | MB_ICONERROR);
        SetWindowTextA(hStatusLabel, "Error en descarga.");
    }

    // Neteja
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
        // Crear directori temporal
        std::filesystem::create_directory(tempDir);
        
        // Descomprimir usant tar
        std::string cmd = "tar -xf \"" + g_selectedZip + "\" -C \"" + tempDir + "\"";
        system(cmd.c_str());
        
        uploadDir = tempDir;
    }

    auto startTime = std::chrono::steady_clock::now();
    auto lastUpdate = std::chrono::steady_clock::now();

    auto callback = [&](const UploadStats& stats) -> bool {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();

        // Limitar actualitzacions a ~100ms
        if (elapsed < 100 && stats.uploadedFiles < stats.totalFiles) return true;

        lastUpdate = now;

        // Calcular ETR
        auto totalElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        std::string timeStr = "Calculando...";

        if (totalElapsed > 2 && stats.uploadedFiles > 0) { // Esperar 2s per velocitat estable
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

    // Comprovar si el directori remot existeix
    SetWindowTextA(hStatusLabel, "Verificando directorio remoto...");
    if (!uploader.RemoteDirectoryExists(g_config.remoteDir)) {
        std::string msg = "El directorio remoto '" + g_config.remoteDir + "' no existe.\n¿Desea crearlo?";
        int ret = MessageBoxA(hMainWindow, msg.c_str(), "Directorio no encontrado", MB_YESNO | MB_ICONQUESTION);
        if (ret == IDNO) {
            SetWindowTextA(hStatusLabel, "Subida cancelada.");
            EnableWindow(hUploadBtn, TRUE);
            return 0;
        }
        // Si SÍ, UploadDirectory el crearà (utilitza mkdir -p)
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
        // Neteja
        std::filesystem::remove_all(tempDir);
        g_selectedZip = ""; // Reiniciar selecció després de la pujada
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
            
            // Alternar caràcter de contrasenya
            // Utilitzar EM_SETPASSWORDCHAR és més simple que canviar estils al vol
            // El caràcter de contrasenya per defecte sol ser una vinyeta (0x25CF) o asterisc
            // Però si només volem mostrar/amagar, 0 l'elimina.
            // Per restaurar, podem utilitzar '*' o simplement reiniciar l'estil?
            // Provem eliminant/afegint l'estil ES_PASSWORD.
            
            /*
            LONG style = GetWindowLong(hPass, GWL_STYLE);
            if (checked) style &= ~ES_PASSWORD;
            else style |= ES_PASSWORD;
            SetWindowLong(hPass, GWL_STYLE, style);
            SetWindowPos(hPass, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            InvalidateRect(hPass, NULL, TRUE);
            */
            
            // Enfocament més simple:
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
