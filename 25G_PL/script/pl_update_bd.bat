@echo off
setlocal

if not defined VIVADO_BIN set "VIVADO_BIN=D:\FPGA\Xilinx\Vivado\2020.2\bin\vivado.bat"
if not exist "%VIVADO_BIN%" (
    echo ERROR: Vivado launcher was not found: %VIVADO_BIN%
    echo Set VIVADO_BIN to your Vivado 2020.2 vivado.bat path and run again.
    pause
    exit /b 1
)

echo Starting PL Block Design update...
if "%~1"=="" (
    call "%VIVADO_BIN%" -mode batch -source "%~dp0pl_update_bd.tcl"
) else (
    call "%VIVADO_BIN%" -mode batch -source "%~dp0pl_update_bd.tcl" -tclargs %*
)
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" echo FAILED: PL Block Design update returned %RESULT%.
pause
exit /b %RESULT%
