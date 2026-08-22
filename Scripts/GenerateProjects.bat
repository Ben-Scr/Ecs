@echo off
setlocal
pushd "%~dp0.."

set "PREMAKE=%CD%\Vendor\Bin\premake5.exe"

if not exist "%PREMAKE%" (
    echo premake5.exe was not found at:
    echo %PREMAKE%
    popd
    exit /b 1
)

set "ACTION=vs2022"

if "%~1"=="" goto run_default

set "FIRST=%~1"
if "%FIRST:~0,2%"=="--" goto run_default

rem First argument is an explicit Premake action, so pass all arguments unchanged.
"%PREMAKE%" %*
goto finish

:run_default
"%PREMAKE%" %ACTION% %*

:finish
set "RESULT=%errorlevel%"
popd
exit /b %RESULT%
