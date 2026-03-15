@echo off
setlocal

set BUILD_DIR=%~dp0build
set WASM_BUILD_DIR=%~dp0build_wasm
set EMSDK_ENV=D:\Personal\emsdk\emsdk_env.bat
set CONFIG=Debug
set TARGET=
set EXE=
set RUN=0

:: ---- parse arguments -------------------------------------------------------
:parse_args
if "%~1"=="" goto :check_target
if /i "%~1"=="cli" (
    set TARGET=avr_cli
    set EXE=%BUILD_DIR%\apps\cli\%CONFIG%\avr_cli.exe
    shift & goto :parse_args
)
if /i "%~1"=="gui" (
    set TARGET=avr_gui
    set EXE=%BUILD_DIR%\apps\gui\%CONFIG%\avr_gui.exe
    shift & goto :parse_args
)
if /i "%~1"=="wasm" (
    set TARGET=wasm
    shift & goto :parse_args
)
if /i "%~1"=="--run" (
    set RUN=1
    shift & goto :parse_args
)
if /i "%~1"=="--config" (
    set CONFIG=%~2
    shift & shift & goto :parse_args
)

echo Unknown argument: %~1
goto :usage

:: ---- validate --------------------------------------------------------------
:check_target
if "%TARGET%"=="" goto :usage

:: ---- wasm build (separate path) --------------------------------------------
if /i "%TARGET%"=="wasm" goto :build_wasm

:: ---- build -----------------------------------------------------------------
echo [build] Building %TARGET% (config: %CONFIG%)...
cmake --build "%BUILD_DIR%" --config %CONFIG% --target %TARGET%
if errorlevel 1 (
    echo [build] BUILD FAILED
    exit /b 1
)
echo [build] Done.

:: ---- optional run ----------------------------------------------------------
if "%RUN%"=="1" (
    if not exist "%EXE%" (
        echo [run] Executable not found: %EXE%
        exit /b 1
    )
    echo [run] Running %EXE%...
    "%EXE%"
    exit /b %errorlevel%
)

exit /b 0

:: ---- wasm build ------------------------------------------------------------
:build_wasm
echo [wasm] Activating Emscripten environment...
if not exist "%EMSDK_ENV%" (
    echo [wasm] ERROR: emsdk_env.bat not found at %EMSDK_ENV%
    echo [wasm]        Update EMSDK_ENV in build.bat to match your installation.
    exit /b 1
)

echo [wasm] Configuring (emcmake cmake)...
cmd /c ""%EMSDK_ENV%" 2>nul && emcmake cmake -S "%~dp0." -B "%WASM_BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release"
if errorlevel 1 (
    echo [wasm] CMAKE CONFIGURE FAILED
    exit /b 1
)

echo [wasm] Building...
cmd /c ""%EMSDK_ENV%" 2>nul && cmake --build "%WASM_BUILD_DIR%" --config Release"
if errorlevel 1 (
    echo [wasm] BUILD FAILED
    exit /b 1
)

echo [wasm] Done. Output: apps\wasm\web\public\avrion_wasm.js + .wasm
exit /b 0

:: ---- usage -----------------------------------------------------------------
:usage
echo Usage: build.bat [cli^|gui^|wasm] [--run] [--config Debug^|Release]
echo   cli         - build the CLI application (avr_cli)
echo   gui         - build the GUI application (avr_gui)
echo   wasm        - build the WebAssembly simulator (requires Emscripten)
echo   --run       - run the executable after a successful build (cli/gui only)
echo   --config    - build configuration (default: Debug, ignored for wasm)
echo.
echo Examples:
echo   build.bat cli
echo   build.bat gui --run
echo   build.bat cli --run --config Release
echo   build.bat wasm
exit /b 1
