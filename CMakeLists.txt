cmake_minimum_required(VERSION 3.15)
project(DuckFaceLLM VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
endif()

if(MSVC)
    # Sem isso, o MSVC tenta interpretar os arquivos-fonte na code page do
    # sistema (ex: 1252) em vez de UTF-8, quebrando os literais Unicode
    # usados em Localization.h (chinês, acentos) e em símbolos como '●'.
    add_compile_options(/utf-8)
endif()

find_package(CURL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# WIN32 aqui é o que faz o executavel abrir como app GUI puro no Windows,
# sem nenhuma janela de console/terminal por tras dele.
add_executable(DuckFaceLLM WIN32
    src/main.cpp
    src/ModelDownloader.cpp
    src/LlamaServerManager.cpp
    src/HttpClient.cpp
)

if(WIN32)
    # Embute o icone do pato diretamente no .exe (taskbar, Alt+Tab, atalhos).
    target_sources(DuckFaceLLM PRIVATE resources/app.rc)
endif()

target_link_libraries(DuckFaceLLM PRIVATE
    CURL::libcurl
    glfw
    OpenGL::GL
    imgui::imgui
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
)

if(WIN32)
    target_link_libraries(DuckFaceLLM PRIVATE opengl32 crypt32 shell32 winmm ole32 sapi.lib) # crypt32: DPAPI (token HF) | shell32: ShellExecuteA (abrir log)
    # Garante que o entry point continue sendo main() (nao WinMain), mesmo
    # com o subsystem WINDOWS ativo via WIN32 acima.
    set_target_properties(DuckFaceLLM PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

add_custom_command(TARGET DuckFaceLLM POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:DuckFaceLLM>/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:DuckFaceLLM>/models
    # Sem isso, "tools\tune_model.ps1" nunca existe ao lado do .exe compilado
    # (so existia na arvore de codigo-fonte), fazendo o Auto-tune sempre
    # falhar com uma mensagem de erro enganosa sobre o llama-bench.exe.
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tools $<TARGET_FILE_DIR:DuckFaceLLM>/tools
)
