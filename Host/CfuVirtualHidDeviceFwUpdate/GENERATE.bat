@echo off
:: Goto current directory
cd /d "%~dp0"

:: Visual Studio command prompt
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"

:: Copy INX to INF
copy /y CfuVirtualHidDeviceFwUpdate.inx CfuVirtualHidDeviceFwUpdate.inf

:: Update INF with current time
stampinf.exe -f CfuVirtualHidDeviceFwUpdate.inf -d * -a amd64 -v *

:: Generate driver catalog (.CAT)
Inf2cat.exe /driver:. /os:"10_x64"

:: Sign CAT file
signtool.exe sign /a /fd sha256 CfuVirtualHidDeviceFwUpdate.cat

::pause
