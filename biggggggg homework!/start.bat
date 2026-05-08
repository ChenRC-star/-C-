@echo off
setlocal
cd /d "%~dp0"
title Campus Book Trade Server

echo ============================================================
echo  Campus Book Trade Management System
echo ============================================================
echo.

if not exist "build\CampusBookTrade.exe" (
    echo [ERROR] build\CampusBookTrade.exe was not found.
    echo Please compile the project first.
    echo.
    pause
    exit /b 1
)

if not exist "data\books.csv" (
    echo [ERROR] data\books.csv was not found.
    echo Please keep the data folder with this bat file.
    echo.
    pause
    exit /b 1
)

if not exist "web\login.html" (
    echo [ERROR] web\login.html was not found.
    echo Please keep the web folder with this bat file.
    echo.
    pause
    exit /b 1
)

netstat -ano | findstr /R /C:":8080 .*LISTENING" >nul
if "%errorlevel%"=="0" (
    echo [INFO] Port 8080 is already in use.
    echo The server may already be running.
    echo.
    echo Open this URL in your browser:
    echo http://127.0.0.1:8080/login.html
    echo.
    start "" "http://127.0.0.1:8080/login.html"
    pause
    exit /b 0
)

echo Starting HTTP server...
echo.
echo URL:
echo http://127.0.0.1:8080/login.html
echo.
echo Test account:
echo student_id: 20260001
echo password:   123456
echo.
echo Press Ctrl+C to stop the server.
echo ============================================================
echo.

start "" "http://127.0.0.1:8080/login.html"
"build\CampusBookTrade.exe"

echo.
echo [INFO] Server exited.
echo If the window closed quickly before, check the message above.
echo.
pause
