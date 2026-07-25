@echo off
setlocal

set "XSCT_BIN=D:\FPGA\Xilinx\Vitis\2020.2\bin\xsct.bat"
if not exist "%XSCT_BIN%" (
    echo ERROR: XSCT launcher was not found: %XSCT_BIN%
    exit /b 1
)

set "PS_AUTOMATION_STATUS_FILE=%TEMP%\ps_automation_%RANDOM%_%RANDOM%.status"
if exist "%PS_AUTOMATION_STATUS_FILE%" del /q "%PS_AUTOMATION_STATUS_FILE%"

call "%XSCT_BIN%" %*
if not exist "%PS_AUTOMATION_STATUS_FILE%" (
    echo ERROR: PS automation ended without a status result.
    exit /b 1
)

set /p "PS_AUTOMATION_RESULT="<"%PS_AUTOMATION_STATUS_FILE%"
del /q "%PS_AUTOMATION_STATUS_FILE%"
if /i not "%PS_AUTOMATION_RESULT%"=="PASS" exit /b 1
exit /b 0
