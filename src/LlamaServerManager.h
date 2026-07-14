#pragma once
#include <string>
#include <windows.h>

// Controla o processo llama-server.exe (start/stop) sem herdar a janela de console.
class LlamaServerManager {
public:
    ~LlamaServerManager();

    bool Start(const std::string& modelPath, int port = 8080, int ctxSize = 4096,
               int gpuLayers = 999, const std::string& mmprojPath = "",
               const std::string& extraArgs = "");
    void Stop();
    bool IsRunning() const;

    // True while the child process is actually alive. Lets the health-check
    // poller distinguish "model still loading" from "process crashed on
    // startup" (bad model file, port already in use, out of VRAM, etc).
    bool ProcessAlive() const;

    // Exit code of the dead process (only meaningful after ProcessAlive()
    // returned false). Useful for error messages / the log file.
    DWORD LastExitCode() const;

    // Path of the log file stderr/stdout of the last started server was
    // redirected to (e.g. "logs\llama-server-8080.log"), for diagnostics.
    std::string LogFilePath() const { return m_logPath; }

private:
    PROCESS_INFORMATION m_procInfo{};
    bool m_running = false;
    std::string m_logPath;
};
