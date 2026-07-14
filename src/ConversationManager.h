#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct ChatMessage {
    std::string role;     // "user" or "assistant"
    std::string content;  // final answer text (no reasoning), or the user's typed text
    std::string thinking; // assistant-only: reasoning shown at the time, kept for display
    std::string imageBase64; // user-only: base64 image data if an image was attached
    std::string imageMime;   // user-only: e.g. "image/png"
    std::string attachedFileName; // display-only, e.g. "diagram.png" or "report.pdf"
};

struct Conversation {
    std::string id;
    std::string title;
    long long createdAt = 0;
    std::vector<ChatMessage> messages;
};

// Lightweight metadata used to populate the History list without loading
// every conversation's full message list into memory.
struct ConversationSummary {
    std::string id;
    std::string title;
    long long createdAt = 0;
};

class ConversationManager {
public:
    // Not a singleton anymore: the app keeps two independent instances —
    // one for regular Chat conversations, one for Agent projects — so the
    // two don't get mixed together in each other's History/Projects list.
    explicit ConversationManager(std::string dir = "conversations") : m_dir(std::move(dir)) {}

    Conversation NewConversation() {
        Conversation c;
        c.id = "conv_" + std::to_string(NowMillis());
        c.title = "";
        c.createdAt = NowMillis();
        return c;
    }

    void Save(Conversation& c) {
        std::filesystem::create_directories(m_dir);
        if (c.id.empty()) c.id = "conv_" + std::to_string(NowMillis());
        if (c.title.empty() && !c.messages.empty()) {
            c.title = MakeTitleFromFirstMessage(c.messages[0].content);
        }
        if (c.title.empty()) c.title = "New Conversation";

        json j;
        j["id"] = c.id;
        j["title"] = c.title;
        j["created_at"] = c.createdAt;
        j["messages"] = json::array();
        for (auto& m : c.messages) {
            j["messages"].push_back({
                {"role", m.role},
                {"content", m.content},
                {"thinking", m.thinking},
                {"image_base64", m.imageBase64},
                {"image_mime", m.imageMime},
                {"attached_file_name", m.attachedFileName}
            });
        }

        std::ofstream f(FilePath(c.id));
        if (f.is_open()) f << j.dump(2);
        UpdateIndexEntry(c.id, c.title, c.createdAt);
    }

    bool Load(const std::string& id, Conversation& out) {
        std::ifstream f(FilePath(id));
        if (!f.is_open()) return false;
        try {
            json j;
            f >> j;
            out.id = j.value("id", id);
            out.title = j.value("title", "");
            out.createdAt = j.value("created_at", 0LL);
            out.messages.clear();
            if (j.contains("messages")) {
                for (auto& m : j["messages"]) {
                    ChatMessage cm;
                    cm.role = m.value("role", "user");
                    cm.content = m.value("content", "");
                    cm.thinking = m.value("thinking", "");
                    cm.imageBase64 = m.value("image_base64", "");
                    cm.imageMime = m.value("image_mime", "");
                    cm.attachedFileName = m.value("attached_file_name", "");
                    out.messages.push_back(cm);
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    void Delete(const std::string& id) {
        std::filesystem::remove(FilePath(id));
        RemoveIndexEntry(id);
    }

    // Le apenas o index.json (titulo/data de cada conversa) em vez de
    // fazer parse de TODOS os arquivos de conversa — que podem conter
    // imagens em base64 de varios MB cada. Antes disso, abrir a aba
    // History com muitas conversas travava a UI perceptavelmente; agora e
    // instantaneo. Se o indice nao existir (versao antiga do app) ele e
    // reconstruido uma unica vez a partir dos arquivos e salvo.
    std::vector<ConversationSummary> ListAll() {
        std::vector<ConversationSummary> out;
        if (!std::filesystem::exists(m_dir)) return out;

        json idx = LoadIndex();
        if (idx.is_null()) {
            idx = RebuildIndexFromFiles();
            SaveIndex(idx);
        }

        for (auto& [id, meta] : idx.items()) {
            // Ignora entradas cujo arquivo foi apagado por fora do app.
            if (!std::filesystem::exists(FilePath(id))) continue;
            ConversationSummary s;
            s.id = id;
            s.title = meta.value("title", "Untitled");
            s.createdAt = meta.value("created_at", 0LL);
            out.push_back(s);
        }

        std::sort(out.begin(), out.end(), [](const ConversationSummary& a, const ConversationSummary& b) {
            return a.createdAt > b.createdAt;
        });
        return out;
    }

private:
    std::string m_dir = "conversations";

    std::string IndexPath() {
        return (std::filesystem::path(m_dir) / "index.json").string();
    }

    json LoadIndex() {
        std::ifstream f(IndexPath());
        if (!f.is_open()) return json();
        try { json j; f >> j; return j.is_object() ? j : json(); }
        catch (...) { return json(); }
    }

    void SaveIndex(const json& idx) {
        std::filesystem::create_directories(m_dir);
        std::ofstream f(IndexPath());
        if (f.is_open()) f << idx.dump(2);
    }

    void UpdateIndexEntry(const std::string& id, const std::string& title, long long createdAt) {
        json idx = LoadIndex();
        if (idx.is_null()) idx = RebuildIndexFromFiles();
        idx[id] = { {"title", title}, {"created_at", createdAt} };
        SaveIndex(idx);
    }

    void RemoveIndexEntry(const std::string& id) {
        json idx = LoadIndex();
        if (idx.is_null()) return;
        idx.erase(id);
        SaveIndex(idx);
    }

    // Migracao/recuperacao: varre os arquivos de conversa uma unica vez
    // para montar o indice (o caminho lento antigo, agora executado so
    // quando o index.json nao existe ou foi corrompido).
    json RebuildIndexFromFiles() {
        json idx = json::object();
        if (!std::filesystem::exists(m_dir)) return idx;
        for (auto& entry : std::filesystem::directory_iterator(m_dir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            if (entry.path().filename() == "index.json") continue;
            std::ifstream f(entry.path());
            if (!f.is_open()) continue;
            try {
                json j;
                f >> j;
                std::string id = j.value("id", entry.path().stem().string());
                idx[id] = { {"title", j.value("title", "Untitled")},
                            {"created_at", j.value("created_at", 0LL)} };
            } catch (...) { continue; }
        }
        return idx;
    }

    std::string FilePath(const std::string& id) {
        return (std::filesystem::path(m_dir) / (id + ".json")).string();
    }

    static long long NowMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    static std::string MakeTitleFromFirstMessage(const std::string& text) {
        std::string t = text.substr(0, 60);
        // avoid cutting mid-word too awkwardly
        size_t lastSpace = t.find_last_of(' ');
        if (lastSpace != std::string::npos && lastSpace > 20) t = t.substr(0, lastSpace);
        for (auto& c : t) if (c == '\n' || c == '\r') c = ' ';
        return t.empty() ? "New Conversation" : t;
    }
};
