@echo off
:: Goto current directory
cd /d "%~dp0"

:: Trust driver certificate
certutil.exe -addstore root CfuVirtualHid.cer
certutil.exe -f -addstore trustedpublisher CfuVirtualHid.cer

:: Install virtual FW driver and create HW instance
devcon.exe install cfuvirtualhid.inf HID\CFU_VIRTUAL_DEVICE

pause
