#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <ctime>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// A ready-made prompt template: a specialized system prompt tuned for one
// kind of task, plus a list of keywords used to auto-detect when it applies.
struct PromptTemplate {
    std::string id;
    std::string displayName;
    std::vector<std::string> keywords; // lowercase, matched as substrings
    std::string systemPrompt;
    bool isBuiltIn = false; // built-ins ship with the app and can't be deleted
};

// Manages both the built-in templates (hardcoded, always available) and
// user-created custom templates (persisted to custom_templates.json next to
// the executable, so they survive across sessions and app updates).
class TemplateManager {
public:
    static TemplateManager& Instance() {
        static TemplateManager instance;
        return instance;
    }

    // Full combined list: built-ins first, then custom ones.
    const std::vector<PromptTemplate>& All() {
        if (m_combined.empty() || m_dirty) {
            RebuildCombined();
        }
        return m_combined;
    }

    // Scores every template (built-in + custom) against the user's input and
    // returns the index into All() of the best match, or -1 if nothing
    // scored above zero.
    int SelectBest(const std::string& userInput) {
        std::string lower = ToLower(userInput);
        const auto& templates = All();

        int bestIdx = -1;
        int bestScore = 0;

        for (int i = 0; i < (int)templates.size(); i++) {
            int score = 0;
            for (const auto& kw : templates[i].keywords) {
                if (!kw.empty() && lower.find(kw) != std::string::npos) score++;
            }
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }
        return bestIdx;
    }

    // Adds a new custom template. keywordsCsv is a comma-separated string
    // (e.g. "rust, cargo, .rs") typed by the user in the GUI; it gets split,
    // trimmed and lowercased automatically. Returns false if name or system
    // prompt are empty.
    bool AddCustom(const std::string& displayName,
                    const std::string& keywordsCsv,
                    const std::string& systemPrompt,
                    std::string& errorOut) {
        if (displayName.empty()) { errorOut = "Name is required."; return false; }
        if (systemPrompt.empty()) { errorOut = "System prompt is required."; return false; }

        PromptTemplate t;
        t.id = "custom_" + std::to_string(m_nextCustomId++);
        t.displayName = displayName;
        t.systemPrompt = systemPrompt;
        t.keywords = SplitKeywords(keywordsCsv);
        t.isBuiltIn = false;

        if (t.keywords.empty()) {
            errorOut = "Add at least one keyword so the app knows when to auto-select this template.";
            return false;
        }

        m_custom.push_back(t);
        m_dirty = true;
        Save();
        return true;
    }

    // Atualiza um template existente. Templates CUSTOM sao editados no
    // lugar. Templates BUILT-IN (cujo texto vive no codigo) nao podem ser
    // alterados diretamente — entao a edicao "converte": o built-in e
    // ocultado e uma copia custom com as mudancas assume o lugar dele.
    bool Update(const std::string& id,
                const std::string& displayName,
                const std::string& keywordsCsv,
                const std::string& systemPrompt,
                std::string& errorOut) {
        if (displayName.empty()) { errorOut = "Name is required."; return false; }
        if (systemPrompt.empty()) { errorOut = "System prompt is required."; return false; }
        auto keywords = SplitKeywords(keywordsCsv);
        if (keywords.empty()) {
            errorOut = "Add at least one keyword so the app knows when to auto-select this template.";
            return false;
        }

        for (auto& t : m_custom) {
            if (t.id == id) {
                t.displayName = displayName;
                t.keywords = keywords;
                t.systemPrompt = systemPrompt;
                m_dirty = true;
                Save();
                return true;
            }
        }

        // Built-in: oculta o original e cria a versao editada como custom.
        for (auto& b : BuiltIns()) {
            if (b.id == id) {
                if (std::find(m_hiddenBuiltins.begin(), m_hiddenBuiltins.end(), id) == m_hiddenBuiltins.end())
                    m_hiddenBuiltins.push_back(id);
                PromptTemplate t;
                t.id = "custom_" + std::to_string(++m_nextCustomId) + "_" + std::to_string((long long)time(nullptr));
                t.isBuiltIn = false;
                t.displayName = displayName;
                t.keywords = keywords;
                t.systemPrompt = systemPrompt;
                m_custom.push_back(t);
                m_dirty = true;
                Save();
                return true;
            }
        }
        errorOut = "Template not found.";
        return false;
    }

    // Removes a template by id. Works for both custom templates (actually
    // deleted) and built-in ones (hidden permanently, since their code
    // lives in this file — hiding is persisted so it survives restarts).
    bool Remove(const std::string& id) {
        auto it = std::remove_if(m_custom.begin(), m_custom.end(),
            [&](const PromptTemplate& t) { return t.id == id; });
        if (it != m_custom.end()) {
            m_custom.erase(it, m_custom.end());
            m_dirty = true;
            Save();
            return true;
        }

        // Not a custom template: must be a built-in id. Hide it.
        if (std::find(m_hiddenBuiltins.begin(), m_hiddenBuiltins.end(), id) == m_hiddenBuiltins.end()) {
            m_hiddenBuiltins.push_back(id);
            m_dirty = true;
            Save();
            return true;
        }
        return false;
    }

