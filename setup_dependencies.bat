@echo off
REM ============================================================
REM Instala todas as dependencias do projeto via vcpkg.
REM Pre-requisito: ter o vcpkg clonado e "bootstrapped".
REM   git clone https://github.com/microsoft/vcpkg.git
REM   cd vcpkg && bootstrap-vcpkg.bat
REM ============================================================

if "%VCPKG_ROOT%"=="" (
    echo [ERRO] Defina a variavel de ambiente VCPKG_ROOT apontando para a pasta do vcpkg.
    echo Exemplo: set VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

"%VCPKG_ROOT%\vcpkg.exe" install ^
    curl:x64-windows ^
    glfw3:x64-windows ^
    "imgui[glfw-binding,opengl3-binding]:x64-windows" ^
    nlohmann-json:x64-windows ^
    zlib:x64-windows

echo.
echo Dependencias instaladas com sucesso.
echo Agora rode build.bat para compilar o projeto.
pause
