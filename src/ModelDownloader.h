#pragma once
#include <string>
#include <vector>
#include <atomic>

struct DownloadState {
    std::atomic<double> percent{0.0};
    std::atomic<bool> inProgress{false};
    std::atomic<bool> success{false};
    std::atomic<bool> cancelRequested{false};
    std::string errorMessage;
    std::string currentFile;
    // Bytes ja presentes no .part quando o download foi retomado — usado
    // para a barra de progresso refletir o arquivo INTEIRO, nao so o resto.
    std::atomic<long long> resumeOffset{0};
};

// DuckFaceLLM runs on llama.cpp, whose engine only executes .gguf files.
// This downloader is intentionally scoped to .gguf only — there is no
// point downloading raw safetensors weights that the app can't run.
class ModelDownloader {
public:
    static bool DownloadFile(const std::string& repo,
                              const std::string& filename,
                              const std::string& destPath,
                              const std::string& hfToken,
                              DownloadState* state);

    // Lists only .gguf files available in a Hugging Face repository.
    static bool ListGgufFiles(const std::string& repo,
                               const std::string& hfToken,
                               std::vector<std::string>& outFiles,
                               std::string& errorOut);

    static bool ValidateToken(const std::string& hfToken, std::string& usernameOut);
};
