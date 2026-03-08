@echo off
setlocal

set BUILD_DIR=%~dp0build
set CONFIG=Debug

if /i "%~1"=="compile" goto :compile
if /i "%~1"=="test"    goto :test
if /i "%~1"=="all"     goto :all

echo Usage: test.bat [compile^|test^|all]
echo   compile  - build all test targets
echo   test     - run all tests via CTest
echo   all      - compile then test
exit /b 1

:: -----------------------------------------------------------------------
:compile
echo [compile] Building test targets (config: %CONFIG%)...
cmake --build "%BUILD_DIR%" --config %CONFIG% ^
  --target test_intel_hex_decoder test_instructions_alu test_instructions_branch
if errorlevel 1 (
    echo [compile] BUILD FAILED
    exit /b 1
)
echo [compile] Done.
exit /b 0

:: -----------------------------------------------------------------------
:test
echo [test] Running tests (config: %CONFIG%)...
ctest --test-dir "%BUILD_DIR%" -C %CONFIG% --output-on-failure
if errorlevel 1 (
    echo [test] SOME TESTS FAILED
    exit /b 1
)
echo [test] All tests passed.
exit /b 0

:: -----------------------------------------------------------------------
:all
call "%~f0" compile
if errorlevel 1 exit /b 1
call "%~f0" test
exit /b %errorlevel%
