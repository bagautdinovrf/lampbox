@echo off
set PROJECT_DIR=%CD%
set PROJECT_DRIVE=%CD:~0,2%
call "C:\Qt53\5.3\msvc2010_opengl\bin\qtenv2.bat"
call "C:\Qt\Qt5.3.1\5.3\msvc2010_opengl\bin\qtenv2.bat"
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"
cd %PROJECT_DRIVE%
cd %PROJECT_DIR%
echo Create install bundle Lampmedia Player for windows
rmdir /S /Q build 2> nul
mkdir build
cd build
echo -----BEGIN BUILD LMPLAYER AND LMWATCHER------
qmake ../src/lampbox.pro -r -spec win32-msvc2010 "CONFIG+=release"
nmake /S /NOLOGO
echo -----END BUILD-----
echo -----CREATE BUNDLE-----
rmdir /S /Q ..\winbundle 2> nul
mkdir ..\winbundle
copy release\lampbox.exe ..\winbundle\
cd ..\winbundle\
windeployqt --concurrent -opengl -multimediawidgets lampbox.exe
echo -----PATCHING---------
for /f %%i in ('qtpaths --binaries-dir') do set PATHTOBIN=%%i
rem Qt bug fix
copy %PATHTOBIN%\Qt5MultimediaQuick_p.dll .
echo ------END-------------
cd ..\windows\
for /f %%i in ('..\winbundle\lampbox.exe version') do set VERSION=%%i
makensis.exe getstation.nsi
copy getstation.exe ..\winbundle
makensis.exe /DVERSION=%VERSION% setupqt52.nsi
move Lamp*.exe ../
cd ..
echo -----END NSIS BUILD----
pause