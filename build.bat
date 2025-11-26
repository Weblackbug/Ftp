@echo off
echo Compiling FTP Uploader...

:: Setup MSVC Environment
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
if not exist "%VS_PATH%" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2019\Community"
if not exist "%VS_PATH%" set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
if not exist "%VS_PATH%" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community"

if exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" (
    call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Warning: Could not find vcvars64.bat in standard locations.
    echo Trying to use existing environment...
)

:: Check for CL (MSVC)
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: MSVC compiler 'cl.exe' not found in PATH.
    echo Please run this script from the 'Developer Command Prompt for VS'.
    exit /b 1
)

:: Compile Resources
rc resource.rc

:: Compile and Link
:: /EHsc: Enable C++ exceptions
:: /std:c++17: Use C++17 features (filesystem)
:: /Fe: Output filename
:: user32.lib, etc.: Link against Windows libraries
cl main.cpp ftp_uploader.cpp config_manager.cpp miniz.c resource.res /EHsc /std:c++17 /Fe:FtpUploader.exe user32.lib gdi32.lib comctl32.lib comdlg32.lib

if %errorlevel% equ 0 (
    echo Build successful! FtpUploader.exe created.
) else (
    echo Build failed.
)
