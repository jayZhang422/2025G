@echo off
call "%~dp0ps_xsct_runner.bat" "%~dp0ps_program_and_run.tcl" %*
exit /b %ERRORLEVEL%