    // Restores every hidden built-in template.
    void RestoreBuiltIns() {
        m_hiddenBuiltins.clear();
        m_dirty = true;
        Save();
    }

    bool HasHiddenBuiltIns() const { return !m_hiddenBuiltins.empty(); }

    void Load(const std::string& path = "custom_templates.json") {
        m_path = path;
        m_custom.clear();
        m_hiddenBuiltins.clear();
        m_nextCustomId = 1;

        std::ifstream f(path);
        if (!f.is_open()) { m_dirty = true; return; } // no custom templates yet, that's fine

        try {
            json j;
            f >> j;
            if (j.contains("hidden_builtins") && j["hidden_builtins"].is_array()) {
                for (auto& id : j["hidden_builtins"]) {
                    m_hiddenBuiltins.push_back(id.get<std::string>());
                }
            }
            if (j.contains("templates") && j["templates"].is_array()) {
                for (auto& item : j["templates"]) {
                    PromptTemplate t;
                    t.id = item.value("id", "");
                    t.displayName = item.value("name", "");
                    t.systemPrompt = item.value("system_prompt", "");
                    t.isBuiltIn = false;
                    if (item.contains("keywords") && item["keywords"].is_array()) {
                        for (auto& kw : item["keywords"]) {
                            t.keywords.push_back(kw.get<std::string>());
                        }
                    }
                    if (t.id.empty() || t.displayName.empty()) continue;
                    m_custom.push_back(t);

                    // keep the auto-increment id counter ahead of loaded ids
                    if (t.id.rfind("custom_", 0) == 0) {
                        try {
                            int n = std::stoi(t.id.substr(7));
                            if (n >= m_nextCustomId) m_nextCustomId = n + 1;
                        } catch (...) {}
                    }
                }
            }
        } catch (...) {
            // corrupted file: start fresh rather than crash
        }
        m_dirty = true;
    }

    void Save() {
        json j;
        j["templates"] = json::array();
        for (auto& t : m_custom) {
            json item;
            item["id"] = t.id;
            item["name"] = t.displayName;
            item["system_prompt"] = t.systemPrompt;
            item["keywords"] = t.keywords;
            j["templates"].push_back(item);
        }
        j["hidden_builtins"] = m_hiddenBuiltins;
        std::ofstream f(m_path.empty() ? "custom_templates.json" : m_path);
        if (f.is_open()) f << j.dump(4);
    }

private:
    TemplateManager() = default;

    std::vector<PromptTemplate> m_custom;
    std::vector<PromptTemplate> m_combined;
    std::vector<std::string> m_hiddenBuiltins;
    bool m_dirty = true;
    int m_nextCustomId = 1;
    std::string m_path = "custom_templates.json";

