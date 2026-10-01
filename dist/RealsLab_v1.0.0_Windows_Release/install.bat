@echo off
chcp 65001 >nul
echo ========================================================
echo       REALS LAB EXTENSION - INSTALLER CHO REAPER
echo ========================================================
set TARGET_DIR=%APPDATA%\REAPER\UserPlugins
if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"
echo Đang sao chép reaper_realslab.dll vào %TARGET_DIR%...
copy /Y "%~dp0reaper_realslab.dll" "%TARGET_DIR%\reaper_realslab.dll" >nul
if %ERRORLEVEL% equ 0 (
    echo [THÀNH CÔNG] Đã cài đặt Reals Lab vào REAPER!
    echo Bạn hãy mở REAPER, vào Actions -^> Show action list -^> tìm 'Reals Lab: Show Window'
) else (
    echo [LỖI] Không thể copy file. Hãy chắc chắn rằng REAPER đã được đóng trước khi cài đặt.
)
pause
