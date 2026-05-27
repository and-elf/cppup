@echo off
REM cppup bootstrap script for Windows (MinGW / MSYS2 / w64devkit).
REM
REM Mirrors bootstrap.sh: compiles the slim cppup_bootstrap binary from
REM the source list in scripts\bootstrap_sources.txt, then optionally runs
REM `build` or `update` against the freshly-built binary.
REM
REM Usage (from a shell that has g++ and bash on PATH — Git Bash, MSYS2,
REM PowerShell with mingw installed, or cmd.exe with the same):
REM
REM     bootstrap.bat           - only build the slim binary
REM     bootstrap.bat build     - then run `cppup_bootstrap build`
REM     bootstrap.bat update    - then run `cppup_bootstrap update`
REM
REM Override the compiler via:   set CXX=x86_64-w64-mingw32-g++

setlocal enabledelayedexpansion

REM Pin CWD to the script's directory so paths resolve regardless of how
REM the script was invoked.
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
cd /d "%ROOT%" || exit /b 1

if "%CXX%"=="" set "CXX=g++"
set "BUILD_DIR=bootstrap_build"
set "BOOTSTRAP_BINARY=%BUILD_DIR%\cppup_bootstrap.exe"
set "MANIFEST=scripts\bootstrap_sources.txt"
set "CXXFLAGS=-std=c++26 -O2 -DCPPUP_SLIM -DCPPUP_VERSION=0.1.0"
set "INCLUDES=-Isrc/core/configuration -Isrc/core/cli -Isrc/core/cli/commands -Isrc/cli -Iinclude -Isrc"
REM Windows link line — same shared libs as bootstrap.sh minus -ldl
REM (no libdl on Windows; LoadLibrary lives in kernel32) plus the system
REM libs that OpenSSL on MinGW pulls in transitively. -lstdc++exp provides
REM std::__open_terminal / std::__write_to_terminal — std::print routes
REM terminal writes through these in GCC 15+ libstdc++ on Windows and they
REM live in the experimental lib, not libstdc++ proper.
set "LIBS=-lcppup_config -lsqlite3 -lcrypto -lpthread -lws2_32 -lcrypt32 -lstdc++exp"

echo === cppup Bootstrap (Windows) ===

REM 1. Prerequisites.
where %CXX% >nul 2>nul
if errorlevel 1 (
    echo [ERROR] C++ compiler '%CXX%' not found on PATH.
    exit /b 1
)
where ar >nul 2>nul
if errorlevel 1 (
    echo [ERROR] 'ar' not found on PATH ^(install MinGW binutils^).
    exit /b 1
)
where bash >nul 2>nul
if errorlevel 1 (
    echo [ERROR] 'bash' not found on PATH — required to run
    echo         scripts\amalgamate_configuration_header.sh.
    echo         Install Git for Windows or MSYS2.
    exit /b 1
)

REM 2. Amalgamate the configuration header (slim build #embed's it).
echo [INFO] Amalgamating configuration header...
bash scripts/amalgamate_configuration_header.sh >nul
if errorlevel 1 (
    echo [ERROR] amalgamate_configuration_header.sh failed.
    exit /b 1
)

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM 3. Parse manifest into the two object lists, compiling as we go.
REM Section state is carried across iterations via !section!.
set "CONFIG_OBJS="
set "MAIN_OBJS="
set "section="

for /f "usebackq eol=# tokens=* delims=" %%L in ("%MANIFEST%") do (
    set "line=%%L"
    REM Strip trailing CR if the file was checked out with CRLF.
    for /f "tokens=* delims=" %%T in ("!line!") do set "line=%%T"

    if not "!line!"=="" (
        set "first=!line:~0,1!"
        if "!first!"=="[" (
            set "section=!line:~1!"
            set "section=!section:]=!"
        ) else (
            REM Derive a unique .o name by replacing '/' with '_' and dropping '.cpp'.
            set "src=!line!"
            set "objbase=!src:/=_!"
            set "objbase=!objbase:.cpp=!"
            set "obj=%BUILD_DIR%\!objbase!.o"

            echo [INFO] Compiling !src!
            %CXX% %CXXFLAGS% -c "!src!" -o "!obj!" %INCLUDES%
            if errorlevel 1 (
                echo [ERROR] Failed to compile !src!
                exit /b 1
            )

            if "!section!"=="config" (
                set "CONFIG_OBJS=!CONFIG_OBJS! !obj!"
            ) else if "!section!"=="main" (
                set "MAIN_OBJS=!MAIN_OBJS! !obj!"
            ) else (
                echo [ERROR] Source !src! found outside [config]/[main] section.
                exit /b 1
            )
        )
    )
)

REM 4. Archive the config objects.
echo [INFO] Creating libcppup_config.a...
ar rcs %BUILD_DIR%\libcppup_config.a !CONFIG_OBJS!
if errorlevel 1 (
    echo [ERROR] ar failed.
    exit /b 1
)

REM 5. Link the slim binary.
echo [INFO] Linking %BOOTSTRAP_BINARY%...
%CXX% %CXXFLAGS% !MAIN_OBJS! -L%BUILD_DIR% -o %BOOTSTRAP_BINARY% %LIBS%
if errorlevel 1 (
    echo [ERROR] Link failed.
    exit /b 1
)

if exist %BOOTSTRAP_BINARY% (
    echo [INFO] Slim binary built: %BOOTSTRAP_BINARY%
) else (
    echo [ERROR] Linker reported success but %BOOTSTRAP_BINARY% is missing.
    exit /b 1
)

REM 6. Optional follow-up command.
if "%~1"=="" (
    echo [INFO] Next: '%BOOTSTRAP_BINARY% update' ^(prebuilt^) or '%BOOTSTRAP_BINARY% build' ^(from source^)
    exit /b 0
)
if /i "%~1"=="build" (
    echo [INFO] Running: %BOOTSTRAP_BINARY% build
    %BOOTSTRAP_BINARY% build
    exit /b !ERRORLEVEL!
)
if /i "%~1"=="update" (
    echo [INFO] Running: %BOOTSTRAP_BINARY% update
    %BOOTSTRAP_BINARY% update
    exit /b !ERRORLEVEL!
)
echo [ERROR] unknown command '%~1' ^(expected: build or update^)
exit /b 1
