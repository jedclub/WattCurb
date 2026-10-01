@echo off
rem WattCurb Build Artifact & Ephemeral Cache Cleaner for Windows
rem Safely purges build directories, compiler outputs, and temporary logs.

setlocal enabledelayedexpansion

cd /d "%~dp0"

if not exist "CMakeLists.txt" (
    echo [!] Error: Must be executed within the WattCurb repository tree.
    exit /b 1
)

echo ===================================================================
echo   WattCurb Build Artifacts Clean-up
echo ===================================================================

rem 1. Remove build directories
for %%d in (build build_* output output_*) do (
    if exist "%%d" (
        echo   Deleting directory: %%d
        rmdir /s /q "%%d" 2>nul
    )
)

rem 2. Clean tmp directory contents while keeping tmp folder
if exist "tmp" (
    echo   Cleaning tmp directory...
    del /f /q /s "tmp\*" 2>nul
    for /d %%p in ("tmp\*") do rmdir /s /q "%%p" 2>nul
)

rem 3. Remove loose CMake and compiler artifacts
if exist "CMakeCache.txt" del /f /q "CMakeCache.txt" 2>nul
if exist "CMakeFiles" rmdir /s /q "CMakeFiles" 2>nul
if exist "cmake_install.cmake" del /f /q "cmake_install.cmake" 2>nul
if exist "CTestTestfile.cmake" del /f /q "CTestTestfile.cmake" 2>nul
if exist "compile_commands.json" del /f /q "compile_commands.json" 2>nul

echo.
echo [+] Clean-up completed successfully.
endlocal
