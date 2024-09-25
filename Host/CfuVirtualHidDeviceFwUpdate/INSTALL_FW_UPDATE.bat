@echo off
:: Goto current directory
cd /d "%~dp0"

pnputil /add-driver CfuVirtualHidDeviceFwUpdate.inf /install

pause
