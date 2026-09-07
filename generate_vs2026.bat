@echo off
setlocal

set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "GENERATOR=Visual Studio 18 2026"

echo Configuring VulkanEngine...
echo Project root: %PROJECT_ROOT%
echo Build directory: %BUILD_DIR%

cmake --help | findstr /C:"%GENERATOR%" >nul
if errorlevel 1 (
    echo Error: CMake does not list "%GENERATOR%" as an available generator.
    echo Run "cmake --help" to see the installed generator names.
    exit /b 1
)

if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    if errorlevel 1 (
        echo Error: Failed to create build directory.
        exit /b 1
    )
)

cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -G "%GENERATOR%" -A x64
if errorlevel 1 (
    echo Error: CMake configuration failed.
    exit /b 1
)

echo Generation successful.
echo Solution generated in: %BUILD_DIR%
