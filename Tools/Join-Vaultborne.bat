@echo off
setlocal enabledelayedexpansion
title Vaultborne - Join

rem ---------------------------------------------------------------------------
rem Connects to someone else's listen server. Copy this file into the packaged
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
echo     VAULTBORNE - joining
echo   ============================================
echo.

set "HOSTIP="
set /p "HOSTIP=  Host's address: "
if "!HOSTIP!"=="" (
	echo.
	echo   No address given, nothing to join.
	echo.
	pause
	exit /b 1
)

set "NAME="
set /p "NAME=  Your name for the scoreboard (Enter to skip): "

set "URL=!HOSTIP!"
if not "!NAME!"=="" set "URL=!HOSTIP!?Name=!NAME!"

echo.
echo   Connecting to !HOSTIP! ...
echo   A black screen for a few seconds is normal while the level loads.
start "" "!GAME!" "!URL!" -windowed -resx=1600 -resy=900
exit /b 0
