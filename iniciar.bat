@echo off
title Simulador y Descifrador Enigma (C++17)
cd /d "%~dp0"
enigma.exe %*
if "%~1"=="" pause
