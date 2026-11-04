@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-BWTL.ps1" -InstallRoot "%~dp0." %*
