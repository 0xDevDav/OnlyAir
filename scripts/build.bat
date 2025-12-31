@echo off
setlocal enabledelayedexpansion

echo.
echo ==========================================
echo OnlyAir Build Script
echo ==========================================
echo.

:: Check for Visual Studio
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: Visual Studio compiler not found!
    echo Please run this script from a Visual Studio Developer Command Prompt
    echo or run vcvarsall.bat first.
    exit /b 1
)

:: Set directories
set "PROJECT_ROOT=%~dp0.."
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "FFMPEG_DIR=%PROJECT_ROOT%\third_party\ffmpeg"

:: Check for FFmpeg
if not exist "%FFMPEG_DIR%\include\libavcodec\avcodec.h" (
    echo Error: FFmpeg not found!
    echo Please run scripts\setup_dependencies.ps1 first.
    exit /b 1
)

:: Check for Qt6
if "%Qt6_DIR%"=="" (
    echo Warning: Qt6_DIR not set. CMake will try to find Qt automatically.
)

echo Project root: %PROJECT_ROOT%
echo Build directory: %BUILD_DIR%
echo FFmpeg directory: %FFMPEG_DIR%
echo.

:: Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

:: Configure with CMake
echo Configuring with CMake...
cmake -B "%BUILD_DIR%" -S "%PROJECT_ROOT%" -DFFMPEG_DIR="%FFMPEG_DIR%" -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    exit /b 1
)

:: Build
echo.
echo Building Release configuration...
cmake --build "%BUILD_DIR%" --config Release --target OnlyAir
if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo.
echo ==========================================
echo Build completed successfully!
echo ==========================================
echo.
echo Output files:
echo   - %BUILD_DIR%\bin\Release\OnlyAir.exe
echo   - %BUILD_DIR%\bin\Release\softcam.dll
echo.

endlocal
