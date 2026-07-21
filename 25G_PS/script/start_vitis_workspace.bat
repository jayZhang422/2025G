@echo off
setlocal

set "VITIS_BIN=D:\FPGA\Xilinx\Vitis\2020.2\bin\vitis.bat"
if "%~1"=="" (
    set "WORKSPACE=%~dp0.."
) else (
    set "WORKSPACE=%~f1"
)

if not exist "%VITIS_BIN%" (
    echo ERROR: Vitis launcher was not found: %VITIS_BIN%
    pause
    exit /b 1
)

if not exist "%WORKSPACE%" (
    echo ERROR: Workspace was not found: %WORKSPACE%
    pause
    exit /b 1
)

echo Starting Vitis 2020.2 with workspace: %WORKSPACE%
call "%VITIS_BIN%" -workspace "%WORKSPACE%"
