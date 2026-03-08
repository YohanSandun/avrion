@echo off
setlocal

set BUILD_DIR=%~dp0build
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

:: ---- usage -----------------------------------------------------------------
:usage
echo Usage: build.bat [cli^|gui] [--run] [--config Debug^|Release]
echo   cli         - build the CLI application (avr_cli)
echo   gui         - build the GUI application (avr_gui)
echo   --run       - run the executable after a successful build
echo   --config    - build configuration (default: Debug)
echo.
echo Examples:
echo   build.bat cli
echo   build.bat gui --run
echo   build.bat cli --run --config Release
exit /b 1
