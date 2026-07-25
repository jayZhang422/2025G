@echo off
call "%~dp0ps_xsct_runner.bat" "%~dp0ps_update_platform_and_build.tcl" %*
exit /b %ERRORLEVEL%
