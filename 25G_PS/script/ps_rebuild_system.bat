@echo off
call "%~dp0ps_xsct_runner.bat" "%~dp0ps_rebuild_system.tcl" %*
exit /b %ERRORLEVEL%
