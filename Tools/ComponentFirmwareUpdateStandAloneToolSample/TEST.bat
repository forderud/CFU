@echo off
:: Goto current directory
cd /d "%~dp0"


::FwUpdateCfu.exe version protocolCfgExample.cfg
::pause


FwUpdateCfu.exe update protocolCfgExample.cfg CfuVirtualHidDevice_MCU.offer.bin CfuVirtualHidDevice_MCU.payload.bin
pause
