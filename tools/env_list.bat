@echo off
echo ESP-IDF Environment Variables:
echo =============================

echo.
echo IDF_PATH: %IDF_PATH%
echo.

echo ADF_PATH: %ADF_PATH%
echo.

echo IDF_PYTHON_ENV_PATH: %IDF_PYTHON_ENV_PATH%
echo.

echo IDF_TOOLS_PATH: %IDF_TOOLS_PATH%
echo.

echo PYTHON: %PYTHON%
echo.

echo Searching for all ESP-IDF related variables:
echo -------------------------------------------
setlocal enabledelayedexpansion
for /f "tokens=1* delims==" %%a in ('set') do (
    set "var=%%a"
    if "!var:IDF=!" neq "!var!" (
        echo %%a = %%b
    )
)

echo.
echo PATH components:
echo --------------
for %%i in ("%PATH:;=" "%") do (
    set "item=%%~i"
    if "!item:IDF=!" neq "!item!" (
        echo %%i
    )
)

echo.
echo =============================
