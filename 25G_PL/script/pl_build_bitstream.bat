@echo off
setlocal

if not defined VIVADO_BIN set "VIVADO_BIN=D:\FPGA\Xilinx\Vivado\2020.2\bin\vivado.bat"
if not exist "%VIVADO_BIN%" (
    echo ERROR: Vivado launcher was not found: %VIVADO_BIN%
    echo Set VIVADO_BIN to your Vivado 2020.2 vivado.bat path and run again.
    pause
    exit /b 1
)

echo Starting PL bitstream build with the TCL default of 24 worker threads...
echo When the existing bitstream is current: 1 rebuilds all outputs, 0 keeps current files.
echo Optional arguments: --rebuild, --keep, --threads N, or --check.
if "%~1"=="" (
    call "%VIVADO_BIN%" -mode batch -source "%~dp0pl_build_bitstream.tcl"
) else (
    call "%VIVADO_BIN%" -mode batch -source "%~dp0pl_build_bitstream.tcl" -tclargs %*
)
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" echo FAILED: PL bitstream build returned %RESULT%.
pause
exit /b %RESULT%
