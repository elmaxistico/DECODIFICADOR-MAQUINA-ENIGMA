@echo off
set SCRIPT="%TEMP%\CreateShortcut.vbs"
set TARGET=%~dp0iniciar.bat
set SHORTCUT=%USERPROFILE%\Desktop\Enigma Machine.lnk

echo Set oWS = WScript.CreateObject("WScript.Shell") > %SCRIPT%
echo sLinkFile = "%SHORTCUT%" >> %SCRIPT%
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> %SCRIPT%
echo oLink.TargetPath = "%TARGET%" >> %SCRIPT%
echo oLink.WorkingDirectory = "%~dp0" >> %SCRIPT%
echo oLink.Description = "Simulador y Descifrador de Enigma" >> %SCRIPT%
echo oLink.Save >> %SCRIPT%

cscript //nologo %SCRIPT%
if exist %SCRIPT% del %SCRIPT%

echo ============================================================
echo   Acceso directo creado con exito en tu Escritorio!
echo   Ubicacion: "%SHORTCUT%"
echo ============================================================
pause
