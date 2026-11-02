@echo off
setlocal enabledelayedexpansion
title Adrenaline - Host

rem ---------------------------------------------------------------------------
rem Starts a listen server on the test course. Copy this file into the packaged
rem build, next to Getaway.exe (or next to the Windows folder) and double-click
rem it. See *Playing with friends on a LAN* in docs/multiplayer.md.
rem ---------------------------------------------------------------------------

set "GAME=%~dp0Getaway.exe"
if not exist "!GAME!" set "GAME=%~dp0Windows\Getaway.exe"
if not exist "!GAME!" (
	echo.
	echo   Could not find Getaway.exe.
	echo.
	echo   Put this file in the same folder as Getaway.exe, or in the folder
	echo   that contains the Windows folder.
	echo.
	pause
	exit /b 1
)

echo.
echo   ============================================
echo     ADRENALINE - hosting
echo   ============================================
echo.
echo   Read one of these addresses to your friends:
echo.
set "FOUND="
for /f "tokens=2 delims=:" %%A in ('ipconfig ^| findstr /c:"IPv4 Address"') do (
	set "IP=%%A"
	echo       !IP: =!
	set "FOUND=1"
)
if not defined FOUND echo       (none found - run ipconfig in a terminal and read the IPv4 address^)
echo.
echo   Same house: use the 192.168.x.x or 10.x.x.x one.
echo   Different cities: use your Tailscale / ZeroTier / Radmin address, or
echo   forward UDP port 7777 on your router first.
echo.
echo   Windows will ask to allow Getaway through the firewall. Say yes, and
echo   tick Private networks.
echo.

set "NAME="
set /p "NAME=  Your name for the scoreboard (Enter to skip): "

set "OPTS="
if not "!NAME!"=="" set "OPTS=?Name=!NAME!"

echo.
echo   Starting...
start "" "!GAME!" "/Game/Maps/TestCourse?listen!OPTS!" -windowed -resx=1600 -resy=900
exit /b 0
