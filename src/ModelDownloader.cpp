#include "ModelDownloader.h"
#include <curl/curl.h>
#include <fstream>
#include <cstdio>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct WriteCtx {
    std::ofstream* out;
    DownloadState* state;
};

static size_t FileWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* ctx = static_cast<WriteCtx*>(userp);
    if (ctx->state && ctx->state->cancelRequested.load()) return 0;
    ctx->out->write(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

static int ProgressFn(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* state = static_cast<DownloadState*>(clientp);
    if (state->cancelRequested.load()) return 1;
    // Em retomadas, dltotal/dlnow cobrem so o restante; somamos o offset
    // para o % mostrado corresponder ao arquivo completo.
    curl_off_t off = (curl_off_t)state->resumeOffset.load();
    if (dltotal + off > 0) state->percent = (double)(dlnow + off) / (double)(dltotal + off) * 100.0;
    return 0;
}

static curl_slist* BuildAuthHeader(const std::string& hfToken) {
    curl_slist* headers = nullptr;
    if (!hfToken.empty()) {
        std::string authHeader = "Authorization: Bearer " + hfToken;
        headers = curl_slist_append(headers, authHeader.c_str());
    }
    return headers;
}

bool ModelDownloader::DownloadFile(const std::string& repo,
                                    const std::string& filename,
                                    const std::string& destPath,
                                    const std::string& hfToken,
                                    DownloadState* state) {
    state->inProgress = true;
    state->success = false;
    state->percent = 0.0;
    state->currentFile = filename;
    state->errorMessage.clear();

    std::string url = "https://huggingface.co/" + repo + "/resolve/main/" + filename;

    CURL* curl = curl_easy_init();
    if (!curl) {
        state->errorMessage = "Failed to initialize curl";
        state->inProgress = false;
        return false;
    }

    // RETOMADA: GGUFs tem dezenas de GB. Se um .part de uma tentativa
    // anterior existir, continuamos de onde parou (HTTP Range) em vez de
    // apagar tudo e recomecar do zero.
    std::string tmpPath = destPath + ".part";
    curl_off_t resumeFrom = 0;
    {
        std::ifstream existing(tmpPath, std::ios::binary | std::ios::ate);
        if (existing.is_open()) resumeFrom = (curl_off_t)existing.tellg();
    }
    state->resumeOffset = (long long)resumeFrom;
    std::ofstream outFile(tmpPath, std::ios::binary | (resumeFrom > 0 ? std::ios::app : std::ios::trunc));
    if (!outFile.is_open()) {
        state->errorMessage = "Could not create destination file: " + tmpPath;
        curl_easy_cleanup(curl);
        state->inProgress = false;
        return false;
    }

    WriteCtx ctx{ &outFile, state };
    curl_slist* headers = BuildAuthHeader(hfToken);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FileWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressFn);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, state);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DuckFaceLLM/2.0");
    if (resumeFrom > 0) curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, resumeFrom);
    // Detecta conexao "morta": se ficar abaixo de 1 byte/s por 60s, aborta
    // em vez de travar para sempre. Tambem limita o tempo de conexao.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    outFile.close();

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // 200 = download completo; 206 = resposta parcial (retomada aceita).
    // 416 na retomada significa que o .part ja estava completo.
    bool ok = (res == CURLE_OK && (httpCode == 200 || httpCode == 206));
    if (!ok && resumeFrom > 0 && httpCode == 416) ok = true;
    // Se pedimos retomada mas o servidor ignorou o Range e devolveu 200
    // (arquivo inteiro), o corpo completo foi APENDADO ao .part parcial e
    // ele ficou corrompido: descarta e trata como falha para a proxima
    // tentativa comecar limpa do zero.
    if (resumeFrom > 0 && httpCode == 200) {
        ok = false;
        std::remove(tmpPath.c_str());
        if (state->errorMessage.empty())
            state->errorMessage = "Server did not support resuming; partial file discarded - please retry.";
    }

    if (ok) {
        std::remove(destPath.c_str());
        std::rename(tmpPath.c_str(), destPath.c_str());
        state->success = true;
    } else {
        // NAO apagamos o .part em caso de falha/cancelamento: e ele que
        // permite retomar o download depois em vez de recomecar do zero.
        if (state->cancelRequested.load()) {
            state->errorMessage = "Cancelled by user (partial file kept - restarting the download will resume it)";
        } else if (httpCode == 401 || httpCode == 403) {
            state->errorMessage = "Access denied (HTTP " + std::to_string(httpCode) +
                                   "). Check your token and whether you accepted the model's license.";
        } else {
            state->errorMessage = "HTTP error " + std::to_string(httpCode) +
                                   " (" + curl_easy_strerror(res) + ")";
        }
    }

    state->inProgress = false;
    return ok;
}

static size_t StringWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool ModelDownloader::ListGgufFiles(const std::string& repo,
                                     const std::string& hfToken,
                                     std::vector<std::string>& outFiles,
                                     std::string& errorOut) {
    CURL* curl = curl_easy_init();
    if (!curl) { errorOut = "Failed to initialize curl"; return false; }

    std::string url = "https://huggingface.co/api/models/" + repo;
    std::string response;
    curl_slist* headers = BuildAuthHeader(hfToken);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DuckFaceLLM/2.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        errorOut = std::string("Network error: ") + curl_easy_strerror(res);
        return false;
    }
    if (httpCode == 401 || httpCode == 403) {
        errorOut = "Access denied (HTTP " + std::to_string(httpCode) +
                   "). This model may require a token with access, or you need to accept the license on the website first.";
        return false;
    }
    if (httpCode == 404) {
        errorOut = "Repository not found.";
        return false;
    }

    try {
        json j = json::parse(response);
        if (!j.contains("siblings")) {
            errorOut = "API response has no file list.";
            return false;
        }
        for (auto& sib : j["siblings"]) {
            std::string fname = sib.value("rfilename", "");
            if (fname.size() > 5 && fname.substr(fname.size() - 5) == ".gguf") {
                outFiles.push_back(fname);
            }
        }
        if (outFiles.empty()) {
            errorOut = "No .gguf files found in this repository. "
                       "DuckFaceLLM's engine (llama.cpp) only runs .gguf files, so non-GGUF repos can't be used here — "
                       "look for a GGUF quantization of this model instead (often published by users like bartowski or unsloth).";
            return false;
        }
        return true;
    } catch (...) {
        errorOut = "Failed to parse Hugging Face API response.";
        return false;
    }
}

bool ModelDownloader::ValidateToken(const std::string& hfToken, std::string& usernameOut) {
    if (hfToken.empty()) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = "https://huggingface.co/api/whoami-v2";
    std::string response;
    curl_slist* headers = BuildAuthHeader(hfToken);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DuckFaceLLM/2.0");
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) return false;

    try {
        json j = json::parse(response);
        usernameOut = j.value("name", "");
        return true;
    } catch (...) {
        return false;
    }
}