    static std::string ToLower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        return out;
    }

    static std::string Trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    static std::vector<std::string> SplitKeywords(const std::string& csv) {
        std::vector<std::string> out;
        std::stringstream ss(csv);
        std::string item;
        while (std::getline(ss, item, ',')) {
            std::string trimmed = Trim(item);
            if (!trimmed.empty()) out.push_back(ToLower(trimmed));
        }
        return out;
    }

    void RebuildCombined() {
        m_combined.clear();
        for (auto& t : BuiltIns()) {
            bool hidden = std::find(m_hiddenBuiltins.begin(), m_hiddenBuiltins.end(), t.id) != m_hiddenBuiltins.end();
            if (!hidden) m_combined.push_back(t);
        }
        for (auto& t : m_custom) m_combined.push_back(t);
        m_dirty = false;
    }

    static const std::vector<PromptTemplate>& BuiltIns() {
        static std::vector<PromptTemplate> t = BuildBuiltIns();
        return t;
    }

    static std::vector<PromptTemplate> BuildBuiltIns() {
        std::vector<PromptTemplate> t;

        t.push_back({
            "cpp", "C++ Programming",
            {"c++", "cpp", ".cpp", ".hpp", ".h ", "cmake", "std::", "compilador c++", "programa em c++"},
            "You are an expert C++ engineer. Write clean, modern C++ (C++17 or later unless told otherwise). "
            "Always: (1) include necessary headers, (2) use RAII and smart pointers instead of raw new/delete when possible, "
            "(3) add brief comments only where the logic isn't obvious, (4) provide a minimal compilable example when relevant, "
            "(5) mention the exact compiler flags or CMake setup needed if it's not trivial. "
            "Do not pad the answer with unrelated theory; stay focused on working code and a short explanation.",
            true
        });

        t.push_back({
            "python", "Python Programming",
            {"python", ".py", "pip install", "django", "flask", "pandas", "numpy", "script em python"},
            "You are an expert Python engineer. Write clean, idiomatic Python (PEP 8), using type hints when helpful. "
            "Prefer the standard library unless a third-party package is clearly the right tool; if you use one, name it "
            "and the pip install command. Keep functions small and testable. Avoid unnecessary boilerplate or over-engineering.",
            true
        });

        t.push_back({
            "webdev", "Web Development (HTML/CSS/JS)",
            {"html", "css", "javascript", "typescript", "react", "vue", "frontend", "front-end", "node.js", "npm"},
            "You are an expert front-end/full-stack web developer. Write clean, modern HTML/CSS/JS (or TS/React when asked). "
            "Prefer semantic HTML, avoid inline styles unless requested, keep JS unobtrusive and modular. "
            "Call out browser compatibility concerns only if genuinely relevant.",
            true
        });

        t.push_back({
            "sql", "SQL / Databases",
            {"sql", "select ", "database", "banco de dados", "query", "postgres", "mysql", "sqlite"},
            "You are an expert database engineer. Write correct, efficient SQL. State which SQL dialect you're targeting "
            "(ANSI SQL by default, unless the user names PostgreSQL/MySQL/SQLite/etc). Mention indexes or performance "
            "concerns only when they matter for the query at hand.",
            true
        });

        t.push_back({
            "shell", "Shell / Bash Scripting",
            {"bash", "shell script", ".sh", "linux terminal", "powershell", "cmd", "script de terminal"},
            "You are an expert in shell scripting (bash/POSIX sh, or PowerShell if the user is on Windows). "
            "Write scripts that handle errors sensibly (set -euo pipefail for bash where appropriate), quote variables "
            "correctly, and include a one-line comment above any non-obvious command.",
            true
        });

        t.push_back({
            "debug", "Debugging / Fixing Code",
            {"erro", "bug", "corrigir", "fix", "not working", "não funciona", "exception", "traceback", "stack trace", "debug"},
            "You are a meticulous debugging assistant. First identify the most likely root cause of the problem based on "
            "the code/error given. Explain the cause briefly, then give the corrected code. If the cause is ambiguous, "
            "list the top 1-2 likely causes and how to distinguish between them, instead of guessing silently.",
            true
        });

        t.push_back({
            "codereview", "Code Review",
            {"revisar codigo", "code review", "review this code", "revisar meu codigo", "melhorar esse codigo", "refactor", "refatorar"},
            "You are a senior engineer doing a code review. Point out real issues: bugs, edge cases, security concerns, "
            "readability, and performance, in that priority order. Suggest concrete fixes, not vague advice. "
            "Acknowledge what's already done well, briefly, without padding.",
            true
        });

        t.push_back({
            "explain", "Explain / Teach a Concept",
            {"explique", "explain", "o que é", "what is", "como funciona", "how does", "me ensine", "teach me"},
            "You are a clear, patient teacher. Explain the concept starting from first principles, using a concrete "
            "example or analogy where it helps. Avoid unnecessary jargon; when you must use a technical term, define it "
            "briefly the first time. Keep the explanation as short as it can be while still being complete.",
            true
        });

        t.push_back({
            "translate", "Translation",
            {"traduza", "translate", "traducir", "traduzir para", "translation"},
            "You are a professional translator. Preserve tone, meaning, and register (formal/informal) from the source. "
            "Do not add explanations unless asked. If a term has no direct equivalent, translate it as closely as "
            "possible and add a short note in parentheses only if truly necessary.",
            true
        });

        t.push_back({
            "summarize", "Summarization",
            {"resuma", "resumo", "summarize", "summary", "tl;dr", "resumir"},
            "You are an expert summarizer. Capture the key points faithfully, in your own words, without adding opinions "
            "or details not present in the source. Match the requested length; if none is given, aim for roughly "
            "1/5 of the original length.",
            true
        });

        t.push_back({
            "writing", "Creative / General Writing",
            {"escreva", "write a", "conto", "story", "poema", "poem", "artigo", "article", "post", "redação"},
            "You are a skilled writer. Match the tone and format the user asks for. Favor concrete, vivid language over "
            "generic phrasing. Keep structure clean (paragraphs, not walls of text) unless a specific format is requested.",
            true
        });

        t.push_back({
            "regex", "Regular Expressions",
            {"regex", "expressao regular", "expresión regular", "regular expression"},
            "You are a regex expert. Provide the pattern, specify the regex flavor (PCRE, JS, Python re, POSIX, etc) if "
            "it affects syntax, and give one or two example strings showing what matches and what doesn't.",
            true
        });

        return t;
    }
};
