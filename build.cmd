@echo off
setlocal EnableExtensions EnableDelayedExpansion
if not defined PTK (
  echo Set PTK to the installed Creo 10 Common Files\protoolkit directory.
  exit /b 2
)
if not exist "%PTK%\includes\ProToolkit.h" exit /b 2
if not defined VSCMD_VER (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
  if not defined VSROOT exit /b 3
  call "!VSROOT!\VC\Auxiliary\Build\vcvars64.bat"
  if errorlevel 1 exit /b 3
)
pushd "%~dp0"
if not exist build mkdir build
if not exist bin mkdir bin
cl /nologo /MD /W3 /utf-8 /GS /DPRO_MACHINE=36 /DPRO_OS=4 /I"%PTK%\includes" /c src\assistant.c /Fobuild\assistant.obj
if errorlevel 1 exit /b 1
link /nologo /DLL /MACHINE:X64 /OUT:bin\BWTL.dll /IMPLIB:build\BWTL.lib build\assistant.obj "%PTK%\x86e_win64\obj\protk_dllmd_NU.lib" "%PTK%\x86e_win64\obj\ucore.lib" "%PTK%\x86e_win64\obj\udata.lib" kernel32.lib user32.lib wsock32.lib advapi32.lib mpr.lib winspool.lib netapi32.lib psapi.lib gdi32.lib shell32.lib comdlg32.lib ole32.lib ws2_32.lib
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
