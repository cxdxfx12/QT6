# Express Freight Calculator - PowerShell Build Script
# Using Qt 6.12.0 + MinGW 64-bit

$QT_DIR = "E:\Qt\6.12.0\mingw_64"
$CMAKE_DIR = "E:\Qt\Tools\CMake_64\bin"
$NINJA_DIR = "E:\Qt\Tools\Ninja"
$MINGW_DIR = "E:\Qt\Tools\mingw1310_64\bin"
$QXLSX_URL = "https://github.com/QtExcel/QXlsx/archive/refs/tags/1.4.4.zip"
$QXLSX_TAG = "1.4.4"

$env:PATH = "$CMAKE_DIR;$NINJA_DIR;$MINGW_DIR;$QT_DIR\bin;$env:PATH"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Express Freight Calculator Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# 1. Download QXlsx
Write-Host ""
Write-Host "[1/4] Downloading QXlsx library..." -ForegroundColor Yellow

if (-not (Test-Path "QXlsx")) {
    Write-Host "Downloading QXlsx $QXLSX_TAG..."
    
    try {
        Invoke-WebRequest -Uri $QXLSX_URL -OutFile "QXlsx.zip" -ErrorAction Stop
        Write-Host "Download completed, extracting..."
        
        Expand-Archive -Path "QXlsx.zip" -DestinationPath "." -Force
        Rename-Item -Path "QXlsx-$QXLSX_TAG" -NewName "QXlsx" -Force
        Remove-Item -Path "QXlsx.zip" -Force
        
        Write-Host "QXlsx download completed" -ForegroundColor Green
    }
    catch {
        Write-Host "Download failed. Please download QXlsx manually and extract to QXlsx folder" -ForegroundColor Red
        Write-Host "Error: $_" -ForegroundColor Red
        Read-Host "Press any key to exit"
        exit 1
    }
} else {
    Write-Host "QXlsx already exists, skipping download" -ForegroundColor Gray
}

# 2. Configure CMake
Write-Host ""
Write-Host "[2/4] Configuring CMake..." -ForegroundColor Yellow

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}
Set-Location "build"

cmake .. -G "Ninja" `
    -DCMAKE_PREFIX_PATH="$QT_DIR" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER="$MINGW_DIR\gcc.exe" `
    -DCMAKE_CXX_COMPILER="$MINGW_DIR\g++.exe"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed" -ForegroundColor Red
    Read-Host "Press any key to exit"
    exit 1
}
Write-Host "CMake configuration succeeded" -ForegroundColor Green

# 3. Build Project
Write-Host ""
Write-Host "[3/4] Building project..." -ForegroundColor Yellow

ninja

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed" -ForegroundColor Red
    Read-Host "Press any key to exit"
    exit 1
}
Write-Host "Build succeeded" -ForegroundColor Green

# 4. Package
Write-Host ""
Write-Host "[4/4] Packaging application..." -ForegroundColor Yellow

if (-not (Test-Path "dist")) {
    New-Item -ItemType Directory -Path "dist" | Out-Null
}

Copy-Item "WukongFreight.exe" "dist\WukongFreight.exe" -Force

Write-Host "Collecting dependencies..." -ForegroundColor Yellow

windeployqt --release --force "dist\WukongFreight.exe"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Dependency collection failed, copying manually..." -ForegroundColor Red
    Copy-Item "$QT_DIR\bin\Qt6Core.dll" "dist\" -Force
    Copy-Item "$QT_DIR\bin\Qt6Gui.dll" "dist\" -Force
    Copy-Item "$QT_DIR\bin\Qt6Widgets.dll" "dist\" -Force
    Copy-Item "$QT_DIR\bin\Qt6Concurrent.dll" "dist\" -Force
    Copy-Item "$MINGW_DIR\libgcc_s_seh-1.dll" "dist\" -Force
    Copy-Item "$MINGW_DIR\libstdc++-6.dll" "dist\" -Force
    Copy-Item "$MINGW_DIR\libwinpthread-1.dll" "dist\" -Force
}

$QXLSX_BUILD = "QXlsx\build\release\QXlsx.dll"
if (Test-Path $QXLSX_BUILD) {
    Copy-Item $QXLSX_BUILD "dist\" -Force
} else {
    Write-Host "QXlsx.dll not found, please build QXlsx manually" -ForegroundColor Red
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Packaging completed!" -ForegroundColor Green
Write-Host "Output directory: build\dist\" -ForegroundColor White
Write-Host "Executable: WukongFreight.exe" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Cyan

Read-Host "Press any key to exit"