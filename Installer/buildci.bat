@echo off
set PROJECT_DIR=%CD%
call "C:\Qt\5.2.1\msvc2010\bin\qtenv2.bat"
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
cd %PROJECT_DIR%
echo Create install bundle LampBox for windows
rmdir /S /Q build 2> nul
mkdir build
cd build
qmake ../../lampbox.pro -r -spec win32-msvc2010 "CONFIG+=release"
nmake /S /NOLOGO
echo -----END BUILD-----
copy release\lampbox.exe ..\qt\5.2.1\
for /f %%i in ('..\qt\5.2.1\lampbox.exe version') do set VERSION=%%i
makensis.exe ..\getstation.nsi
copy ..\getstation.exe ..\qt\5.2.1\
makensis.exe /DVERSION=%VERSION% ..\setupqt52.nsi
move Lamp*.exe ../
cd ..
echo -----END NSIS BUILD----
pause