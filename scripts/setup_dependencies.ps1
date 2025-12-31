# OnlyAir - Dependency Setup Script
# Run this script to download and setup all required dependencies

param(
    [string]$FFmpegVersion = "6.1.1"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ThirdPartyDir = Join-Path $ProjectRoot "third_party"

Write-Host "OnlyAir Dependency Setup" -ForegroundColor Cyan
Write-Host "========================" -ForegroundColor Cyan
Write-Host ""

# Create directories
New-Item -ItemType Directory -Force -Path $ThirdPartyDir | Out-Null

# Download FFmpeg
Write-Host "Downloading FFmpeg $FFmpegVersion..." -ForegroundColor Yellow
$FFmpegUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n$FFmpegVersion-latest-win64-lgpl-shared.zip"
$FFmpegZip = Join-Path $ThirdPartyDir "ffmpeg.zip"
$FFmpegDir = Join-Path $ThirdPartyDir "ffmpeg"

if (-not (Test-Path $FFmpegDir)) {
    try {
        Invoke-WebRequest -Uri $FFmpegUrl -OutFile $FFmpegZip -UseBasicParsing
        Expand-Archive -Path $FFmpegZip -DestinationPath $ThirdPartyDir -Force

        # Find and rename extracted folder
        $ExtractedDir = Get-ChildItem -Path $ThirdPartyDir -Directory | Where-Object { $_.Name -like "ffmpeg-*" } | Select-Object -First 1
        if ($ExtractedDir) {
            Rename-Item -Path $ExtractedDir.FullName -NewName "ffmpeg"
        }

        Remove-Item $FFmpegZip -Force
        Write-Host "FFmpeg downloaded successfully!" -ForegroundColor Green
    }
    catch {
        Write-Host "Failed to download FFmpeg. Please download manually from:" -ForegroundColor Red
        Write-Host "https://github.com/BtbN/FFmpeg-Builds/releases" -ForegroundColor White
        Write-Host "Extract to: $FFmpegDir" -ForegroundColor White
    }
}
else {
    Write-Host "FFmpeg already exists, skipping..." -ForegroundColor Gray
}

# VB-Cable installer (optional)
Write-Host ""
Write-Host "VB-Cable Virtual Audio:" -ForegroundColor Yellow
$VBCableDir = Join-Path $ThirdPartyDir "vbcable"

if (-not (Test-Path $VBCableDir)) {
    New-Item -ItemType Directory -Force -Path $VBCableDir | Out-Null
}

if (-not (Test-Path (Join-Path $VBCableDir "VBCABLE_Setup_x64.exe"))) {
    Write-Host "Note: VB-Cable must be downloaded manually due to license restrictions." -ForegroundColor Cyan
    Write-Host "Download from: https://vb-audio.com/Cable/" -ForegroundColor White
    Write-Host "Place VBCABLE_Setup_x64.exe in: $VBCableDir" -ForegroundColor White
}
else {
    Write-Host "VB-Cable installer found." -ForegroundColor Green
}

Write-Host ""
Write-Host "========================" -ForegroundColor Cyan
Write-Host "Setup complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Prerequisites:" -ForegroundColor Yellow
Write-Host "1. Visual Studio 2022 with C++ workload"
Write-Host "2. Qt 6.x (Core, Widgets, OpenGL, OpenGLWidgets)"
Write-Host ""
Write-Host "Build steps:" -ForegroundColor Yellow
Write-Host "1. Set Qt6_DIR environment variable or pass to CMake"
Write-Host "2. Run: cmake -B build -G `"Visual Studio 17 2022`" -A x64 -DQt6_DIR=`"C:/Qt/6.x/msvc2022_64/lib/cmake/Qt6`""
Write-Host "3. Run: cmake --build build --config Release --target OnlyAir"
Write-Host ""
