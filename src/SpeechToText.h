#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

// Cliente do whisper-server (whisper.cpp): manda um WAV por multipart para
// POST /inference e recebe {"text": "..."}. Mesmo modelo de operacao do
// llama-server: um processo local gerenciado pelo app, sem nuvem.
namespace SpeechToText {

inline size_t WriteCb(void* c, size_t s, size_t n, void* u) {
    ((std::string*)u)->append((char*)c, s * n);
    return s * n;
}

// Transcreve o WAV. `language` vazio = deteccao automatica.
inline std::string Transcribe(const std::vector<uint8_t>& wav, int port,
                               const std::string& language, std::string& errorOut) {
    CURL* curl = curl_easy_init();
    if (!curl) { errorOut = "curl init failed"; return ""; }

    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/inference";
    std::string response;

    curl_mime* mime = curl_mime_init(curl);
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "file");
    curl_mime_filename(part, "audio.wav");
    curl_mime_type(part, "audio/wav");
    curl_mime_data(part, (const char*)wav.data(), wav.size());

    part = curl_mime_addpart(mime);
    curl_mime_name(part, "response_format");
    curl_mime_data(part, "json", CURL_ZERO_TERMINATED);

    if (!language.empty()) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "language");
        curl_mime_data(part, language.c_str(), CURL_ZERO_TERMINATED);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L); // audios longos demoram

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) { errorOut = curl_easy_strerror(res); return ""; }
    if (httpCode != 200) { errorOut = "whisper-server HTTP " + std::to_string(httpCode); return ""; }

    try {
        auto j = nlohmann::json::parse(response);
        std::string text = j.value("text", "");
        // whisper devolve com espacos/linhas nas pontas
        while (!text.empty() && (text.front() == ' ' || text.front() == '\n')) text.erase(text.begin());
        while (!text.empty() && (text.back() == ' ' || text.back() == '\n')) text.pop_back();
        return text;
    } catch (...) {
        errorOut = "unexpected whisper-server response";
        return "";
    }
}

// Health check simples (o whisper-server responde na raiz).
inline bool CheckHealth(int port) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    std::string resp;
    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

} // namespace SpeechToText
