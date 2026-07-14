#include "LlamaServerManager.h"
#include <sstream>
#include <vector>
#include <iostream>
#include <filesystem>

LlamaServerManager::~LlamaServerManager() {
    if (m_running) Stop();
}

bool LlamaServerManager::Start(const std::string& modelPath, int port, int ctxSize,
                                int gpuLayers, const std::string& mmprojPath,
                                const std::string& extraArgs) {
    if (m_running) Stop();

    std::ostringstream cmd;
    cmd << "bin\\llama-server.exe"
        << " -m \"" << modelPath << "\""
        << " -c " << ctxSize
        << " --port " << port
        << " --host 127.0.0.1"
        << " -ngl " << gpuLayers      // usa GPU se disponivel; ignorado em builds CPU
        << " --jinja"                  // aplica o chat template embutido no GGUF (reduz alucinacao)
        << " --reasoning-format auto"; // separa reasoning_content quando o modelo suportar

    if (!mmprojPath.empty()) {
        // Habilita entrada de imagens (visao) para modelos multimodais
        // (llava, qwen2-vl, etc). Ignorado sem problema se o modelo
        // carregado nao suportar.
        cmd << " --mmproj \"" << mmprojPath << "\"";
    }

    if (!extraArgs.empty()) {
        // Argumentos extras definidos pelo usuario (aba Configuracoes) ou
        // pelo usuario, ex: "--flash-attn -t 8 -b 1024".
        // Vem por ultimo, entao qualquer flag repetida aqui tem prioridade
        // sobre as definidas acima (o llama-server usa a ultima ocorrencia).
        cmd << " " << extraArgs;
    }

    // Redireciona stdout/stderr do servidor para um arquivo de log. Sem
    // isso (CREATE_NO_WINDOW), qualquer erro real do llama-server (modelo
    // corrompido, porta em uso, VRAM insuficiente) se perdia no vazio e o
    // usuario so via um dot laranja eterno.
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;

    HANDLE hLog = INVALID_HANDLE_VALUE;
    try { std::filesystem::create_directories("logs"); } catch (...) {}
    m_logPath = "logs\\llama-server-" + std::to_string(port) + ".log";
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE }; // herdavel
    hLog = CreateFileA(m_logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hLog != INVALID_HANDLE_VALUE) {
        si.hStdOutput = hLog;
        si.hStdError  = hLog;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    } else {
        si.dwFlags &= ~STARTF_USESTDHANDLES; // sem log, segue sem redirecionar
    }

    ZeroMemory(&m_procInfo, sizeof(m_procInfo));

    std::string cmdStr = cmd.str();
    std::vector<char> cmdBuf(cmdStr.begin(), cmdStr.end());
    cmdBuf.push_back('\0');

    // CREATE_NO_WINDOW garante que nenhum console apareca para o processo filho.
    BOOL ok = CreateProcessA(
        nullptr, cmdBuf.data(), nullptr, nullptr,
        (si.dwFlags & STARTF_USESTDHANDLES) ? TRUE : FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &m_procInfo
    );

    // O filho ja herdou o handle do log; o pai pode fechar o seu.
    if (hLog != INVALID_HANDLE_VALUE) CloseHandle(hLog);

    if (!ok) {
        std::cerr << "Failed to start llama-server.exe (error " << GetLastError() << ")" << std::endl;
        return false;
    }
    m_running = true;
    return true;
}

void LlamaServerManager::Stop() {
    if (m_running) {
        TerminateProcess(m_procInfo.hProcess, 0);
        WaitForSingleObject(m_procInfo.hProcess, 3000);
        CloseHandle(m_procInfo.hProcess);
        CloseHandle(m_procInfo.hThread);
        m_running = false;
    }
}

bool LlamaServerManager::IsRunning() const {
    return m_running;
}

bool LlamaServerManager::ProcessAlive() const {
    if (!m_running || m_procInfo.hProcess == nullptr) return false;
    return WaitForSingleObject(m_procInfo.hProcess, 0) == WAIT_TIMEOUT;
}

DWORD LlamaServerManager::LastExitCode() const {
    DWORD code = 0;
    if (m_procInfo.hProcess) GetExitCodeProcess(m_procInfo.hProcess, &code);
    return code;
}
