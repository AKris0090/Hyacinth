@echo off
cd "C:\Users\ajnkr\Documents\Hyacinth\x64\Release"
set started=0
:waitloop
powershell -Command "netstat -an | Select-String '0.0.0.0:6767'" >nul 2>&1
if errorlevel 1 (
    if %started%==0 (
        start ./Hyacinth-Server.exe
        set started=1
    )
    timeout /t 1 /nobreak >nul
    goto waitloop
)