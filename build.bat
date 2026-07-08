@echo off
chcp 65001 >nul
echo ========================================
echo 快递运费结算系统 - 构建脚本
echo ========================================

set QT_DIR=E:\Qt\6.12.0\mingw_64
set CMAKE_DIR=E:\Qt\Tools\CMake_64\bin
set NINJA_DIR=E:\Qt\Tools\Ninja
set MINGW_DIR=E:\Qt\Tools\mingw1310_64\bin
set QXLSX_URL=https://github.com/QtExcel/QXlsx/archive/refs/tags/1.4.4.zip

set PATH=%CMAKE_DIR%;%NINJA_DIR%;%MINGW_DIR%;%QT_DIR%\bin;%PATH%

echo [1/4] 下载QXlsx库...
if not exist "QXlsx" (
    echo 正在下载QXlsx 1.4.4...
    powershell -Command "Invoke-WebRequest -Uri '%QXLSX_URL%' -OutFile 'QXlsx.zip'"
    if %errorlevel% neq 0 (
        echo 下载失败，请手动下载QXlsx并解压到QXlsx目录
        pause
        exit /b 1
    )
    powershell -Command "Expand-Archive -Path 'QXlsx.zip' -DestinationPath '.' -Force"
    ren QXlsx-1.4.4 QXlsx
    del QXlsx.zip
    echo QXlsx下载完成
) else (
    echo QXlsx已存在，跳过下载
)

echo.
echo [2/4] 配置CMake...
mkdir build 2>nul
cd build

cmake .. -G "Ninja" ^
    -DCMAKE_PREFIX_PATH=%QT_DIR% ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=%MINGW_DIR%\gcc.exe ^
    -DCMAKE_CXX_COMPILER=%MINGW_DIR%\g++.exe

if %errorlevel% neq 0 (
    echo CMake配置失败
    pause
    exit /b 1
)
echo CMake配置成功

echo.
echo [3/4] 编译项目...
ninja

if %errorlevel% neq 0 (
    echo 编译失败
    pause
    exit /b 1
)
echo 编译成功

echo.
echo [4/4] 打包程序...
mkdir dist 2>nul
copy ExpressFreightCalculator.exe dist\运费QT.exe

echo 正在收集依赖库...
windeployqt --release --force dist\运费QT.exe

if %errorlevel% neq 0 (
    echo 依赖收集失败，尝试手动复制关键库...
    copy %QT_DIR%\bin\Qt6Core.dll dist\
    copy %QT_DIR%\bin\Qt6Gui.dll dist\
    copy %QT_DIR%\bin\Qt6Widgets.dll dist\
    copy %QT_DIR%\bin\Qt6Concurrent.dll dist\
    copy %MINGW_DIR%\libgcc_s_seh-1.dll dist\
    copy %MINGW_DIR%\libstdc++-6.dll dist\
    copy %MINGW_DIR%\libwinpthread-1.dll dist\
)

echo.
echo ========================================
echo 打包完成！
echo 输出目录: build\dist\
echo 主程序: 运费QT.exe
echo ========================================
pause