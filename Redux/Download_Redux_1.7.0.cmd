@echo off
setlocal DisableDelayedExpansion
set "ROOT_URL=https://github.com/cheniska/Space-Rangers-Mods/releases/download/redux-v1.7.0"
set "DEST=%~dp0Universe_Redux_1.7.0"
where curl.exe >nul 2>&1
if errorlevel 1 (
    echo curl.exe is required.
    goto failed
)
if not exist "%DEST%\" mkdir "%DEST%"
pushd "%DEST%" || goto failed
call :download "Universe_Redux_1.7.0.exe" "365da12872e5961765289d33619685d836b9922554f0fa198fd77b22d60a2543" || goto download_failed
call :download "Universe_Redux_1.7.0-1.bin" "00877fdfd8eba282c9133f7d1f6b39c581e12b7340c920747cae6b5f7306ad94" || goto download_failed
call :download "Universe_Redux_1.7.0-2.bin" "a2c4339aaa9bf199f8b14a319e527d65b4f3a5965f0434780dcd4d6ff0e33796" || goto download_failed
call :download "Universe_Redux_1.7.0-3.bin" "750dda9f1dc95fd1095c578bdf43b1a7d0b78a4b019803c0d21eb0acf70261ac" || goto download_failed
call :download "Universe_Redux_1.7.0-4.bin" "2a682852a2dfc6b4aa1c7ca11d0e58246d8c394a8679419889c02c03a9c22ff5" || goto download_failed
call :download "Universe_Redux_1.7.0-5.bin" "3401480ad27e071478b17687bd8b57bf451a7724a52be9a12681cb9e40f640c5" || goto download_failed
call :download "Universe_Redux_1.7.0-6.bin" "46e7fb2e0d71e27ad114b622123d4d71813a23153308a648b4d83dcc9b9866ce" || goto download_failed
call :download "Universe_Redux_1.7.0-7.bin" "4be8c09eb00a3965b2c8bb4a34c444989483d262e88ea96fa8a2fb23fe9dc82d" || goto download_failed
call :download "Universe_Redux_1.7.0-8.bin" "8f5d50c2e567a346889596f34855eb56f72bd97ba74fa354a2146b2dba29ed1d" || goto download_failed
echo.
echo All files downloaded and verified. Directory:
echo "%DEST%"
echo Run Universe_Redux_1.7.0.exe to install.
popd
if not defined SRHD_NO_PAUSE pause
exit /b 0

:download
set "PART=%~1"
set "WANTED=%~2"
if exist "%PART%" (
    call :verify "%PART%" "%WANTED%"
    if not errorlevel 1 exit /b 0
    echo Existing file fails verification: "%PART%".
    echo Move it aside and run the downloader again.
    exit /b 1
)
echo Downloading %PART%...
if exist "%PART%.part" (
    call :verify "%PART%.part" "%WANTED%"
    if not errorlevel 1 goto rename
)
curl.exe --fail --location --retry 3 --retry-delay 2 --continue-at - --output "%PART%.part" "%ROOT_URL%/%PART%"
if errorlevel 1 exit /b 1
call :verify "%PART%.part" "%WANTED%"
if errorlevel 1 (
    echo Downloaded file fails SHA-256 verification: "%PART%.part".
    echo Move that partial file aside and retry.
    exit /b 1
)
:rename
if exist "%PART%" exit /b 1
ren "%PART%.part" "%PART%" || exit /b 1
exit /b 0

:verify
set "SRHD_SETUP_FILE=%~1"
set "SRHD_SETUP_HASH=%~2"
powershell.exe -NoProfile -NonInteractive -Command "$actual = (Get-FileHash -LiteralPath $env:SRHD_SETUP_FILE -Algorithm SHA256).Hash; if ($actual -ne $env:SRHD_SETUP_HASH) { exit 1 }"
exit /b %ERRORLEVEL%

:download_failed
popd
:failed
echo.
echo Download failed. Run this file again to continue.
if not defined SRHD_NO_PAUSE pause
exit /b 1
