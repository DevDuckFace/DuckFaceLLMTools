#include "HttpClient.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct StreamCtx {
    std::function<void(bool, const std::string&)> onDelta;
    std::function<bool()> shouldStop;
    std::string buffer;
    bool usedNativeReasoning = false;
    bool wasCancelled = false; // set only when shouldStop() aborted the stream
};

static size_t StreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* ctx = static_cast<StreamCtx*>(userp);
    if (ctx->shouldStop && ctx->shouldStop()) {
        ctx->wasCancelled = true; // distingue cancelamento do usuario de erro real
        return 0;
    }

    std::string chunk((char*)contents, size * nmemb);
    ctx->buffer += chunk;

    size_t pos;
    while ((pos = ctx->buffer.find("\n\n")) != std::string::npos) {
        std::string line = ctx->buffer.substr(0, pos);
        ctx->buffer.erase(0, pos + 2);

        if (line.rfind("data: ", 0) != 0) continue;
        std::string jsonStr = line.substr(6);
        if (jsonStr == "[DONE]") continue;

        try {
            json j = json::parse(jsonStr);
            if (!j.contains("choices") || j["choices"].empty()) continue;
            auto& choice = j["choices"][0];
            if (!choice.contains("delta")) continue;
            auto& delta = choice["delta"];

            if (delta.contains("reasoning_content") && !delta["reasoning_content"].is_null()) {
                std::string r = delta["reasoning_content"].get<std::string>();
                if (!r.empty()) {
                    ctx->usedNativeReasoning = true;
                    if (ctx->onDelta) ctx->onDelta(true, r);
                }
            }
            if (delta.contains("content") && !delta["content"].is_null()) {
                std::string c = delta["content"].get<std::string>();
                if (!c.empty() && ctx->onDelta) ctx->onDelta(false, c);
            }
        } catch (...) {
            // partial/malformed line: wait for the next chunk
        }
    }
    return size * nmemb;
}

bool HttpClient::ChatStream(const std::vector<ChatTurn>& history,
                             int port,
                             const SamplingParams& params,
                             bool enableThinking,
                             std::function<void(bool, const std::string&)> onDelta,
                             std::function<bool()> shouldStop) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/v1/chat/completions";

    json messages = json::array();
    for (auto& turn : history) {
        if (!turn.imageBase64.empty()) {
            // Vision-capable models expect content as an array mixing text
            // and an image_url data-uri part (OpenAI-compatible format,
            // supported by llama-server when an --mmproj is loaded).
            json contentArr = json::array();
            contentArr.push_back({ {"type", "text"}, {"text", turn.content} });
            std::string dataUri = "data:" + turn.imageMime + ";base64," + turn.imageBase64;
            contentArr.push_back({ {"type", "image_url"}, {"image_url", { {"url", dataUri} }} });
            messages.push_back({ {"role", turn.role}, {"content", contentArr} });
        } else {
            messages.push_back({ {"role", turn.role}, {"content", turn.content} });
        }
    }

    json body;
    body["messages"]         = messages;
    body["stream"]           = true;
    body["temperature"]      = params.temperature;
    body["top_k"]            = params.topK;
    body["top_p"]             = params.topP;
    body["min_p"]             = params.minP;
    body["repeat_penalty"]    = params.repeatPenalty;
    body["max_tokens"]        = params.maxTokens;
    body["reasoning_format"]  = enableThinking ? "auto" : "none";

    std::string bodyStr = body.dump();

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    StreamCtx ctx{ onDelta, shouldStop, "", false };

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    // Sem CONNECTTIMEOUT, um servidor que caiu deixa a UI presa por muito
    // tempo esperando o curl desistir da conexao.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // CURLE_WRITE_ERROR so conta como sucesso quando foi o PROPRIO usuario
    // que cancelou (shouldStop). Um erro de escrita real (disco, etc)
    // continua sendo reportado como falha, em vez de engolido.
    return res == CURLE_OK || (res == CURLE_WRITE_ERROR && ctx.wasCancelled);
}

HttpClient::SplitResult HttpClient::SplitThinking(const std::string& fullText) {
    SplitResult result;
    size_t start = fullText.find("<think>");
    size_t end = fullText.find("</think>");

    if (start != std::string::npos && end != std::string::npos && end > start) {
        result.thinking = fullText.substr(start + 7, end - (start + 7));
        result.finalAnswer = fullText.substr(0, start) + fullText.substr(end + 8);
    } else if (start != std::string::npos && end == std::string::npos) {
        result.thinking = fullText.substr(start + 7);
        result.finalAnswer = fullText.substr(0, start);
    } else {
        result.finalAnswer = fullText;
    }
    return result;
}

bool HttpClient::CheckHealth(int port) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/health";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void*, size_t s, size_t n, void*) { return s * n; });
    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    return res == CURLE_OK && code == 200;
}
