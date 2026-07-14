#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <windows.h>
#include <wincrypt.h>
#include <nlohmann/json.hpp>
#include "HttpClient.h" // SamplingParams
#include "Base64.h"
#pragma comment(lib, "crypt32.lib")

using json = nlohmann::json;

// Manages persistent app settings, saved to config.json next to the executable.
// Stores: private Hugging Face token, language, theme, thinking mode,
// last used model, server port and sampling parameters (temperature, top-k, etc).
class ConfigManager {
public:
    std::string hfToken;
    int language = 1;      // 0=EN,1=PT_BR,2=ES,3=ZH
    bool darkMode = true;
    bool thinkingEnabled = true;
    bool webSearchEnabled = false;
    bool agentWebSearchEnabled = false;
    std::string lastModelPath = "models\\model.gguf";
    int serverPort = 8080;
    SamplingParams sampling;

    // Agent tab settings
    std::string agentModelPath;
    std::string agentMmprojPath;
    std::string agentProjectFolder;
    int agentPort = 8081;
    bool agentFreeMode = false;
    bool agentThinkingEnabled = true;

    // Reviewer model (optional second model that checks the Creator's work)
    std::string reviewerModelPath;
    std::string reviewerMmprojPath;
    int reviewerPort = 8082;
    bool useReviewerModel = false;
    bool useCustomReviewerPrompt = false;
    std::string customReviewerPrompt;

    // Debate Mode (Chat tab)
    bool debateModeEnabled = false;
    int debateRounds = 2;

    // Extra llama-server command-line arguments, applied to every server
    // this app starts (Chat, Agent, Reviewer, Debaters).
    std::string extraServerArgs;

    static ConfigManager& Instance() {
        static ConfigManager instance;
        return instance;
    }

    void Load(const std::string& path = "config.json") {
        m_path = path;
        std::ifstream f(path);
        if (!f.is_open()) return; // first run: keep defaults

        try {
            json j;
            f >> j;
            // Prefere o formato novo (token criptografado com DPAPI);
            // ainda le o campo antigo em texto puro de configs existentes,
            // que sera migrado para o formato seguro no proximo Save().
            std::string enc = j.value("hf_token_enc", "");
            if (!enc.empty()) {
                hfToken = UnprotectString(enc);
            } else {
                hfToken = j.value("hf_token", "");
            }
            language        = j.value("language", 1);
            darkMode        = j.value("dark_mode", true);
            thinkingEnabled = j.value("thinking_enabled", true);
            webSearchEnabled = j.value("web_search_enabled", false);
            agentWebSearchEnabled = j.value("agent_web_search_enabled", false);
            lastModelPath   = j.value("last_model_path", "models\\model.gguf");
            serverPort      = j.value("server_port", 8080);

            agentModelPath      = j.value("agent_model_path", "");
            agentMmprojPath     = j.value("agent_mmproj_path", "");
            agentProjectFolder  = j.value("agent_project_folder", "");
            agentPort           = j.value("agent_port", 8081);
            agentFreeMode       = j.value("agent_free_mode", false);
            agentThinkingEnabled = j.value("agent_thinking_enabled", true);

            reviewerModelPath      = j.value("reviewer_model_path", "");
            reviewerMmprojPath     = j.value("reviewer_mmproj_path", "");
            reviewerPort           = j.value("reviewer_port", 8082);
            useReviewerModel       = j.value("use_reviewer_model", false);
            useCustomReviewerPrompt = j.value("use_custom_reviewer_prompt", false);
            customReviewerPrompt   = j.value("custom_reviewer_prompt", "");

            debateModeEnabled = j.value("debate_mode_enabled", false);
            debateRounds = j.value("debate_rounds", 2);
            extraServerArgs = j.value("extra_server_args", "");

            if (j.contains("sampling")) {
                auto& s = j["sampling"];
                sampling.temperature   = s.value("temperature", 0.7f);
                sampling.topK          = s.value("top_k", 40);
                sampling.topP          = s.value("top_p", 0.9f);
                sampling.minP          = s.value("min_p", 0.05f);
                sampling.repeatPenalty = s.value("repeat_penalty", 1.1f);
                sampling.maxTokens     = s.value("max_tokens", 2048); // mesmo default do SamplingParams
                sampling.ctxSize       = s.value("ctx_size", 4096);
            }
        } catch (...) {
            // corrupted config: ignore and keep defaults
        }
    }

    void Save() {
        json j;
        // O token e criptografado com DPAPI (vinculado a conta do Windows
        // do usuario) em vez de salvo em texto puro no config.json.
        j["hf_token_enc"] = ProtectString(hfToken);
        j["language"] = language;
        j["dark_mode"] = darkMode;
        j["thinking_enabled"] = thinkingEnabled;
        j["web_search_enabled"] = webSearchEnabled;
        j["agent_web_search_enabled"] = agentWebSearchEnabled;
        j["last_model_path"] = lastModelPath;
        j["server_port"] = serverPort;

        j["agent_model_path"] = agentModelPath;
        j["agent_mmproj_path"] = agentMmprojPath;
        j["agent_project_folder"] = agentProjectFolder;
        j["agent_port"] = agentPort;
        j["agent_free_mode"] = agentFreeMode;
        j["agent_thinking_enabled"] = agentThinkingEnabled;

        j["reviewer_model_path"] = reviewerModelPath;
        j["reviewer_mmproj_path"] = reviewerMmprojPath;
        j["reviewer_port"] = reviewerPort;
        j["use_reviewer_model"] = useReviewerModel;
        j["use_custom_reviewer_prompt"] = useCustomReviewerPrompt;
        j["custom_reviewer_prompt"] = customReviewerPrompt;

        j["debate_mode_enabled"] = debateModeEnabled;
        j["debate_rounds"] = debateRounds;
        j["extra_server_args"] = extraServerArgs;

        j["sampling"] = {
            {"temperature", sampling.temperature},
            {"top_k", sampling.topK},
            {"top_p", sampling.topP},
            {"min_p", sampling.minP},
            {"repeat_penalty", sampling.repeatPenalty},
            {"max_tokens", sampling.maxTokens},
            {"ctx_size", sampling.ctxSize}
        };

        std::ofstream f(m_path.empty() ? "config.json" : m_path);
        if (f.is_open()) {
            f << j.dump(4);
        }
    }

private:
    ConfigManager() = default;
    std::string m_path = "config.json";

    // Criptografa uma string com a DPAPI do Windows e devolve em base64.
    // So a mesma conta de usuario (nesta maquina) consegue descriptografar.
    static std::string ProtectString(const std::string& plain) {
        if (plain.empty()) return "";
        DATA_BLOB in{ (DWORD)plain.size(), (BYTE*)plain.data() };
        DATA_BLOB out{};
        if (!CryptProtectData(&in, L"DFFastLLM", nullptr, nullptr, nullptr, 0, &out)) return "";
        std::vector<uint8_t> bytes(out.pbData, out.pbData + out.cbData);
        LocalFree(out.pbData);
        return Base64::Encode(bytes);
    }

    static std::string UnprotectString(const std::string& b64) {
        if (b64.empty()) return "";
        std::vector<uint8_t> bytes = Base64::Decode(b64);
        if (bytes.empty()) return "";
        DATA_BLOB in{ (DWORD)bytes.size(), bytes.data() };
        DATA_BLOB out{};
        if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) return "";
        std::string plain((char*)out.pbData, out.cbData);
        LocalFree(out.pbData);
        return plain;
    }
};
