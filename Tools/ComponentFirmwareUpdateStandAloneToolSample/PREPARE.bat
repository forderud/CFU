@echo off
:: Goto current directory
cd /d "%~dp0"


:: Copy in FW files
copy ..\..\Host\CfuVirtualHidDeviceFwUpdate\*.bin .

:: Copy in tool
copy x64\Debug\FwUpdateCfu.exe .
