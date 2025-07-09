@echo off
setlocal

REM --- Configuration ---
set ESPTOOL_PY_PATH=esptool.py
set MKLITTLEFS_PATH=tools\mklittlefs.exe
set PARTITIONS_CSV_PATH=partitions\partitions.csv
set DATA_DIR=data
set IMAGE_NAME=littlefs_image.bin
set PARTITION_LABEL=storage
set COM_PORT=COM5
set ESP_CHIP=esp32-s3
set BAUD_RATE=921600
set PAGE_SIZE=256
set BLOCK_SIZE=4096

echo ############################################################
echo # LittleFS Image Creation and Deployment Script (Simplified) #
echo ############################################################
echo.

REM --- 1. Find Partition Offset and Size from partitions.csv ---
echo [+] Reading partition info for '%PARTITION_LABEL%' from '%PARTITIONS_CSV_PATH%'...
set PARTITION_OFFSET=
set PARTITION_SIZE=

for /F "tokens=1,2,3,4,5 delims=, " %%a in ('findstr /B /I /C:"%PARTITION_LABEL%," "%PARTITIONS_CSV_PATH%"') do (
    if /I "%%a"=="%PARTITION_LABEL%" (
        set PARTITION_OFFSET=%%d
        set PARTITION_SIZE=%%e
    )
)

if not defined PARTITION_OFFSET (
    echo ERROR: Partition label '%PARTITION_LABEL%' not found or offset missing in '%PARTITIONS_CSV_PATH%'.
    goto :error_exit
)
if not defined PARTITION_SIZE (
    echo ERROR: Partition label '%PARTITION_LABEL%' not found or size missing in '%PARTITIONS_CSV_PATH%'.
    goto :error_exit
)

REM Remove trailing comma if present
set PARTITION_OFFSET=%PARTITION_OFFSET:,=%
set PARTITION_SIZE=%PARTITION_SIZE:,=%

echo [+] Partition Offset: %PARTITION_OFFSET%
echo [+] Partition Size: %PARTITION_SIZE% (Hex)
echo.

REM --- 2. Create LittleFS image ---
echo [+] Creating LittleFS image '%IMAGE_NAME%' from directory '%DATA_DIR%'...
echo [+] Image size target: %PARTITION_SIZE%

if exist "%IMAGE_NAME%" del "%IMAGE_NAME%" >nul 2>nul

if not exist "%DATA_DIR%" (
    echo ERROR: Data directory "%DATA_DIR%" not found.
    goto :error_exit
)

echo Running: "%MKLITTLEFS_PATH%" -c "%DATA_DIR%" -s %PARTITION_SIZE% -p %PAGE_SIZE% -b %BLOCK_SIZE% "%IMAGE_NAME%"
"%MKLITTLEFS_PATH%" -c "%DATA_DIR%" -s %PARTITION_SIZE% -p %PAGE_SIZE% -b %BLOCK_SIZE% "%IMAGE_NAME%"
if %errorlevel% neq 0 (
    echo ERROR: Failed to create LittleFS image. Check mklittlefs output.
    goto :error_exit
)
if not exist "%IMAGE_NAME%" (
    echo ERROR: LittleFS image file "%IMAGE_NAME%" was not created.
    goto :error_exit
)
echo [+] LittleFS image created successfully: %IMAGE_NAME%
echo.

REM --- 3. Deploy LittleFS image ---
echo [+] Flashing '%IMAGE_NAME%' to offset %PARTITION_OFFSET% on %COM_PORT% at %BAUD_RATE% bps...
echo Running: python -m esptool --chip "%ESP_CHIP%" --port %COM_PORT% --baud %BAUD_RATE% write_flash %PARTITION_OFFSET% "%IMAGE_NAME%"
python -m esptool --chip "%ESP_CHIP%" --port "%COM_PORT%" --baud %BAUD_RATE% write_flash %PARTITION_OFFSET% "%IMAGE_NAME%"

if %errorlevel% neq 0 (
    echo ERROR: Failed to flash LittleFS image. Check esptool.py output.
    goto :error_exit
)

echo [+] LittleFS image flashed successfully!
echo.
echo ############################################################
echo # Script Finished                                        #
echo ############################################################
goto :eof

:error_exit
echo.
echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
echo ! SCRIPT TERMINATED DUE TO AN ERROR                      !
echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
exit /b 1
