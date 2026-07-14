@echo off
REM ============================================================
REM Compila o projeto DuckFaceLLM em modo Release.
REM Pre-requisito: setup_dependencies.bat ja executado.
REM ============================================================

if "%VCPKG_ROOT%"=="" (
    echo [ERRO] Defina a variavel de ambiente VCPKG_ROOT apontando para a pasta do vcpkg.
    exit /b 1
)

if not exist build (
    mkdir build
)

cd build

cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [ERRO] Falha ao configurar o CMake.
    cd ..
    exit /b 1
)

cmake --build . --config Release
if errorlevel 1 (
    echo [ERRO] Falha ao compilar.
    cd ..
    exit /b 1
)

cd ..
echo.
echo Build concluido! Executavel em build\Release\DuckFaceLLM.exe
echo.
echo IMPORTANTE: baixe o llama-server.exe (llama.cpp) e coloque em bin\llama-server.exe
echo antes de rodar o app. Veja instrucoes em README.md
pause
