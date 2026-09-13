@echo off
rem Build FrostDock with the Visual Studio Developer Command Prompt (MSVC).
rem Run this in "x64 Native Tools Command Prompt for VS".
cl /nologo /std:c++17 /O2 /DUNICODE /D_UNICODE src\main.cpp ^
   /Fe:FrostDock.exe ^
   /link user32.lib gdi32.lib shell32.lib dwmapi.lib ^
   /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
