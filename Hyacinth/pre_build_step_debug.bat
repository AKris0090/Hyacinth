@echo off
cd "C:\Users\ajnkr\Documents\Hyacinth\x64\Debug"
taskkill /f /im Hyacinth-Server.exe >nul 2>&1
set started=0
:waitloop
powershell -Command "if (netstat -an | Select-String '0.0.0.0:6767') { exit 0 } else { exit 1 }"
echo errorlevel: %errorlevel%
if errorlevel 1 (
    if %started%==0 (
        start ./Hyacinth-Server.exe
        set started=1
    )
    timeout /t 1 /nobreak >nul
    goto waitloop
)
pause