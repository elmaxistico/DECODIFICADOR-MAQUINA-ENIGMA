@echo off
title Simulador y Descifrador Enigma (C++17)
cd /d "%~dp0"
if exist "%~dp0enigma_clean.exe" (
    "%~dp0enigma_clean.exe" %*
) else (
    "%~dp0enigma.exe" %*
)
if "%~1"=="" pause
