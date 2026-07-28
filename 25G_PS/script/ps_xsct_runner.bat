@echo off
setlocal

set "XSCT_BIN=D:\FPGA\Xilinx\Vitis\2020.2\bin\xsct.bat"
if not exist "%XSCT_BIN%" (
    echo ERROR: XSCT launcher was not found: %XSCT_BIN%
    exit /b 1
)

for /f %%I in ('powershell.exe -NoLogo -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss_fff"') do set "PS_AUTOMATION_RUN_ID=%%I"
if not defined PS_AUTOMATION_RUN_ID set "PS_AUTOMATION_RUN_ID=%RANDOM%_%RANDOM%"

set "PS_AUTOMATION_LOG_DIR=%~dp0..\.Xil\ps_automation_logs"
if not exist "%PS_AUTOMATION_LOG_DIR%" mkdir "%PS_AUTOMATION_LOG_DIR%" >nul 2>&1
if not exist "%PS_AUTOMATION_LOG_DIR%" (
    echo ERROR: Could not create PS automation log directory: %PS_AUTOMATION_LOG_DIR%
    exit /b 1
)

set "PS_AUTOMATION_LOG_FILE=%PS_AUTOMATION_LOG_DIR%\%~n1_%PS_AUTOMATION_RUN_ID%_%RANDOM%.log"
set "PS_AUTOMATION_STATUS_FILE=%TEMP%\ps_automation_%PS_AUTOMATION_RUN_ID%_%RANDOM%.status"
if exist "%PS_AUTOMATION_STATUS_FILE%" del /q "%PS_AUTOMATION_STATUS_FILE%"

echo Running PS automation. Output will be shown when XSCT exits.
echo Log file: %PS_AUTOMATION_LOG_FILE%
pushd "%PS_AUTOMATION_LOG_DIR%" >nul
call "%XSCT_BIN%" %* >"%PS_AUTOMATION_LOG_FILE%" 2>&1
set "XSCT_RESULT=%ERRORLEVEL%"
popd >nul
type "%PS_AUTOMATION_LOG_FILE%"
echo Log retained: %PS_AUTOMATION_LOG_FILE%

if not exist "%PS_AUTOMATION_STATUS_FILE%" (
    echo ERROR: PS automation ended without a status result. XSCT exit code: %XSCT_RESULT%
    exit /b 1
)

set /p "PS_AUTOMATION_RESULT="<"%PS_AUTOMATION_STATUS_FILE%"
del /q "%PS_AUTOMATION_STATUS_FILE%"
if /i not "%PS_AUTOMATION_RESULT%"=="PASS" exit /b 1
if not "%XSCT_RESULT%"=="0" (
    echo ERROR: XSCT returned exit code %XSCT_RESULT% despite a PASS status.
    exit /b 1
)
exit /b 0
