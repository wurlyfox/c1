@echo off
rem Rebuild c1 on Windows using MSYS2 MinGW32 (see README.md, "Windows (MSYS2)").
rem
rem   rebuild.bat          build whatever changed
rem   rebuild.bat clean    remove all build output first, then build everything
rem   rebuild.bat test     build and run the unit tests
rem
rem MSYS2 is expected in C:\msys64; set MSYS2_ROOT to use another location.

setlocal
if not defined MSYS2_ROOT set "MSYS2_ROOT=C:\msys64"
set "BASH=%MSYS2_ROOT%\usr\bin\bash.exe"

if not exist "%BASH%" (
  echo MSYS2 was not found at %MSYS2_ROOT%. Install it, or set MSYS2_ROOT.
  set "RESULT=1"
  goto done
)

set "TARGET="
if /i "%~1"=="clean" set "TARGET=clean"
if /i "%~1"=="test" set "TARGET=test"
if not "%~1"=="" if not defined TARGET (
  echo Unknown option "%~1". Use: rebuild.bat [clean^|test]
  set "RESULT=1"
  goto done
)

rem run make in a MINGW32 login shell, from this script's directory
set "MSYSTEM=MINGW32"
set "CHERE_INVOKING=1"
pushd "%~dp0"
if "%TARGET%"=="clean" (
  "%BASH%" -lc "export OS=Windows_NT; make clean; make"
) else (
  "%BASH%" -lc "export OS=Windows_NT; make %TARGET%"
)
set "RESULT=%ERRORLEVEL%"
popd

echo.
if "%RESULT%"=="0" (
  if "%TARGET%"=="test" (echo Tests passed.) else (echo Build succeeded: build\c1.exe)
) else (
  echo Failed with exit code %RESULT%.
)

:done
rem keep the window open when started by double-clicking
echo %cmdcmdline% | find /i "%~0" >nul && pause
exit /b %RESULT%
