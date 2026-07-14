#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "LlamaServerManager.h"
#include "HttpClient.h"
#include "ModelDownloader.h"
#include "Localization.h"
#include "ThemeManager.h"
#include "ConfigManager.h"
#include "TemplateManager.h"
#include "ConversationManager.h"
#include "UiHelpers.h"
#include "Base64.h"
#include "FilePicker.h"
#include "PdfTextExtractor.h"
#include "AgentEngine.h"
#include "FolderPicker.h"
#include "WebSearch.h"

#include <thread>
#include <chrono>
#include <algorithm>
#include <set>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <shellapi.h> // ShellExecuteA (botao 'Ver log do servidor')

// ===================== Global application state =====================

static LlamaServerManager g_server;
static ConversationManager g_chatConvMgr("conversations");
static ConversationManager g_agentConvMgr("agent_projects");
static std::atomic<bool> g_darkMode{true};
static std::atomic<bool> g_thinkingEnabled{true};
static std::atomic<bool> g_webSearchEnabled{false};      // pesquisa DuckDuckGo antes de responder (Chat)
static std::atomic<bool> g_agentWebSearchEnabled{false}; // idem, para o modo Agente
// Sidebars de configuracao (botao de engrenagem). Fechados por padrao para
// deixar a area central inteira para a conversa.
static bool g_chatSidebarOpen = false;
static bool g_agentSidebarOpen = false;
static std::atomic<bool> g_isGenerating{false};
static std::atomic<bool> g_stopRequested{false};
static std::atomic<bool> g_serverRunning{false};
static std::atomic<bool> g_serverReady{false};

static std::mutex g_stateMutex; // guards live thinking/output buffers AND g_conversation
static std::string g_liveThinking;
static std::string g_liveOutput;
static Conversation g_conversation;

static char g_promptBuffer[8192] = "";
static char g_modelPathBuffer[512] = "";
static char g_mmprojPathBuffer[512] = "";
static std::string g_customSystemPromptBuffer =
    "You are a helpful, concise assistant. Answer directly and stay on topic.";
static int g_port = 8080;

static SamplingParams g_sampling;
static std::string g_extraServerArgs;

// --- Debate Mode: N models discuss the same question over R rounds before
// a final synthesized answer is produced. Each debater runs its own
// independent llama-server, so they can be different models entirely. ---
struct Debater {
    LlamaServerManager server;
    char modelPath[512] = "";
    char mmprojPath[512] = "";
    int port = 8090;
    std::atomic<bool> running{false};
    std::atomic<bool> ready{false};
};
static bool g_debateModeEnabled = false;
static int g_debateRounds = 2;
static std::vector<std::unique_ptr<Debater>> g_debaters;
static int g_pendingRemoveDebaterIdx = -1;

static void AddDebater() {
    auto d = std::make_unique<Debater>();
    d->port = 8090 + (int)g_debaters.size();
    g_debaters.push_back(std::move(d));
}

// --- Attachments (pending, not yet sent) ---
static bool g_pendingIsImage = false;
static std::string g_pendingImageBase64;
static std::string g_pendingImageMime;
static std::string g_pendingAttachFileName;
static std::atomic<bool> g_extractingPdf{false};
static std::string g_attachStatusMsg;

// --- Template system ---
static bool g_autoTemplate = true;
static int g_forcedTemplateIdx = -1;
static std::string g_activeTemplateName;
static std::string g_newTemplateName;
static std::string g_newTemplateKeywords;
static std::string g_newTemplateSystemPrompt;
static std::string g_templateStatusMsg;
static std::string g_pendingDeleteTemplateId;
// Edicao de templates: id em edicao + buffers do formulario inline
static std::string g_editingTemplateId;
static std::string g_editTemplateName, g_editTemplateKeywords, g_editTemplatePrompt;

// --- Resizable columns: share of width given to the Thinking panel ---
static float g_colRatioThinking = 0.28f;

// --- Reviewer model (optional second model that checks the Creator's work) ---
static LlamaServerManager g_reviewerServer;
static char g_reviewerModelPathBuffer[512] = "";
static char g_reviewerMmprojPathBuffer[512] = "";
static int g_reviewerPort = 8082;
static std::atomic<bool> g_reviewerServerRunning{false};
static std::atomic<bool> g_reviewerServerReady{false};
static std::atomic<bool> g_useReviewerModel{false};
static bool g_useCustomReviewerPrompt = false;
static std::string g_customReviewerPrompt;

// --- History tab ---
static std::vector<ConversationSummary> g_conversationList;
static std::vector<ConversationSummary> g_agentConversationList;
static std::string g_pendingDeleteAgentConvId;
static std::string g_projectsSearchBuffer;
static std::set<std::string> g_projectsSelectedIds;
static bool g_projectsShowDeleteAllConfirm = false;
static std::string g_pendingDeleteConvId;
static std::string g_historySearchBuffer;
static std::set<std::string> g_historySelectedIds;
static bool g_historyShowDeleteAllConfirm = false;

// --- Agent tab ---
static LlamaServerManager g_agentServer;
static char g_agentModelPathBuffer[512] = "";
static char g_agentMmprojPathBuffer[512] = "";
static int g_agentPort = 8081; // different default port so Chat and Agent servers can run side by side
static std::atomic<bool> g_agentServerRunning{false};
static std::atomic<bool> g_agentServerReady{false};
static std::atomic<bool> g_agentThinkingEnabled{true};
static std::atomic<bool> g_agentFreeMode{false}; // false = always ask before writes (default, safer)

static std::string g_agentProjectFolder;
static std::string g_agentTaskBuffer;
static std::string g_agentCurrentTaskText; // original task description, used for template matching
static bool g_agentAutoTemplate = true;
static int g_agentForcedTemplateIdx = -1;
static std::string g_agentActiveTemplateName;

// --- Agent task attachment (image only needs mmproj; PDF gets inlined as text) ---
static bool g_agentPendingIsImage = false;
static std::string g_agentPendingImageBase64;
static std::string g_agentPendingImageMime;
static std::string g_agentPendingAttachFileName;
static std::atomic<bool> g_agentExtractingPdf{false};
static std::string g_agentPdfPendingAppend;
static std::atomic<bool> g_agentPdfAppendReady{false};
static std::string g_agentAttachStatusMsg;

static std::mutex g_agentMutex; // guards everything below
static Conversation g_agentConversation;
static std::string g_agentLiveThinking;
static std::string g_agentLiveOutput;
static std::atomic<bool> g_agentRunning{false};    // task loop actively working
static std::atomic<bool> g_agentStopRequested{false};
static std::atomic<bool> g_agentAwaitingApproval{false};
static AgentAction g_agentPendingAction;
static std::atomic<int> g_agentApprovalDecision{0}; // 0=pending, 1=approved, 2=rejected
static std::string g_agentStatusMsg;
static int g_agentInvalidRetries = 0;

// --- Models tab ---
static char g_hfRepoBuffer[256] = "";
static char g_hfTokenBuffer[256] = "";
static bool g_showToken = false;
static std::string g_tokenStatusMsg;
static std::vector<std::string> g_availableFiles;
static int g_selectedFileIdx = -1;
static DownloadState g_downloadState;
static std::vector<std::string> g_localModels;
static std::atomic<bool> g_fetchingFiles{false};
static std::string g_fetchError;

// ===================== Helpers =====================

static void RefreshLocalModels() {
    g_localModels.clear();
    std::filesystem::path modelsDir = "models";
    if (!std::filesystem::exists(modelsDir)) std::filesystem::create_directories(modelsDir);
    for (auto& entry : std::filesystem::directory_iterator(modelsDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
            g_localModels.push_back(entry.path().filename().string());
        }
    }
}

static void RefreshConversationList() {
    g_conversationList = g_chatConvMgr.ListAll();
}

static void RefreshAgentConversationList() {
    g_agentConversationList = g_agentConvMgr.ListAll();
}

// Exports a conversation to a plain Markdown file, one section per
// message, with code fences preserved as-is (they're already Markdown
// fences in the raw content).
static bool ExportConversationToMarkdown(const Conversation& conv, const std::string& outPath) {
    try {
        std::ofstream f(outPath, std::ios::binary);
        if (!f.is_open()) return false;

        std::string title = conv.title.empty() ? "Conversation" : conv.title;
        f << "# " << title << "\n\n";

        for (auto& m : conv.messages) {
            std::string roleLabel = (m.role == "user") ? "You" : "Assistant";
            f << "## " << roleLabel << "\n\n";
            if (!m.attachedFileName.empty()) {
                f << "*Attached: " << m.attachedFileName << "*\n\n";
            }
            if (!m.thinking.empty()) {
                f << "<details>\n<summary>Reasoning / discussion</summary>\n\n";
                f << m.thinking << "\n\n</details>\n\n";
            }
            f << m.content << "\n\n---\n\n";
        }
        return true;
    } catch (...) {
        return false;
    }
}

static std::string ResolveSystemPrompt(const std::string& userInput, std::string& templateNameOut) {
    auto& mgr = TemplateManager::Instance();
    const auto& templates = mgr.All();

    if (g_forcedTemplateIdx >= 0 && g_forcedTemplateIdx < (int)templates.size()) {
        templateNameOut = templates[g_forcedTemplateIdx].displayName;
        return templates[g_forcedTemplateIdx].systemPrompt;
    }

    if (g_autoTemplate) {
        int idx = mgr.SelectBest(userInput);
        if (idx >= 0) {
            templateNameOut = templates[idx].displayName;
            return templates[idx].systemPrompt;
        }
    }

    templateNameOut.clear();
    return g_customSystemPromptBuffer;
}

static std::vector<ChatTurn> BuildHistory(const std::string& systemPrompt) {
    std::vector<ChatTurn> history;
    if (!systemPrompt.empty()) history.push_back({ "system", systemPrompt, "", "" });

    std::lock_guard<std::mutex> lock(g_stateMutex);
    for (auto& m : g_conversation.messages) {
        history.push_back({ m.role, m.content, m.imageBase64, m.imageMime });
    }
    return history;
}

// Asks the model itself for a short plain-text summary of the given
// history, used to compact context that's getting too long. Thinking is
// forced off for this call to keep it fast and avoid wasting tokens.
static std::string RequestContextSummary(std::vector<ChatTurn> historyToSummarize, int port,
                                          const SamplingParams& sampling, std::function<bool()> shouldStop) {
    historyToSummarize.push_back({
        "user",
        "The conversation above is getting long. Reply with ONLY a concise plain-text summary (roughly 5-10 "
        "sentences, no special format) covering: what has been accomplished so far, what still needs to be done, "
        "and any details that are important to remember to continue correctly. Do not include anything else.",
        "", ""
    });

    std::string summary;
    HttpClient::ChatStream(
        historyToSummarize, port, sampling, /*thinking=*/false,
        [&](bool isReasoning, const std::string& text) { if (!isReasoning) summary += text; },
        shouldStop
    );
    return summary;
}

// Shared by both Chat and Agent: if the estimated token count of the
// conversation is approaching the server's context size, replaces the
// conversation's message history with a single synthetic message
// containing a model-generated summary, so the conversation can keep going
// indefinitely instead of eventually overflowing the context window (which
// otherwise leaves less and less room for the model's own output, and can
// even make it produce empty responses once there's barely any room left).
// Returns true if compaction happened.
static bool MaybeCompactConversation(Conversation& conv, std::mutex& mtx, const std::string& systemPrompt,
                                      int port, const SamplingParams& sampling, std::function<bool()> shouldStop,
                                      ConversationManager& convMgr, std::string& statusOut) {
    std::vector<ChatTurn> history;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!systemPrompt.empty()) history.push_back({ "system", systemPrompt, "", "" });
        for (auto& m : conv.messages) history.push_back({ m.role, m.content, "", "" });
    }

    int estTokens = 0;
    for (auto& t : history) estTokens += AgentEngine::EstimateTokens(t.content);
    int threshold = (int)(sampling.ctxSize * 0.75f);
    if (estTokens < threshold) return false;

    std::string summary = RequestContextSummary(history, port, sampling, shouldStop);
    if (AgentEngine::TrimStr(summary).empty()) return false; // summarization failed; don't destroy history for nothing

    std::lock_guard<std::mutex> lock(mtx);
    ChatMessage compactMsg;
    compactMsg.role = "user";
    compactMsg.content = "[Context was getting long, so it was automatically summarized to keep going.]\n"
                          "Summary of the conversation so far:\n" + summary + "\n\nContinue from here.";
    conv.messages.clear();
    conv.messages.push_back(compactMsg);
    convMgr.Save(conv);
    statusOut = "Context was automatically summarized to continue within the model's context window.";
    return true;
}

static void PdfExtractWorker(std::string path, std::string fileName) {
    g_extractingPdf = true;
    try {
        bool success = false;
        std::string text = PdfTextExtractor::Extract(path, success);

        if (success) {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            std::string block = "\n\n--- Attached PDF: " + fileName + " ---\n" + text + "\n--- end of " + fileName + " ---\n";
            size_t curLen = strlen(g_promptBuffer);
            size_t roomLeft = sizeof(g_promptBuffer) - curLen - 1;
            strncat(g_promptBuffer, block.c_str(), roomLeft);
            g_attachStatusMsg = "Attached " + fileName + " (" + std::to_string(text.size()) + " characters added to the prompt).";
        } else {
            g_attachStatusMsg = "Couldn't extract text from " + fileName +
                " - it may be a scanned/image-based PDF. Try pasting the text manually instead.";
        }
    } catch (std::exception& e) {
        g_attachStatusMsg = std::string("Couldn't process ") + fileName + ": " + e.what();
    } catch (...) {
        g_attachStatusMsg = "Couldn't process " + fileName + " due to an unexpected error.";
    }
    g_extractingPdf = false;
}

static void HandleAgentAttachClick() {
    std::string path = FilePicker::OpenImageOrPdfDialog();
    if (path.empty()) return;

    size_t slash = path.find_last_of("\\/");
    std::string fileName = (slash == std::string::npos) ? path : path.substr(slash + 1);

    if (FilePicker::IsImage(path)) {
        auto bytes = PdfTextExtractor::ReadFileBytes(path);
        if (bytes.empty()) {
            g_agentAttachStatusMsg = "Couldn't read " + fileName + ".";
            return;
        }
        g_agentPendingImageBase64 = Base64::Encode(bytes);
        g_agentPendingImageMime = FilePicker::MimeTypeFor(path);
        g_agentPendingAttachFileName = fileName;
        g_agentPendingIsImage = true;
        g_agentAttachStatusMsg = "Image ready: " + fileName +
            " (requires a vision model + mmproj file loaded on the Agent's server to actually be understood).";
    } else if (FilePicker::IsPdf(path)) {
        g_agentPendingIsImage = false;
        g_agentPendingImageBase64.clear();
        g_agentAttachStatusMsg = "Extracting text from " + fileName + "...";
        g_agentExtractingPdf = true;
        std::thread([path, fileName]() {
            bool success = false;
            std::string text = PdfTextExtractor::Extract(path, success);
            if (success) {
                // Don't touch g_agentTaskBuffer directly from this
                // background thread - ImGui's InputTextMultiline on the
                // main thread reads/writes that same std::string every
                // frame, and mutating it concurrently is a data race.
                // Stash the text and let DrawAgentTab() apply it safely.
                g_agentPdfPendingAppend = "\n\n--- Attached PDF: " + fileName + " ---\n" + text +
                                           "\n--- end of " + fileName + " ---\n";
                g_agentPdfAppendReady = true;
                g_agentAttachStatusMsg = "Attached " + fileName + " (" + std::to_string(text.size()) + " characters added to the task).";
            } else {
                g_agentAttachStatusMsg = "Couldn't extract text from " + fileName +
                    " - it may be a scanned/image-based PDF. Try pasting the text manually instead.";
            }
            g_agentExtractingPdf = false;
        }).detach();
    }
}

static void ClearAgentPendingAttachment() {
    g_agentPendingIsImage = false;
    g_agentPendingImageBase64.clear();
    g_agentPendingImageMime.clear();
    g_agentPendingAttachFileName.clear();
    g_agentAttachStatusMsg.clear();
}

static void HandleAttachClick() {
    std::string path = FilePicker::OpenImageOrPdfDialog();
    if (path.empty()) return;

    size_t slash = path.find_last_of("\\/");
    std::string fileName = (slash == std::string::npos) ? path : path.substr(slash + 1);

    if (FilePicker::IsImage(path)) {
        auto bytes = PdfTextExtractor::ReadFileBytes(path);
        if (bytes.empty()) {
            g_attachStatusMsg = "Couldn't read " + fileName + ".";
            return;
        }
        g_pendingImageBase64 = Base64::Encode(bytes);
        g_pendingImageMime = FilePicker::MimeTypeFor(path);
        g_pendingAttachFileName = fileName;
        g_pendingIsImage = true;
        g_attachStatusMsg = "Image ready: " + fileName +
            " (requires a vision model + mmproj file loaded to actually be understood).";
    } else if (FilePicker::IsPdf(path)) {
        g_pendingIsImage = false;
        g_pendingImageBase64.clear();
        g_attachStatusMsg = "Extracting text from " + fileName + "...";
        std::thread(PdfExtractWorker, path, fileName).detach();
    }
}

static void ClearPendingAttachment() {
    g_pendingIsImage = false;
    g_pendingImageBase64.clear();
    g_pendingImageMime.clear();
    g_pendingAttachFileName.clear();
    g_attachStatusMsg.clear();
}

static void GenerateWorker(std::string systemPrompt, SamplingParams params, bool thinking,
                           std::string webSearchQuery) {
    g_isGenerating = true;
    g_stopRequested = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_liveThinking.clear();
        g_liveOutput.clear();
    }

    // ---- Pesquisa na web (opcional, checkbox no Chat) ----
    // Busca no DuckDuckGo, baixa as melhores paginas e injeta o material
    // como contexto EFEMERO: entra no prompt desta geracao, mas nao e
    // gravado na conversa (senao cada save carregaria KBs de paginas).
    std::string webContext;
    std::vector<std::string> webSources;
    if (!webSearchQuery.empty()) {
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_liveOutput = Localization::T("web_searching");
        }
        webContext = WebSearch::BuildContext(webSearchQuery, webSources);
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_liveOutput.clear();
            if (webContext.empty())
                g_liveThinking = std::string("(") + Localization::T("web_search_failed") + ")";
        }
    }

    try {
        bool escalatedThinkingOff = false;
        int escalatedMaxTokens = 0;
        int emptyStreak = 0;
        const int maxAttempts = 6;
        std::string finalOutput, finalThinking;

        for (int attempt = 0; attempt < maxAttempts && !g_stopRequested.load(); attempt++) {
            std::string compactionNote;
            bool compacted = MaybeCompactConversation(
                g_conversation, g_stateMutex, systemPrompt, g_port, params,
                []() { return g_stopRequested.load(); }, g_chatConvMgr, compactionNote
            );

            SamplingParams attemptParams = params;
            if (escalatedMaxTokens > 0) attemptParams.maxTokens = escalatedMaxTokens;
            bool useThinking = escalatedThinkingOff ? false : thinking;

            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                g_liveOutput.clear();
                g_liveThinking = compacted ? ("(" + compactionNote + ")") : "";
            }

            std::vector<ChatTurn> history = BuildHistory(systemPrompt);

            // Anexa os resultados da web ao ULTIMO turno do usuario (so
            // nesta requisicao; a mensagem salva permanece limpa).
            if (!webContext.empty()) {
                for (auto it = history.rbegin(); it != history.rend(); ++it) {
                    if (it->role == "user") {
                        it->content += "\n\n" + webContext;
                        break;
                    }
                }
            }

            HttpClient::ChatStream(
                history, g_port, attemptParams, useThinking,
                [](bool isReasoning, const std::string& text) {
                    std::lock_guard<std::mutex> lock(g_stateMutex);
                    if (isReasoning) g_liveThinking += text;
                    else g_liveOutput += text;
                },
                []() { return g_stopRequested.load(); }
            );

            std::string output;
            std::string reasoning;
            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                output = g_liveOutput;
                reasoning = g_liveThinking;
            }

            if (!output.empty() || g_stopRequested.load()) {
                finalOutput = output;
                finalThinking = reasoning;
                break;
            }

            // Empty response: don't just show a blank bubble and make the
            // user start a new conversation to recover. Adapt and retry
            // automatically instead, the same way the Agent does:
            // (1) turn off Thinking Mode + raise the token budget, since a
            // model burning its whole output budget on reasoning is the
            // most common cause of this; (2) if that's still not enough,
            // force a context summary/reset and try again.
            emptyStreak++;
            if (!escalatedThinkingOff) {
                escalatedThinkingOff = true;
                escalatedMaxTokens = std::min(params.maxTokens * 2, 8192);
                continue;
            }
            if (emptyStreak >= 3) {
                SamplingParams tinyCtx = params;
                tinyCtx.ctxSize = 1; // forces MaybeCompactConversation's threshold check to always trigger
                std::string forceNote;
                MaybeCompactConversation(
                    g_conversation, g_stateMutex, systemPrompt, g_port, tinyCtx,
                    []() { return g_stopRequested.load(); }, g_chatConvMgr, forceNote
                );
            }
        }

        if (finalOutput.empty() && !g_stopRequested.load()) {
            finalOutput = "(The model kept returning empty responses, even after automatically disabling "
                          "Thinking Mode, increasing the token budget, and summarizing the conversation. Try "
                          "raising Max Tokens further in the Parameters tab, or use a different model.)";
        }

        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            ChatMessage assistantMsg;
            assistantMsg.role = "assistant";
            assistantMsg.content = finalOutput;
            // Cita as fontes da pesquisa web no fim da resposta, para o
            // usuario poder conferir de onde a informacao veio.
            if (!webSources.empty()) {
                assistantMsg.content += "\n\n" + std::string(Localization::T("web_sources_label"));
                for (auto& u : webSources) assistantMsg.content += "\n- " + u;
            }
            assistantMsg.thinking = finalThinking;
            g_conversation.messages.push_back(assistantMsg);
            g_chatConvMgr.Save(g_conversation);

            // Clear the live buffers now that this turn's text has been
            // committed to the message list — otherwise the transcript would
            // render the same answer twice: once from the saved message, once
            // from the leftover "live" streaming view.
            g_liveOutput.clear();
            g_liveThinking.clear();
        }
        RefreshConversationList();
    } catch (std::exception& e) {
        // Never let an exception escape a detached thread — that calls
        // std::terminate() and silently kills the whole app.
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_liveOutput += "\n[Error: " + std::string(e.what()) + "]";
    } catch (...) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_liveOutput += "\n[An unexpected internal error occurred.]";
    }

    g_isGenerating = false;
}

// Debate Mode: every ready debater answers the question, then in each
// following round every debater sees the whole group's previous answers
// and is asked to refine/critique, and finally the first debater is asked
// to synthesize everything into one best final answer. The full back-and-
// forth is kept in ChatMessage.thinking (shown in the live Thinking panel,
// reusing that UI) while ChatMessage.content holds only the synthesized
// final answer, same as a normal single-model reply.
static void DebateWorker(std::string systemPrompt, std::string userPrompt) {
    g_isGenerating = true;
    g_stopRequested = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_liveThinking.clear();
        g_liveOutput.clear();
    }

    try {
        std::vector<Debater*> ready;
        for (auto& d : g_debaters) {
            if (d->running.load() && d->ready.load()) ready.push_back(d.get());
        }

        if (ready.empty()) {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_liveOutput = "(No debaters are online. Start at least two debater servers in the Chat tab and try again.)";
        } else {
            // transcript[i] = debater i's latest contribution so far, shown to
            // every other debater in the next round.
            std::vector<std::string> transcript(ready.size());
            std::string fullLog;

            for (int round = 1; round <= g_debateRounds && !g_stopRequested.load(); round++) {
                {
                    std::lock_guard<std::mutex> lock(g_stateMutex);
                    fullLog += "\n=== Round " + std::to_string(round) + " ===\n";
                    g_liveThinking = fullLog;
                }

                for (size_t i = 0; i < ready.size() && !g_stopRequested.load(); i++) {
                    std::ostringstream roundPrompt;
                    roundPrompt << "You are Debater " << (i + 1) << " in a multi-model discussion working "
                                << "together to produce the best possible answer to the user's request below. "
                                << "Round " << round << " of " << g_debateRounds << ".\n\n"
                                << "User's request:\n" << userPrompt << "\n\n";

                    if (round == 1) {
                        roundPrompt << "Give your own answer to this request.";
                    } else {
                        roundPrompt << "Here is what every debater (including you) said in the previous round:\n";
                        for (size_t j = 0; j < transcript.size(); j++) {
                            roundPrompt << "\n--- Debater " << (j + 1) << " ---\n" << transcript[j] << "\n";
                        }
                        roundPrompt << "\nCritique the others where they're wrong or incomplete, and give your "
                                    << "own improved/refined answer, incorporating anything good from the "
                                    << "others. Be direct about disagreements.";
                    }

                    std::vector<ChatTurn> history;
                    history.push_back({ "system", systemPrompt, "", "" });
                    history.push_back({ "user", roundPrompt.str(), "", "" });

                    std::string reply;
                    HttpClient::ChatStream(
                        history, ready[i]->port, g_sampling, g_thinkingEnabled.load(),
                        [&](bool isReasoning, const std::string& text) {
                            if (!isReasoning) reply += text;
                            std::lock_guard<std::mutex> lock(g_stateMutex);
                            g_liveThinking = fullLog + "\n--- Debater " + std::to_string(i + 1) +
                                             " (Round " + std::to_string(round) + ") ---\n" + reply;
                        },
                        []() { return g_stopRequested.load(); }
                    );

                    transcript[i] = reply;
                    fullLog += "\n--- Debater " + std::to_string(i + 1) + " (Round " + std::to_string(round) +
                               ") ---\n" + reply + "\n";
                    {
                        std::lock_guard<std::mutex> lock(g_stateMutex);
                        g_liveThinking = fullLog;
                    }
                }
            }

            // Final synthesis: ask the first debater to combine everything
            // into one answer, given the full multi-round transcript.
            std::string finalAnswer;
            if (!g_stopRequested.load()) {
                std::ostringstream synthPrompt;
                synthPrompt << "You just participated in a " << g_debateRounds << "-round discussion with other "
                            << "models to answer this request:\n" << userPrompt << "\n\n"
                            << "Full discussion transcript:\n" << fullLog << "\n\n"
                            << "Now write the single best final answer, combining the strongest points from the "
                            << "whole discussion. Respond with ONLY the final answer itself - no meta-commentary "
                            << "about the debate.";

                std::vector<ChatTurn> history;
                history.push_back({ "system", systemPrompt, "", "" });
                history.push_back({ "user", synthPrompt.str(), "", "" });

                HttpClient::ChatStream(
                    history, ready[0]->port, g_sampling, false,
                    [&](bool isReasoning, const std::string& text) { if (!isReasoning) finalAnswer += text; },
                    []() { return g_stopRequested.load(); }
                );
            }

            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_liveOutput = finalAnswer.empty() && !fullLog.empty()
                ? "(The debate finished but the final synthesis step failed - see the discussion panel for what each model said.)"
                : finalAnswer;
        }

        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            ChatMessage assistantMsg;
            assistantMsg.role = "assistant";
            assistantMsg.content = g_liveOutput;
            assistantMsg.thinking = g_liveThinking;
            g_conversation.messages.push_back(assistantMsg);
            g_chatConvMgr.Save(g_conversation);
            g_liveOutput.clear();
            g_liveThinking.clear();
        }
        RefreshConversationList();
    } catch (std::exception& e) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_liveOutput += "\n[Error: " + std::string(e.what()) + "]";
    } catch (...) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_liveOutput += "\n[An unexpected internal error occurred.]";
    }

    g_isGenerating = false;
}

// One HTTP round-trip: sends the current agent conversation to the model
// and streams the response into the live buffers. Returns the final text.
static std::string ResolveAgentExpertise() {
    auto& mgr = TemplateManager::Instance();
    const auto& templates = mgr.All();

    if (g_agentForcedTemplateIdx >= 0 && g_agentForcedTemplateIdx < (int)templates.size()) {
        g_agentActiveTemplateName = templates[g_agentForcedTemplateIdx].displayName;
        return templates[g_agentForcedTemplateIdx].systemPrompt;
    }

    if (g_agentAutoTemplate) {
        int idx = mgr.SelectBest(g_agentCurrentTaskText);
        if (idx >= 0) {
            g_agentActiveTemplateName = templates[idx].displayName;
            return templates[idx].systemPrompt;
        }
    }

    g_agentActiveTemplateName.clear();
    return "";
}

static std::string AgentGenerateOnce(bool forceThinkingOff, int maxTokensOverride) {
    {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        g_agentLiveThinking.clear();
        g_agentLiveOutput.clear();
    }

    // Re-checked before every single step (not just once at task start), so
    // if the conversation drifts toward a different kind of sub-task the
    // agent still gets the most relevant built-in/custom expertise applied.
    std::string expertise = ResolveAgentExpertise();
    std::string systemPrompt = AgentEngine::BuildAgentSystemPrompt(g_agentProjectFolder, expertise);

    SamplingParams params = g_sampling;
    if (maxTokensOverride > 0) params.maxTokens = maxTokensOverride;

    // Automatically summarize and reset the conversation if it's getting
    // close to the model's context window, so a long-running task doesn't
    // eventually run out of room for the model's own output. This handles
    // the "input context is too full" case; the "output got cut off by
    // reasoning" case is handled separately by the caller via the
    // forceThinkingOff/maxTokensOverride escalation.
    std::string compactionNote;
    bool compacted = MaybeCompactConversation(
        g_agentConversation, g_agentMutex, systemPrompt, g_agentPort, params,
        []() { return g_agentStopRequested.load(); }, g_agentConvMgr, compactionNote
    );
    if (compacted) {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        g_agentStatusMsg = compactionNote;
    }

    std::vector<ChatTurn> history;
    {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        history.push_back({ "system", systemPrompt, "", "" });
        for (auto& m : g_agentConversation.messages) {
            history.push_back({ m.role, m.content, m.imageBase64, m.imageMime });
        }
    }

    bool useThinking = forceThinkingOff ? false : g_agentThinkingEnabled.load();

    HttpClient::ChatStream(
        history, g_agentPort, params, useThinking,
        [](bool isReasoning, const std::string& text) {
            std::lock_guard<std::mutex> lock(g_agentMutex);
            if (isReasoning) g_agentLiveThinking += text;
            else g_agentLiveOutput += text;
        },
        []() { return g_agentStopRequested.load(); }
    );

    std::lock_guard<std::mutex> lock(g_agentMutex);
    return g_agentLiveOutput;
}

// The autonomous loop: generate -> parse action -> execute (or wait for
// approval) -> feed the result back -> repeat, until the model signals
// "done", the user stops it, or too many invalid responses happen in a row.
// Normalizes the reviewer's reply for a loose "APPROVED" check (handles
// "Approved.", "APPROVED!", trailing whitespace, etc without needing the
// model to match a strict template).
static std::string ToLowerNoPunct(const std::string& s) {
    std::string out = AgentEngine::ToLowerStr(s);
    while (!out.empty() && (out.back() == '.' || out.back() == '!' || out.back() == ' ')) out.pop_back();
    return out;
}

// Sends the just-written file to the (optional) second Reviewer model,
// along with the original task and a listing of the project so far, and
// returns either "APPROVED" or a specific list of problems to fix. Runs on
// its own server/port, completely independent from the Creator model.
static std::string ReviewFile(const std::string& taskText, const std::string& filePath,
                               const std::string& fileContent, AgentActionType actionType) {
    auto files = AgentEngine::ListFilesRecursive(g_agentProjectFolder);
    std::ostringstream listing;
    for (auto& f : files) listing << "- " << f << "\n";

    std::string reviewerInstructions = g_useCustomReviewerPrompt && !g_customReviewerPrompt.empty()
        ? g_customReviewerPrompt
        : AgentEngine::DefaultReviewerSystemPrompt();

    // Always prepend the user's original request, clearly labeled, before
    // the reviewer's instructions (default or custom) - this way the
    // reviewer never loses track of what was actually asked, and can hold
    // the Creator model to that regardless of which prompt is in use.
    std::string systemPrompt =
        "This is the user's original request (the overall task the Creator model is working on):\n" +
        taskText + "\n\n" + reviewerInstructions;

    std::string actionLabel = (actionType == AgentActionType::CreateFile)
        ? "CREATE_FILE (this file is new)"
        : "EDIT_FILE (this file already existed and was just modified)";

    std::ostringstream userMsg;
    userMsg << "Action just performed: " << actionLabel << "\n\n"
            << "Files currently in the project folder (verified from disk):\n" << listing.str() << "\n"
            << "File just written: " << filePath << "\n"
            << "Its contents:\n```\n" << fileContent << "\n```\n\n"
            << "First, confirm \"" << filePath << "\" actually appears in the file listing above - if it "
            << "doesn't, that itself is a critical failure to flag. Then review the content as instructed.";

    std::vector<ChatTurn> history;
    history.push_back({ "system", systemPrompt, "", "" });
    history.push_back({ "user", userMsg.str(), "", "" });

    std::string reviewText;
    HttpClient::ChatStream(
        history, g_reviewerPort, g_sampling, /*thinking=*/false,
        [&](bool isReasoning, const std::string& text) { if (!isReasoning) reviewText += text; },
        []() { return g_agentStopRequested.load(); }
    );
    return reviewText;
}

static void AgentWorker(std::string initialTask) {
    g_agentRunning = true;
    g_agentStopRequested = false;
    g_agentInvalidRetries = 0;

    // ---- Pesquisa na web (checkbox no painel do Agente) ----
    // Diferente do Chat, aqui o material pesquisado E gravado na conversa
    // do projeto: o agente trabalha em varios passos e precisa reconsultar
    // essa informacao nas iteracoes seguintes. Por isso usamos menos
    // resultados e paginas mais curtas, para nao inflar demais o contexto.
    if (g_agentWebSearchEnabled.load()) {
        {
            std::lock_guard<std::mutex> lock(g_agentMutex);
            g_agentStatusMsg = Localization::T("web_searching");
        }
        std::vector<std::string> srcs;
        std::string ctx = WebSearch::BuildContext(initialTask, srcs, /*maxResults=*/2, /*maxCharsPerPage=*/3500);
        {
            std::lock_guard<std::mutex> lock(g_agentMutex);
            g_agentStatusMsg.clear();
        }
        if (!ctx.empty()) initialTask += "\n\n" + ctx;
    }

    {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        ChatMessage taskMsg;
        taskMsg.role = "user";
        taskMsg.content = initialTask;
        if (g_agentPendingIsImage) {
            taskMsg.imageBase64 = g_agentPendingImageBase64;
            taskMsg.imageMime = g_agentPendingImageMime;
            taskMsg.attachedFileName = g_agentPendingAttachFileName;
        }
        g_agentConversation.messages.push_back(taskMsg);
    }
    ClearAgentPendingAttachment();

    const int maxInvalidRetries = 3;
    bool doneReviewDone = false;

    // Escalation state for repeated empty/invalid responses. Rather than
    // giving up, the agent adapts: first it just asks again, then it turns
    // off Thinking Mode (a model burning its whole output budget on
    // reasoning is the most common cause of empty responses) and raises
    // the token budget, then it also forces a context summary/reset. Only
    // if ALL of that still fails does it finally stop.
    bool escalatedThinkingOff = false;
    int escalatedMaxTokens = 0;
    int consecutiveEmptyStreak = 0;
    const int maxTotalAttempts = 12; // hard safety ceiling across all escalation levels

    try {
        while (!g_agentStopRequested.load()) {
        std::string modelOutput = AgentGenerateOnce(escalatedThinkingOff, escalatedMaxTokens);

        {
            std::lock_guard<std::mutex> lock(g_agentMutex);
            ChatMessage assistantMsg;
            assistantMsg.role = "assistant";
            assistantMsg.content = modelOutput;
            assistantMsg.thinking = g_agentLiveThinking;
            g_agentConversation.messages.push_back(assistantMsg);
            g_agentLiveOutput.clear();
            g_agentLiveThinking.clear();
        }

        if (g_agentStopRequested.load()) break;

        AgentAction action = AgentEngine::ParseAction(modelOutput);

        if (action.type == AgentActionType::Invalid) {
            g_agentInvalidRetries++;
            bool wasEmpty = AgentEngine::TrimStr(modelOutput).empty();
            if (wasEmpty) consecutiveEmptyStreak++;
            else consecutiveEmptyStreak = 0;

            if (g_agentInvalidRetries > maxTotalAttempts) {
                std::lock_guard<std::mutex> lock(g_agentMutex);
                g_agentStatusMsg = "Agent stopped after many attempts, even with Thinking Mode disabled and a "
                                    "larger token budget. This model may not be reliable enough for autonomous "
                                    "action-taking - try a different (ideally coding-tuned) model.";
                break;
            }

            // Escalation step 1: after a couple of empty responses in a
            // row, stop relying on the model "trying harder" and instead
            // change the conditions that are likely causing it to run out
            // of room: turn off reasoning for this task and double the
            // per-response token budget.
            if (wasEmpty && consecutiveEmptyStreak >= 2 && !escalatedThinkingOff) {
                escalatedThinkingOff = true;
                escalatedMaxTokens = std::min(g_sampling.maxTokens * 2, 8192);
                std::lock_guard<std::mutex> lock(g_agentMutex);
                g_agentStatusMsg = "Kept getting empty responses, so Thinking Mode was automatically turned off "
                                    "for this task and the token budget was increased - retrying automatically.";
                ChatMessage errMsg;
                errMsg.role = "user";
                errMsg.content =
                    "Your last several responses were empty. Skip reasoning entirely for this step and respond "
                    "immediately with ONLY the action block below:\n\n"
                    "```action\nACTION: create_file\nPATH: relative/path.ext\nSUMMARY: short note\n"
                    "CONTENT_START\n...full file content here, verbatim...\nCONTENT_END\n```";
                g_agentConversation.messages.push_back(errMsg);
                continue;
            }

            // Escalation step 2: still failing even without reasoning and
            // with more tokens? The context itself may be the problem (even
            // if it hasn't hit our proactive compaction threshold yet) -
            // force a summary-and-reset right now instead of waiting.
            if (wasEmpty && consecutiveEmptyStreak >= 4) {
                std::string forceNote;
                std::string dummySystemPrompt = AgentEngine::BuildAgentSystemPrompt(g_agentProjectFolder, "");
                SamplingParams forcedParams = g_sampling;
                forcedParams.maxTokens = escalatedMaxTokens > 0 ? escalatedMaxTokens : g_sampling.maxTokens;
                bool forced;
                {
                    // Bypass the normal 75% threshold: force a compaction
                    // unconditionally by temporarily treating ctxSize as
                    // tiny for this one check.
                    SamplingParams tinyCtx = forcedParams;
                    tinyCtx.ctxSize = 1;
                    forced = MaybeCompactConversation(
                        g_agentConversation, g_agentMutex, dummySystemPrompt, g_agentPort, tinyCtx,
                        []() { return g_agentStopRequested.load(); }, g_agentConvMgr, forceNote
                    );
                }
                std::lock_guard<std::mutex> lock(g_agentMutex);
                g_agentStatusMsg = forced
                    ? "Still getting empty responses, so the conversation was force-summarized and reset - "
                      "continuing automatically."
                    : "Still getting empty responses after adjustments; continuing to retry.";
                consecutiveEmptyStreak = 0;
                continue;
            }

            std::lock_guard<std::mutex> lock(g_agentMutex);
            ChatMessage errMsg;
            errMsg.role = "user";
            if (wasEmpty) {
                errMsg.content =
                    "Your response was empty. If you were reasoning, wrap it up quickly and make sure to "
                    "actually output the action block below - don't spend your whole response on reasoning "
                    "alone. Respond now with ONLY this:\n\n"
                    "```action\n"
                    "ACTION: create_file\n"
                    "PATH: relative/path.ext\n"
                    "SUMMARY: short note\n"
                    "CONTENT_START\n"
                    "...full file content here, verbatim...\n"
                    "CONTENT_END\n"
                    "```";
            } else {
                errMsg.content =
                    "That response was not in the correct format - do NOT use JSON. Respond with ONLY a single "
                    "```action fenced block using this EXACT plain-text format (no quotes, no escaping needed):\n\n"
                    "```action\n"
                    "ACTION: create_file\n"
                    "PATH: relative/path.ext\n"
                    "SUMMARY: short note\n"
                    "CONTENT_START\n"
                    "...full file content here, verbatim...\n"
                    "CONTENT_END\n"
                    "```\n\n"
                    "(For read_file/list_files, only ACTION and PATH are needed, no CONTENT block. "
                    "For done, only ACTION and SUMMARY are needed.) Try again now, following this format exactly.";
            }
            g_agentConversation.messages.push_back(errMsg);
            continue;
        }

        g_agentInvalidRetries = 0;
        consecutiveEmptyStreak = 0;

        if (action.type == AgentActionType::Done) {
            if (!doneReviewDone) {
                // Don't just trust the model's own claim that it's finished —
                // re-read what's actually on disk and make it double-check
                // before accepting "done" as final.
                auto files = AgentEngine::ListFilesRecursive(g_agentProjectFolder);
                std::ostringstream listing;
                if (files.empty()) {
                    listing << "(the project folder is currently empty)";
                } else {
                    for (auto& f : files) listing << "- " << f << "\n";
                }

                std::lock_guard<std::mutex> lock(g_agentMutex);
                ChatMessage reviewMsg;
                reviewMsg.role = "user";
                reviewMsg.content =
                    "You said the task is done. Before finishing, here is what's actually in the project "
                    "folder right now:\n" + listing.str() +
                    "\nDouble-check this against the original request. Use read_file on any file you want to "
                    "verify. If everything is complete and correct, respond with the \"done\" action again to "
                    "confirm final completion. If something is missing, incomplete, or wrong, fix it now with "
                    "create_file/edit_file instead.";
                g_agentConversation.messages.push_back(reviewMsg);
                doneReviewDone = true;
                continue;
            }

            std::lock_guard<std::mutex> lock(g_agentMutex);
            g_agentStatusMsg = action.summary.empty() ? "Task complete (verified)." : action.summary;
            g_agentConvMgr.Save(g_agentConversation);
            break;
        }

        // If the model actually fixed/changed something after a "done"
        // claim, require a fresh review pass once it claims done again.
        // (Read-only actions like read_file/list_files don't reset this —
        // those are exactly what we want it doing DURING the review.)
        if (action.type == AgentActionType::CreateFile || action.type == AgentActionType::EditFile ||
            action.type == AgentActionType::DeleteFile) {
            doneReviewDone = false;
        }

        if (action.type == AgentActionType::ReadFile || action.type == AgentActionType::ListFiles) {
            bool success = false;
            std::string result = AgentEngine::ExecuteReadOnly(action, g_agentProjectFolder, success);

            std::lock_guard<std::mutex> lock(g_agentMutex);
            ChatMessage toolMsg;
            toolMsg.role = "user";
            toolMsg.content = "[Result of " +
                std::string(action.type == AgentActionType::ReadFile ? "read_file" : "list_files") +
                " \"" + action.path + "\"]\n" + result;
            g_agentConversation.messages.push_back(toolMsg);
            continue;
        }

        // create_file / edit_file / delete_file: all destructive, gate
        // behind free mode or approval.
        bool freeMode = g_agentFreeMode.load();
        bool approved = freeMode;

        if (!freeMode) {
            {
                std::lock_guard<std::mutex> lock(g_agentMutex);
                g_agentPendingAction = action;
            }
            g_agentApprovalDecision = 0;
            g_agentAwaitingApproval = true;

            while (g_agentApprovalDecision.load() == 0 && !g_agentStopRequested.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            g_agentAwaitingApproval = false;

            if (g_agentStopRequested.load()) break;
            approved = (g_agentApprovalDecision.load() == 1);
        }

        std::string resultText;
        if (approved) {
            if (action.type == AgentActionType::DeleteFile) {
                std::string err;
                bool ok = AgentEngine::ExecuteDelete(action, g_agentProjectFolder, err);
                resultText = ok
                    ? "[Confirmed: \"" + action.path + "\" was deleted and no longer exists on disk.]"
                    : "[Action FAILED: " + err + "]";
            } else {
                std::filesystem::path absPath;
                std::string err;
                bool ok = AgentEngine::ExecuteWrite(action, g_agentProjectFolder, absPath, err);
                if (ok) {
                    std::ifstream verify(absPath, std::ios::binary);
                    std::ostringstream verifyBuf;
                    verifyBuf << verify.rdbuf();
                    std::string writtenContent = verifyBuf.str();

                    bool useReviewer = g_useReviewerModel.load() && g_reviewerServerRunning.load();
                    if (useReviewer) {
                        std::string reviewFeedback = ReviewFile(g_agentCurrentTaskText, action.path, writtenContent, action.type);
                        std::string reviewTrimmed = AgentEngine::TrimStr(reviewFeedback);
                        bool approved = ToLowerNoPunct(reviewTrimmed) == "approved" || reviewTrimmed.empty();

                        resultText = "[Confirmed: \"" + action.path + "\" was written to disk and verified "
                            "(file exists, size matches).]\n"
                            "[Reviewer model's feedback on this file:]\n" +
                            (approved
                                ? std::string("APPROVED - no issues found.")
                                : reviewTrimmed) +
                            (approved
                                ? "\nProceed with the next action."
                                : "\nAddress this feedback now with another edit_file action for the SAME path "
                                  "before moving on to anything else.");
                    } else {
                        // No reviewer model configured/running: fall back to
                        // the built-in self-review (show the Creator its own
                        // output and ask it to double-check).
                        resultText = "[Confirmed: \"" + action.path + "\" was written to disk and verified "
                            "(file exists, size matches). Current contents of that file:]\n"
                            "```\n" + writtenContent + "\n```\n"
                            "Review this: if it is correct and complete for this step, proceed with the next action. "
                            "If something is wrong or incomplete, send another edit_file action for the SAME path now "
                            "to fix it before moving on.";
                    }
                } else {
                    resultText = "[Action FAILED - the file was NOT actually written: " + err + "]";
                }
            }
        } else {
            resultText = "[Action rejected by the user. Try a different approach, "
                          "or ask a clarifying question by using \"done\" with a question as the summary.]";
        }

        {
            std::lock_guard<std::mutex> lock(g_agentMutex);
            ChatMessage toolMsg;
            toolMsg.role = "user";
            toolMsg.content = resultText;
            g_agentConversation.messages.push_back(toolMsg);
            g_agentConvMgr.Save(g_agentConversation);
        }
        }
    } catch (std::exception& e) {
        // Last line of defense: an exception escaping a detached
        // std::thread calls std::terminate() and kills the ENTIRE app
        // instantly with no warning — this is very likely what looked like
        // the program "closing itself" during agent runs. Never let that
        // happen; stop the agent gracefully and report what went wrong.
        std::lock_guard<std::mutex> lock(g_agentMutex);
        g_agentStatusMsg = std::string("Agent stopped due to an unexpected error: ") + e.what();
    } catch (...) {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        g_agentStatusMsg = "Agent stopped due to an unexpected internal error.";
    }

    g_agentRunning = false;
    g_agentAwaitingApproval = false;
}

static void FetchFilesWorker(std::string repo, std::string token) {
    g_fetchingFiles = true;
    g_fetchError.clear();
    std::vector<std::string> files;
    std::string err;
    if (ModelDownloader::ListGgufFiles(repo, token, files, err)) {
        g_availableFiles = files;
        g_selectedFileIdx = -1;
    } else {
        g_availableFiles.clear();
        g_fetchError = err;
    }
    g_fetchingFiles = false;
}

static void DownloadWorker(std::string repo, std::string filename, std::string token) {
    std::string dest = "models/" + filename;
    ModelDownloader::DownloadFile(repo, filename, dest, token, &g_downloadState);
    if (g_downloadState.success.load()) RefreshLocalModels();
}


// Polls the server's /health endpoint in the background until it responds
// (meaning the model has finished loading and is actually ready to serve
// requests) or the server is stopped. This is what drives the orange
// "loading" vs green "ready" distinction in the status dot.
// The optional `server` pointer lets the poller also notice when the
// llama-server process itself DIED during startup (corrupted model, port
// already in use, out of VRAM...). Before this, a crashed server left the
// status dot stuck on orange "loading" forever; now it flips back to red
// and the real error is captured in logs\llama-server-<port>.log.
static void PollServerReady(int port, std::atomic<bool>* runningFlag, std::atomic<bool>* readyFlag,
                            LlamaServerManager* server = nullptr) {
    while (runningFlag->load()) {
        if (server && !server->ProcessAlive()) {
            *runningFlag = false;
            *readyFlag = false;
            return;
        }
        if (HttpClient::CheckHealth(port)) {
            *readyFlag = true;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// Red = off, Orange = process started but model still loading, Green = model
// loaded and responding to health checks. Agora desenha um circulo colorido
// de verdade (via ImDrawList, sem depender de glifo Unicode na fonte) antes
// do texto, com um leve "pulso" enquanto carrega. Se um servidor foi
// associado e o processo dele morreu, mostra um botao para abrir o log com
// o erro real (logs\llama-server-<porta>.log).
// Botao de engrenagem que abre/fecha o painel lateral de configuracoes.
// Verde para destacar (e o principal ponto de entrada das configs da aba)
// e com padding maior para o glifo nao ficar minusculo.
static bool GearButton(const char* id) {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.13f, 0.55f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.68f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.45f, 0.23f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    bool clicked = ImGui::Button(id, ImVec2(46, 0));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

static void DrawServerStatusDot(bool running, bool ready, LlamaServerManager* server = nullptr) {
    ImVec4 color;
    const char* label;
    if (!running) {
        color = ImVec4(0.9f, 0.25f, 0.25f, 1.0f);
        label = Localization::T("status_offline");
    } else if (!ready) {
        color = ImVec4(1.0f, 0.6f, 0.15f, 1.0f);
        label = Localization::T("status_loading");
    } else {
        color = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
        label = Localization::T("status_online");
    }

    float h = ImGui::GetTextLineHeight();
    float r = h * 0.30f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center(pos.x + r + 1.0f, pos.y + h * 0.55f);
    float alpha = 1.0f;
    if (running && !ready) {
        // pulso suave enquanto o modelo carrega, para dar feedback "vivo"
        alpha = 0.55f + 0.45f * (0.5f + 0.5f * sinf((float)ImGui::GetTime() * 4.0f));
    }
    ImU32 col = ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, alpha));
    ImGui::GetWindowDrawList()->AddCircleFilled(center, r, col, 16);
    ImGui::Dummy(ImVec2(r * 2.0f + 6.0f, h));
    ImGui::SameLine();

    ImGui::TextColored(color, "%s %s", Localization::T("status_prefix"), label);
    ImGui::SameLine();

    // Servidor caiu (crash na inicializacao ou durante o uso): oferece o
    // log com o motivo real, em vez de deixar o usuario adivinhando.
    if (server && !running && !server->LogFilePath().empty() &&
        std::filesystem::exists(server->LogFilePath())) {
        if (ImGui::SmallButton((std::string(Localization::T("view_server_log")) + "##" +
                                server->LogFilePath()).c_str())) {
            ShellExecuteA(nullptr, "open", server->LogFilePath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::SameLine();
    }
}

// Stops every running llama-server the app has started (Chat, Agent,
// Reviewer, and every Debater), regardless of which tab you're on. Handy
// for freeing RAM/VRAM quickly or switching models without hunting down
// each Stop button individually.
static void StopAllServers() {
    if (g_serverRunning.load()) { g_server.Stop(); g_serverRunning = false; g_serverReady = false; }
    if (g_agentServerRunning.load()) { g_agentServer.Stop(); g_agentServerRunning = false; g_agentServerReady = false; }
    if (g_reviewerServerRunning.load()) { g_reviewerServer.Stop(); g_reviewerServerRunning = false; g_reviewerServerReady = false; }
    for (auto& d : g_debaters) {
        if (d->running.load()) { d->server.Stop(); d->running = false; d->ready = false; }
    }
}

static void SaveConfigFromState() {
    auto& cfg = ConfigManager::Instance();
    cfg.hfToken = g_hfTokenBuffer;
    cfg.language = Localization::GetLanguageIndex();
    cfg.darkMode = g_darkMode.load();
    cfg.thinkingEnabled = g_thinkingEnabled.load();
    cfg.webSearchEnabled = g_webSearchEnabled.load();
    cfg.agentWebSearchEnabled = g_agentWebSearchEnabled.load();
    cfg.lastModelPath = g_modelPathBuffer;
    cfg.serverPort = g_port;
    cfg.sampling = g_sampling;
    cfg.agentModelPath = g_agentModelPathBuffer;
    cfg.agentMmprojPath = g_agentMmprojPathBuffer;
    cfg.agentProjectFolder = g_agentProjectFolder;
    cfg.agentPort = g_agentPort;
    cfg.agentFreeMode = g_agentFreeMode.load();
    cfg.agentThinkingEnabled = g_agentThinkingEnabled.load();
    cfg.reviewerModelPath = g_reviewerModelPathBuffer;
    cfg.reviewerMmprojPath = g_reviewerMmprojPathBuffer;
    cfg.reviewerPort = g_reviewerPort;
    cfg.useReviewerModel = g_useReviewerModel.load();
    cfg.useCustomReviewerPrompt = g_useCustomReviewerPrompt;
    cfg.customReviewerPrompt = g_customReviewerPrompt;
    cfg.debateModeEnabled = g_debateModeEnabled;
    cfg.debateRounds = g_debateRounds;
    cfg.extraServerArgs = g_extraServerArgs;
    cfg.Save();
}

static void StartNewConversation() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_conversation = g_chatConvMgr.NewConversation();
    g_liveThinking.clear();
    g_liveOutput.clear();
}

// ===================== Screens =====================

static void DrawChatTab() {
    // --- Barra superior compacta: engrenagem (abre/fecha o painel lateral
    // de configuracoes), titulo da conversa, Nova conversa e o status ---
    if (GearButton("\xE2\x9A\x99##chatgear")) g_chatSidebarOpen = !g_chatSidebarOpen; // U+2699 GEAR
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Localization::T("settings"));
    ImGui::SameLine();
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        std::string title = g_conversation.title.empty() ? Localization::T("new_conversation") : g_conversation.title;
        ImGui::Text("%s %s", Localization::T("current_conversation_label"), title.c_str());
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(Localization::T("new_conversation"))) {
        StartNewConversation();
    }
    ImGui::SameLine(0, 20);
    {
        int estTokens = 0;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            for (auto& m : g_conversation.messages) estTokens += AgentEngine::EstimateTokens(m.content);
        }
        float ratio = g_sampling.ctxSize > 0 ? (float)estTokens / (float)g_sampling.ctxSize : 0.0f;
        ImVec4 color = ratio > 0.85f ? ImVec4(1, 0.4f, 0.3f, 1) : ratio > 0.6f ? ImVec4(1, 0.8f, 0.3f, 1) : ImVec4(0.6f, 0.8f, 0.6f, 1);
        ImGui::TextColored(color, "%s ~%d / %d %s", Localization::T("tokens_used_label"), estTokens, g_sampling.ctxSize,
                            Localization::T("tokens_used_suffix"));
    }
    ImGui::Separator();

    // --- Painel lateral esquerdo de configuracoes (escondido por padrao) ---
    if (g_chatSidebarOpen) {
        ImGui::BeginChild("ChatSidebar", ImVec2(430, -1), true);
        ImGui::TextDisabled("%s", Localization::T("settings"));
        ImGui::Separator();
        bool thinking = g_thinkingEnabled.load();
        ImGui::BeginDisabled(g_debateModeEnabled);
        if (ImGui::Checkbox(Localization::T("thinking_mode"), &thinking)) {
            g_thinkingEnabled = thinking;
            SaveConfigFromState();
        }
        ImGui::EndDisabled();

        bool debateMode = g_debateModeEnabled;
        if (ImGui::Checkbox(Localization::T("debate_mode_label"), &debateMode)) {
            g_debateModeEnabled = debateMode;
            if (g_debateModeEnabled && g_debaters.size() < 2) {
                while (g_debaters.size() < 2) AddDebater();
            }
            SaveConfigFromState();
        }
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", Localization::T("debate_mode_hint"));
        ImGui::PopTextWrapPos();

        if (g_debateModeEnabled) {
            ImGui::Spacing();
            ImGui::Text("%s", Localization::T("debate_rounds_label"));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::InputInt("##debaterounds", &g_debateRounds, 1, 1)) {
                if (g_debateRounds < 1) g_debateRounds = 1;
                if (g_debateRounds > 10) g_debateRounds = 10;
                SaveConfigFromState();
            }
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", Localization::T("debate_rounds_hint"));
            ImGui::PopTextWrapPos();

            {
                int readyCount = 0;
                for (auto& d : g_debaters) if (d->running.load() && d->ready.load()) readyCount++;
                int estimatedCalls = readyCount * g_debateRounds + 1;
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.8f, 0.3f, 1));
                ImGui::TextWrapped("%s %d %s",
                                    Localization::T("debate_cost_warning_prefix"), estimatedCalls,
                                    Localization::T("debate_cost_warning_suffix"));
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();

            g_pendingRemoveDebaterIdx = -1;
            for (size_t i = 0; i < g_debaters.size(); i++) {
                auto& d = *g_debaters[i];
                ImGui::PushID((int)i);
                ImGui::Separator();
                ImGui::Text("%s %d", Localization::T("debater_label"), (int)i + 1);

                ImGui::SetNextItemWidth(-70);
                ImGui::InputText("##debatermodelpath", d.modelPath, sizeof(d.modelPath));
                ImGui::SameLine();
                if (ImGui::Button(Localization::T("browse"))) {
                    std::string picked = FilePicker::OpenGgufDialog("Select the model .gguf for this debater");
                    if (!picked.empty()) strncpy(d.modelPath, picked.c_str(), sizeof(d.modelPath) - 1);
                }
                DrawServerStatusDot(d.running.load(), d.ready.load(), &d.server);
                if (!d.running.load()) {
                    if (ImGui::Button(Localization::T("start_server"))) {
                        d.ready = false;
                        if (strlen(d.modelPath) > 0 &&
                            d.server.Start(d.modelPath, d.port, g_sampling.ctxSize, 999, d.mmprojPath,
                                            g_extraServerArgs)) {
                            d.running = true;
                            std::thread(PollServerReady, d.port, &d.running, &d.ready, &d.server).detach();
                        }
                    }
                } else {
                    if (ImGui::Button(Localization::T("stop_server"))) {
                        d.server.Stop();
                        d.running = false;
                        d.ready = false;
                    }
                }

                ImGui::SetNextItemWidth(-70);
                ImGui::InputText("##debatermmproj", d.mmprojPath, sizeof(d.mmprojPath));
                ImGui::SameLine();
                if (ImGui::Button((std::string(Localization::T("browse")) + "##mmprojbrowse").c_str())) {
                    std::string picked = FilePicker::OpenGgufDialog("Select the mmproj .gguf file");
                    if (!picked.empty()) strncpy(d.mmprojPath, picked.c_str(), sizeof(d.mmprojPath) - 1);
                }

                if (g_debaters.size() > 2) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton(Localization::T("remove_debater"))) {
                        g_pendingRemoveDebaterIdx = (int)i;
                    }
                }
                ImGui::PopID();
            }

            if (g_pendingRemoveDebaterIdx >= 0 && g_pendingRemoveDebaterIdx < (int)g_debaters.size()) {
                g_debaters[g_pendingRemoveDebaterIdx]->server.Stop();
                g_debaters.erase(g_debaters.begin() + g_pendingRemoveDebaterIdx);
            }

            ImGui::Spacing();
            ImGui::BeginDisabled(g_debaters.size() >= 6);
            if (ImGui::Button(Localization::T("add_model_button"))) {
                AddDebater();
            }
            ImGui::EndDisabled();
            ImGui::Separator();
        }

        ImGui::Separator();

        if (!g_debateModeEnabled) {
            ImGui::TextWrapped("%s", Localization::T("model_label"));
            ImGui::SetNextItemWidth(-75);
            if (ImGui::InputText("##modelpath", g_modelPathBuffer, sizeof(g_modelPathBuffer))) {
                SaveConfigFromState();
            }
            ImGui::SameLine();
            {
                std::string browseId = std::string(Localization::T("browse")) + "##chatmodelbrowse";
                if (ImGui::Button(browseId.c_str())) {
                    std::string picked = FilePicker::OpenGgufDialog("Select the model .gguf for Chat");
                    if (!picked.empty()) {
                        strncpy(g_modelPathBuffer, picked.c_str(), sizeof(g_modelPathBuffer) - 1);
                        SaveConfigFromState();
                    }
                }
            }

            // Status + Iniciar/Parar na linha de baixo: em SameLine eles
            // estouravam a largura do painel e ficavam cortados/invisiveis.
            DrawServerStatusDot(g_serverRunning.load(), g_serverReady.load(), &g_server);
            if (!g_serverRunning.load()) {
                if (ImGui::Button(Localization::T("start_server"))) {
                    g_serverReady = false;
                    if (strlen(g_modelPathBuffer) > 0 &&
                        g_server.Start(g_modelPathBuffer, g_port, g_sampling.ctxSize, 999, g_mmprojPathBuffer,
                                        g_extraServerArgs)) {
                        g_serverRunning = true;
                        std::thread(PollServerReady, g_port, &g_serverRunning, &g_serverReady, &g_server).detach();
                    }
                }
            } else {
                if (ImGui::Button(Localization::T("stop_server"))) {
                    g_server.Stop();
                    g_serverRunning = false;
                    g_serverReady = false;
                }
            }

            ImGui::TextWrapped("%s", Localization::T("mmproj_label"));
            ImGui::SetNextItemWidth(-75);
            ImGui::InputText("##mmprojpath", g_mmprojPathBuffer, sizeof(g_mmprojPathBuffer));
            ImGui::SameLine();
            {
                std::string browseId = std::string(Localization::T("browse")) + "##chatmmprojbrowse";
                if (ImGui::Button(browseId.c_str())) {
                    std::string picked = FilePicker::OpenGgufDialog("Select the mmproj .gguf file");
                    if (!picked.empty()) {
                        strncpy(g_mmprojPathBuffer, picked.c_str(), sizeof(g_mmprojPathBuffer) - 1);
                        SaveConfigFromState();
                    }
                }
            }
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", Localization::T("mmproj_hint"));
            ImGui::PopTextWrapPos();

            ImGui::Separator();
        }

        ImGui::Checkbox(Localization::T("auto_template_label"), &g_autoTemplate);

        const auto& templates = TemplateManager::Instance().All();
        std::vector<const char*> names;
        names.push_back(Localization::T("template_none"));
        for (auto& t : templates) names.push_back(t.displayName.c_str());

        int comboIdx = g_forcedTemplateIdx + 1;
        ImGui::TextWrapped("%s", Localization::T("template_override_label"));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##tploverride_chat", &comboIdx, names.data(), (int)names.size())) {
            g_forcedTemplateIdx = comboIdx - 1;
        }

        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", Localization::T("context_hint"));
        ImGui::PopTextWrapPos();
        ImGui::EndChild(); // ChatSidebar
        ImGui::SameLine();
    }

    // --- Area central (transcript + input + painel de thinking) ---
    ImGui::BeginChild("ChatCenter", ImVec2(-1, -1), false);
    float totalW = ImGui::GetContentRegionAvail().x;
    bool showThinking = g_thinkingEnabled.load();
    float splitterW = showThinking ? 6.0f : 0.0f;
    float usableW = totalW - splitterW;
    if (usableW < 200.0f) usableW = 200.0f;

    float thinkW = showThinking ? g_colRatioThinking * usableW : 0.0f;
    float chatW = usableW - thinkW;
    if (chatW < 150.0f) chatW = 150.0f;

    float rowHeight = -ImGui::GetFrameHeightWithSpacing() * 2; // leaves room for template/status line

    // --- Left: chat area (transcript on top, input box at the bottom) ---
    ImGui::BeginChild("ChatArea", ImVec2(chatW, rowHeight), true);

    float inputAreaH = 130.0f;
    float transcriptH = ImGui::GetContentRegionAvail().y - inputAreaH - 8.0f;
    if (transcriptH < 60.0f) transcriptH = 60.0f;

    ImGui::BeginChild("Transcript", ImVec2(-1, transcriptH), false);
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        float textWidth = ImGui::GetContentRegionAvail().x - 4.0f;
        for (size_t i = 0; i < g_conversation.messages.size(); i++) {
            auto& msg = g_conversation.messages[i];
            std::string roleLabel = msg.role == "user" ? Localization::T("you_label") : Localization::T("assistant_label");
            ImVec4 roleColor = msg.role == "user" ? ImVec4(0.5f, 0.8f, 1.0f, 1.0f) : ImVec4(0.6f, 1.0f, 0.6f, 1.0f);

            ImGui::TextColored(roleColor, "%s", roleLabel.c_str());
            if (!msg.attachedFileName.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("[attached: %s]", msg.attachedFileName.c_str());
            }
            ImGui::SameLine();
            std::string copyId = Localization::T("copy_all") + std::string("##msg") + std::to_string(i);
            if (ImGui::SmallButton(copyId.c_str())) {
                ImGui::SetClipboardText(msg.content.c_str());
            }
            UiHelpers::RenderContentWithCodeBlocks("msg" + std::to_string(i), msg.content, textWidth);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        if (g_isGenerating.load() || !g_liveOutput.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s", Localization::T("assistant_label"));
            UiHelpers::RenderContentWithCodeBlocks("live", g_liveOutput, textWidth);
        }
    }
    if (g_isGenerating.load()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::Separator();

    // --- Input row: attachment chip (if any), text box, Send/Stop ---
    if (!g_pendingAttachFileName.empty() || g_extractingPdf.load()) {
        if (g_extractingPdf.load()) {
            ImGui::TextDisabled("%s", g_attachStatusMsg.empty() ? "Extracting..." : g_attachStatusMsg.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1), "\xF0\x9F\x93\x8E %s", g_pendingAttachFileName.c_str());
            if (g_pendingIsImage) {
                ImGui::SameLine();
                if (ImGui::SmallButton(Localization::T("remove_attachment"))) {
                    ClearPendingAttachment();
                }
            }
        }
    }

    {
        bool webSearch = g_webSearchEnabled.load();
        if (ImGui::Checkbox(Localization::T("web_search_checkbox"), &webSearch)) {
            g_webSearchEnabled = webSearch;
            SaveConfigFromState();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", Localization::T("web_search_tooltip"));
    }
    float promptW = ImGui::GetContentRegionAvail().x - 100;
    ImGui::SetNextItemWidth(promptW);
    ImGui::InputTextMultiline("##prompt", g_promptBuffer, sizeof(g_promptBuffer), ImVec2(promptW, 70));
    ImGui::SameLine();

    ImGui::BeginGroup();
    if (ImGui::Button(Localization::T("attach_button"), ImVec2(80, 32))) {
        HandleAttachClick();
    }
    ImGui::Spacing();

    if (!g_isGenerating.load()) {
        bool debateReady = false;
        if (g_debateModeEnabled) {
            for (auto& d : g_debaters) if (d->running.load() && d->ready.load()) { debateReady = true; break; }
        }
        bool canSend = (g_debateModeEnabled ? debateReady : g_serverRunning.load()) && strlen(g_promptBuffer) > 0;
        ImGui::BeginDisabled(!canSend);
        if (ImGui::Button(Localization::T("send"), ImVec2(80, 32))) {
            std::string prompt(g_promptBuffer);

            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                ChatMessage userMsg;
                userMsg.role = "user";
                userMsg.content = prompt;
                if (g_pendingIsImage) {
                    userMsg.imageBase64 = g_pendingImageBase64;
                    userMsg.imageMime = g_pendingImageMime;
                    userMsg.attachedFileName = g_pendingAttachFileName;
                }
                g_conversation.messages.push_back(userMsg);
            }

            memset(g_promptBuffer, 0, sizeof(g_promptBuffer));
            ClearPendingAttachment();

            if (g_debateModeEnabled) {
                std::thread(DebateWorker, g_customSystemPromptBuffer, prompt).detach();
            } else {
                std::string templateName;
                std::string sysPrompt = ResolveSystemPrompt(prompt, templateName);
                g_activeTemplateName = templateName;
                std::string searchQuery = g_webSearchEnabled.load() ? prompt : std::string();
                std::thread(GenerateWorker, sysPrompt, g_sampling, g_thinkingEnabled.load(), searchQuery).detach();
            }
        }
        ImGui::EndDisabled();
    } else {
        if (ImGui::Button(Localization::T("stop"), ImVec2(80, 32))) {
            g_stopRequested = true;
        }
    }
    ImGui::EndGroup();

    ImGui::EndChild(); // ChatArea

    // --- Right: Thinking panel ---
    if (showThinking) {
        UiHelpers::VerticalSplitter("##splitter_think", rowHeight, &g_colRatioThinking, usableW,
                                     0.12f, 0.75f, /*ratioControlsRightSide=*/true);

        ImGui::BeginChild("ThinkCol", ImVec2(thinkW, rowHeight), true);
        ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.3f, 1), "%s", Localization::T("thinking_label"));
        ImGui::Separator();
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            UiHelpers::SelectableText("livethink", g_liveThinking, thinkW - 16, ImVec4(0, 0, 0, 0));
        }
        if (g_isGenerating.load()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

    ImGui::Separator();

    if (!g_activeTemplateName.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 1), "%s %s",
            Localization::T("template_active_label"), g_activeTemplateName.c_str());
    } else {
        ImGui::TextDisabled("%s %s", Localization::T("template_active_label"), Localization::T("template_none"));
    }

    if (!g_serverRunning.load())
        ImGui::TextDisabled("%s", Localization::T("status_server_off"));
    else
        ImGui::Text("%s", g_isGenerating.load() ? Localization::T("status_generating") : Localization::T("status_idle"));

    ImGui::EndChild(); // ChatCenter
}

static void DrawSettingsTab() {
    ImGui::Text("%s", Localization::T("general_settings_title"));
    ImGui::Spacing();

    bool dark = g_darkMode.load();
    if (ImGui::Checkbox(Localization::T("dark_mode"), &dark)) {
        g_darkMode = dark;
        dark ? ThemeManager::ApplyDark() : ThemeManager::ApplyLight();
        SaveConfigFromState();
    }

    ImGui::SameLine(0, 40);
    static int langIdx = Localization::GetLanguageIndex();
    const char* langs[] = { "English", "Português (BR)", "Español", "中文" };
    ImGui::SetNextItemWidth(200);
    if (ImGui::Combo(Localization::T("language"), &langIdx, langs, 4)) {
        Localization::SetLanguage(langIdx);
        SaveConfigFromState();
    }

    ImGui::Separator();
    ImGui::TextWrapped("%s", Localization::T("param_hint"));
    ImGui::Separator();
    ImGui::Text("%s", Localization::T("sampling_params_title"));
    ImGui::Spacing();

    bool changed = false;
    ImGui::SetNextItemWidth(300);
    changed |= ImGui::SliderFloat(Localization::T("param_temperature"), &g_sampling.temperature, 0.0f, 2.0f, "%.2f");
    ImGui::SetNextItemWidth(300);
    changed |= ImGui::SliderInt(Localization::T("param_top_k"), &g_sampling.topK, 1, 100);
    ImGui::SetNextItemWidth(300);
    changed |= ImGui::SliderFloat(Localization::T("param_top_p"), &g_sampling.topP, 0.0f, 1.0f, "%.2f");
    ImGui::SetNextItemWidth(300);
    changed |= ImGui::SliderFloat(Localization::T("param_min_p"), &g_sampling.minP, 0.0f, 1.0f, "%.2f");
    ImGui::SetNextItemWidth(300);
    changed |= ImGui::SliderFloat(Localization::T("param_repeat_penalty"), &g_sampling.repeatPenalty, 1.0f, 2.0f, "%.2f");
    ImGui::SetNextItemWidth(300);
    changed |= ImGui::SliderInt(Localization::T("param_max_tokens"), &g_sampling.maxTokens, 64, 1048576,
                                 "%d", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("##maxtokens_exact", &g_sampling.maxTokens, 0, 0)) {
        if (g_sampling.maxTokens < 1) g_sampling.maxTokens = 1;
        changed = true;
    }

    ImGui::SetNextItemWidth(300);
    bool ctxChanged = ImGui::SliderInt(Localization::T("param_ctx_size"), &g_sampling.ctxSize, 512, 1048576,
                                        "%d", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("##ctxsize_exact", &g_sampling.ctxSize, 0, 0)) {
        if (g_sampling.ctxSize < 1) g_sampling.ctxSize = 1;
        ctxChanged = true;
    }
    changed |= ctxChanged;

    ImGui::TextDisabled("%s", Localization::T("large_context_hint"));

    if (ctxChanged && g_serverRunning.load()) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "%s", Localization::T("ctx_restart_hint"));
    }

    if (changed) SaveConfigFromState();

    ImGui::Spacing();
    if (ImGui::Button(Localization::T("param_reset"))) {
        g_sampling = SamplingParams{};
        SaveConfigFromState();
    }

    ImGui::Separator();
    ImGui::Text("%s", Localization::T("system_prompt_label"));
    ImGui::TextDisabled("(used only when no template applies)");
    UiHelpers::InputTextMultilineUnlimited("##sysprompt", &g_customSystemPromptBuffer, ImVec2(600, 100));

    ImGui::Separator();
    ImGui::Text("%s", Localization::T("extra_args_title"));
    ImGui::TextWrapped("%s", Localization::T("extra_args_hint"));

    if (ImGui::TreeNode(Localization::T("extra_args_common_flags_title"))) {
        static const char* commonFlags[][2] = {
            {"-ngl N", "Number of model layers offloaded to GPU. 0 = CPU only, 999 = as many as possible."},
            {"-t N", "Number of CPU threads to use for generation."},
            {"-tb N", "Number of CPU threads to use for batch/prompt processing."},
            {"-b N", "Batch size for prompt processing (default 2048; lower uses less memory)."},
            {"-ub N", "Physical batch size (micro-batch) used internally."},
            {"--flash-attn", "Enables Flash Attention, faster and lower memory on supported GPUs."},
            {"--no-mmap", "Disables memory-mapped model loading (loads fully into RAM instead)."},
            {"--mlock", "Locks the model in RAM, preventing it from being swapped to disk."},
            {"--split-mode none|layer|row", "How to split the model across multiple GPUs, if you have more than one."},
            {"--main-gpu N", "Which GPU index to use as the primary one (multi-GPU systems)."},
            {"--parallel N", "Number of concurrent request slots the server can handle."},
            {"--cache-type-k / --cache-type-v", "KV cache quantization (e.g. q8_0, q4_0) to save memory."},
        };
        for (auto& row : commonFlags) {
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1), "%s", row[0]);
            ImGui::SameLine(180);
            ImGui::TextWrapped("%s", row[1]);
        }
        ImGui::TextDisabled("%s", Localization::T("extra_args_full_list_hint"));
        ImGui::TreePop();
    }

    if (UiHelpers::InputTextMultilineUnlimited("##extraserverargs", &g_extraServerArgs, ImVec2(600, 60))) {
        SaveConfigFromState();
    }
    ImGui::TextDisabled("%s", Localization::T("extra_args_example"));
    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "%s", Localization::T("extra_args_restart_hint"));
}

static void DrawTemplatesTab() {
    ImGui::TextWrapped("%s", Localization::T("templates_hint"));
    ImGui::Separator();

    if (ImGui::CollapsingHeader(Localization::T("add_template_title"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("%s", Localization::T("template_name_label"));
        ImGui::SetNextItemWidth(400);
        UiHelpers::InputTextUnlimited("##newtplname", &g_newTemplateName);

        ImGui::Text("%s", Localization::T("template_keywords_label"));
        ImGui::SetNextItemWidth(500);
        UiHelpers::InputTextUnlimited("##newtplkeywords", &g_newTemplateKeywords);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", Localization::T("template_keywords_hint"));
        ImGui::PopTextWrapPos();

        ImGui::Text("%s", Localization::T("template_prompt_label"));
        UiHelpers::InputTextMultilineUnlimited("##newtplprompt", &g_newTemplateSystemPrompt, ImVec2(600, 200));

        if (ImGui::Button(Localization::T("add_template_button"), ImVec2(180, 0))) {
            std::string err;
            bool ok = TemplateManager::Instance().AddCustom(
                g_newTemplateName, g_newTemplateKeywords, g_newTemplateSystemPrompt, err
            );
            if (ok) {
                g_templateStatusMsg = Localization::T("template_added_msg");
                g_newTemplateName.clear();
                g_newTemplateKeywords.clear();
                g_newTemplateSystemPrompt.clear();
            } else {
                g_templateStatusMsg = err;
            }
        }
        if (!g_templateStatusMsg.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1), "%s", g_templateStatusMsg.c_str());
        }
    }

    auto& mgr = TemplateManager::Instance();
    if (mgr.HasHiddenBuiltIns()) {
        if (ImGui::Button(Localization::T("restore_builtin_templates"))) {
            mgr.RestoreBuiltIns();
        }
    }

    ImGui::Separator();
    ImGui::Text("%s", Localization::T("templates_list_title"));
    ImGui::Spacing();

    ImGui::BeginChild("TemplatesList", ImVec2(0, 0), true);
    for (auto& t : mgr.All()) {
        std::string badge = t.isBuiltIn ? Localization::T("builtin_badge") : Localization::T("custom_badge");
        std::string nodeLabel = t.displayName + "  [" + badge + "]";

        if (ImGui::TreeNode((nodeLabel + "##" + t.id).c_str())) {
            ImGui::TextWrapped("%s", t.systemPrompt.c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("Keywords:");
            std::string kw;
            for (size_t i = 0; i < t.keywords.size(); i++) {
                kw += t.keywords[i];
                if (i + 1 < t.keywords.size()) kw += ", ";
            }
            ImGui::TextWrapped("%s", kw.c_str());

            ImGui::Spacing();

            if (g_editingTemplateId == t.id) {
                // --- Formulario de edicao inline ---
                ImGui::Text("%s", Localization::T("template_name_label"));
                ImGui::SetNextItemWidth(400);
                UiHelpers::InputTextUnlimited(("##editname_" + t.id).c_str(), &g_editTemplateName);
                ImGui::Text("%s", Localization::T("template_keywords_label"));
                ImGui::SetNextItemWidth(500);
                UiHelpers::InputTextUnlimited(("##editkw_" + t.id).c_str(), &g_editTemplateKeywords);
                ImGui::Text("%s", Localization::T("template_prompt_label"));
                UiHelpers::InputTextMultilineUnlimited(("##editprompt_" + t.id).c_str(), &g_editTemplatePrompt, ImVec2(600, 180));

                if (ImGui::Button((std::string(Localization::T("save_changes_button")) + "##save_" + t.id).c_str(), ImVec2(140, 0))) {
                    std::string err;
                    if (TemplateManager::Instance().Update(t.id, g_editTemplateName, g_editTemplateKeywords,
                                                            g_editTemplatePrompt, err)) {
                        g_templateStatusMsg = Localization::T("template_updated_msg");
                        g_editingTemplateId.clear();
                    } else {
                        g_templateStatusMsg = err;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button((std::string(Localization::T("cancel_button")) + "##canceledit_" + t.id).c_str(), ImVec2(110, 0))) {
                    g_editingTemplateId.clear();
                }
                if (t.isBuiltIn) {
                    ImGui::TextDisabled("%s", Localization::T("edit_builtin_note"));
                }
            } else if (g_pendingDeleteTemplateId == t.id) {
                ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", Localization::T("delete_template_confirm"));
                ImGui::SameLine();
                if (ImGui::SmallButton(("Yes##" + t.id).c_str())) {
                    TemplateManager::Instance().Remove(t.id);
                    g_pendingDeleteTemplateId.clear();
                    if (g_forcedTemplateIdx >= (int)mgr.All().size()) g_forcedTemplateIdx = -1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(("Cancel##" + t.id).c_str())) {
                    g_pendingDeleteTemplateId.clear();
                }
            } else {
                std::string editBtnId = std::string(Localization::T("edit_template_button")) + "##edit_" + t.id;
                if (ImGui::SmallButton(editBtnId.c_str())) {
                    g_editingTemplateId = t.id;
                    g_editTemplateName = t.displayName;
                    g_editTemplatePrompt = t.systemPrompt;
                    g_editTemplateKeywords.clear();
                    for (size_t i = 0; i < t.keywords.size(); i++) {
                        g_editTemplateKeywords += t.keywords[i];
                        if (i + 1 < t.keywords.size()) g_editTemplateKeywords += ", ";
                    }
                }
                ImGui::SameLine();
                std::string delBtnId = std::string(Localization::T("delete")) + "##del_" + t.id;
                if (ImGui::SmallButton(delBtnId.c_str())) {
                    g_pendingDeleteTemplateId = t.id;
                }
            }

            ImGui::TreePop();
        }
    }
    ImGui::EndChild();
}

static void DrawAgentTab() {
    if (g_agentPdfAppendReady.load()) {
        g_agentTaskBuffer += g_agentPdfPendingAppend;
        g_agentPdfPendingAppend.clear();
        g_agentPdfAppendReady = false;
    }

    // --- Barra superior compacta: engrenagem + status do agente + tokens ---
    if (GearButton("\xE2\x9A\x99##agentgear")) g_agentSidebarOpen = !g_agentSidebarOpen; // U+2699 GEAR
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Localization::T("settings"));
    ImGui::SameLine();
    if (!g_agentRunning.load()) {
        if (ImGui::Button(Localization::T("new_project"))) {
            std::lock_guard<std::mutex> lock(g_agentMutex);
            g_agentConversation = g_agentConvMgr.NewConversation();
            g_agentConversation.id = "agent_" + g_agentConversation.id;
            g_agentCurrentTaskText.clear();
            g_agentStatusMsg.clear();
        }
        ImGui::SameLine();
    }
    ImGui::Text("%s", g_agentRunning.load() ? Localization::T("agent_working_status") : Localization::T("agent_idle_status"));
    ImGui::SameLine(0, 20);
    {
        int estTokens = 0;
        {
            std::lock_guard<std::mutex> lock(g_agentMutex);
            for (auto& m : g_agentConversation.messages) estTokens += AgentEngine::EstimateTokens(m.content);
        }
        float ratio = g_sampling.ctxSize > 0 ? (float)estTokens / (float)g_sampling.ctxSize : 0.0f;
        ImVec4 color = ratio > 0.85f ? ImVec4(1, 0.4f, 0.3f, 1) : ratio > 0.6f ? ImVec4(1, 0.8f, 0.3f, 1) : ImVec4(0.6f, 0.8f, 0.6f, 1);
        ImGui::TextColored(color, "%s ~%d / %d %s", Localization::T("tokens_used_label"), estTokens, g_sampling.ctxSize,
                            Localization::T("tokens_used_suffix"));
    }
    ImGui::Separator();

    // --- Painel lateral esquerdo (modelo, pasta, reviewer...) ---
    if (g_agentSidebarOpen) {
        ImGui::BeginChild("AgentSidebar", ImVec2(430, -1), true);
        ImGui::TextDisabled("%s", Localization::T("settings"));
        ImGui::Separator();
        ImGui::TextWrapped("%s", Localization::T("model_label"));
        ImGui::SetNextItemWidth(-75);
        ImGui::InputText("##agentmodelpath", g_agentModelPathBuffer, sizeof(g_agentModelPathBuffer));
        ImGui::SameLine();
        std::string agentModelBrowseId = std::string(Localization::T("browse")) + "##agentmodelbrowse";
        if (ImGui::Button(agentModelBrowseId.c_str())) {
            std::string picked = FilePicker::OpenGgufDialog("Select the model .gguf for the Agent");
            if (!picked.empty()) {
                strncpy(g_agentModelPathBuffer, picked.c_str(), sizeof(g_agentModelPathBuffer) - 1);
                SaveConfigFromState();
            }
        }

        DrawServerStatusDot(g_agentServerRunning.load(), g_agentServerReady.load(), &g_agentServer);
        if (!g_agentServerRunning.load()) {
            if (ImGui::Button(Localization::T("start_server"))) {
                g_agentServerReady = false;
                if (strlen(g_agentModelPathBuffer) > 0 &&
                    g_agentServer.Start(g_agentModelPathBuffer, g_agentPort, g_sampling.ctxSize, 999, g_agentMmprojPathBuffer,
                                         g_extraServerArgs)) {
                    g_agentServerRunning = true;
                    std::thread(PollServerReady, g_agentPort, &g_agentServerRunning, &g_agentServerReady, &g_agentServer).detach();
                }
            }
        } else {
            if (ImGui::Button(Localization::T("stop_server"))) {
                g_agentServer.Stop();
                g_agentServerRunning = false;
                g_agentServerReady = false;
            }
        }

        ImGui::TextWrapped("%s", Localization::T("mmproj_label"));
        ImGui::SetNextItemWidth(-75);
        ImGui::InputText("##agentmmprojpath", g_agentMmprojPathBuffer, sizeof(g_agentMmprojPathBuffer));
        ImGui::SameLine();
        std::string agentMmprojBrowseId = std::string(Localization::T("browse")) + "##agentmmprojbrowse";
        if (ImGui::Button(agentMmprojBrowseId.c_str())) {
            std::string picked = FilePicker::OpenGgufDialog("Select the mmproj .gguf file");
            if (!picked.empty()) {
                strncpy(g_agentMmprojPathBuffer, picked.c_str(), sizeof(g_agentMmprojPathBuffer) - 1);
                SaveConfigFromState();
            }
        }
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", Localization::T("mmproj_hint"));
        ImGui::PopTextWrapPos();

        ImGui::Separator();

        ImGui::Text("%s", Localization::T("agent_project_folder_label"));
        if (g_agentProjectFolder.empty()) {
            ImGui::TextDisabled("%s", Localization::T("no_folder_selected"));
        } else {
            ImGui::TextWrapped("%s", g_agentProjectFolder.c_str());
        }
        if (ImGui::Button(Localization::T("choose_folder"))) {
            std::string folder = FolderPicker::OpenFolderDialog();
            if (!folder.empty()) {
                g_agentProjectFolder = folder;
                SaveConfigFromState();
            }
        }

        ImGui::Separator();

        bool freeMode = g_agentFreeMode.load();
        if (ImGui::Checkbox(Localization::T("agent_free_mode_label"), &freeMode)) {
            g_agentFreeMode = freeMode;
            SaveConfigFromState();
        }
        ImGui::TextWrapped("%s", Localization::T("agent_free_mode_hint"));

        bool thinking = g_agentThinkingEnabled.load();
        if (ImGui::Checkbox(Localization::T("thinking_mode"), &thinking)) {
            g_agentThinkingEnabled = thinking;
            SaveConfigFromState();
        }

        bool agentWebSearch = g_agentWebSearchEnabled.load();
        if (ImGui::Checkbox((std::string(Localization::T("web_search_checkbox")) + "##agent").c_str(), &agentWebSearch)) {
            g_agentWebSearchEnabled = agentWebSearch;
            SaveConfigFromState();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", Localization::T("agent_web_search_tooltip"));

        ImGui::Separator();

        ImGui::Checkbox(Localization::T("auto_template_label"), &g_agentAutoTemplate);

        const auto& templates = TemplateManager::Instance().All();
        std::vector<const char*> tplNames;
        tplNames.push_back(Localization::T("template_none"));
        for (auto& t : templates) tplNames.push_back(t.displayName.c_str());

        int tplComboIdx = g_agentForcedTemplateIdx + 1;
        ImGui::TextWrapped("%s", Localization::T("template_override_label"));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##tploverride_agent", &tplComboIdx, tplNames.data(), (int)tplNames.size())) {
            g_agentForcedTemplateIdx = tplComboIdx - 1;
        }

    if (ImGui::CollapsingHeader(Localization::T("reviewer_section_title"))) {
        ImGui::TextWrapped("%s", Localization::T("reviewer_section_hint"));
        ImGui::Separator();

        bool useReviewer = g_useReviewerModel.load();
        if (ImGui::Checkbox(Localization::T("use_reviewer_label"), &useReviewer)) {
            g_useReviewerModel = useReviewer;
            SaveConfigFromState();
        }

        ImGui::TextWrapped("%s", Localization::T("model_label"));
        ImGui::SetNextItemWidth(-75);
        ImGui::InputText("##reviewermodelpath", g_reviewerModelPathBuffer, sizeof(g_reviewerModelPathBuffer));
        ImGui::SameLine();
        {
            std::string browseId = std::string(Localization::T("browse")) + "##reviewermodelbrowse";
            if (ImGui::Button(browseId.c_str())) {
                std::string picked = FilePicker::OpenGgufDialog("Select the model .gguf for the Reviewer");
                if (!picked.empty()) {
                    strncpy(g_reviewerModelPathBuffer, picked.c_str(), sizeof(g_reviewerModelPathBuffer) - 1);
                    SaveConfigFromState();
                }
            }
        }

        DrawServerStatusDot(g_reviewerServerRunning.load(), g_reviewerServerReady.load(), &g_reviewerServer);
        if (!g_reviewerServerRunning.load()) {
            std::string startId = std::string(Localization::T("start_server")) + "##reviewerstart";
            if (ImGui::Button(startId.c_str())) {
                g_reviewerServerReady = false;
                if (strlen(g_reviewerModelPathBuffer) > 0 &&
                    g_reviewerServer.Start(g_reviewerModelPathBuffer, g_reviewerPort, g_sampling.ctxSize, 999, g_reviewerMmprojPathBuffer,
                                            g_extraServerArgs)) {
                    g_reviewerServerRunning = true;
                    std::thread(PollServerReady, g_reviewerPort, &g_reviewerServerRunning, &g_reviewerServerReady, &g_reviewerServer).detach();
                }
            }
        } else {
            if (ImGui::Button((std::string(Localization::T("stop_server")) + "##reviewerstop").c_str())) {
                g_reviewerServer.Stop();
                g_reviewerServerRunning = false;
                g_reviewerServerReady = false;
            }
        }

        ImGui::TextWrapped("%s", Localization::T("mmproj_label"));
        ImGui::SetNextItemWidth(-75);
        ImGui::InputText("##reviewermmprojpath", g_reviewerMmprojPathBuffer, sizeof(g_reviewerMmprojPathBuffer));
        ImGui::SameLine();
        {
            std::string browseId = std::string(Localization::T("browse")) + "##reviewermmprojbrowse";
            if (ImGui::Button(browseId.c_str())) {
                std::string picked = FilePicker::OpenGgufDialog("Select the mmproj .gguf file");
                if (!picked.empty()) {
                    strncpy(g_reviewerMmprojPathBuffer, picked.c_str(), sizeof(g_reviewerMmprojPathBuffer) - 1);
                    SaveConfigFromState();
                }
            }
        }

        ImGui::Separator();

        if (ImGui::Checkbox(Localization::T("use_custom_reviewer_prompt_label"), &g_useCustomReviewerPrompt)) {
            SaveConfigFromState();
        }
        if (g_useCustomReviewerPrompt) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", Localization::T("custom_reviewer_prompt_hint"));
            ImGui::PopTextWrapPos();
            if (UiHelpers::InputTextMultilineUnlimited("##reviewerprompt", &g_customReviewerPrompt, ImVec2(-1, 120))) {
                SaveConfigFromState();
            }
        } else {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", Localization::T("default_reviewer_prompt_note"));
            ImGui::PopTextWrapPos();
        }
    }

        ImGui::EndChild(); // AgentSidebar
        ImGui::SameLine();
    }

    // --- Area central: log de atividade EM CIMA, caixa de aprovacao e
    // campo de tarefa EMBAIXO (mesmo layout do Chat) ---
    ImGui::BeginChild("AgentCenter", ImVec2(-1, -1), false);

    // Altura reservada para o bloco de baixo (aprovacao + input + botoes)
    bool awaitingApproval = g_agentAwaitingApproval.load();
    bool approvalIsEdit = false;
    AgentAction pendingActionCopy;
    if (awaitingApproval) {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        pendingActionCopy = g_agentPendingAction;
        approvalIsEdit = (pendingActionCopy.type == AgentActionType::EditFile);
    }
    float bottomH = 185.0f + (awaitingApproval ? (approvalIsEdit ? 430.0f : 230.0f) : 0.0f);

    float centerTotalW = ImGui::GetContentRegionAvail().x;
    bool agentShowThinking = g_agentThinkingEnabled.load();
    float agentThinkW = agentShowThinking ? centerTotalW * 0.30f : 0.0f;
    float agentLogW = centerTotalW - agentThinkW - (agentShowThinking ? 6.0f : 0.0f);
    float logH = ImGui::GetContentRegionAvail().y - bottomH;
    if (logH < 80.0f) logH = 80.0f;

    ImGui::BeginChild("AgentLog", ImVec2(agentLogW, logH), true);
    ImGui::Text("%s", Localization::T("agent_log_label"));
    ImGui::Separator();
    {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        float w = ImGui::GetContentRegionAvail().x - 4.0f;
        for (size_t i = 0; i < g_agentConversation.messages.size(); i++) {
            auto& msg = g_agentConversation.messages[i];
            std::string roleLabel = msg.role == "user" ? Localization::T("you_label") : Localization::T("assistant_label");
            ImVec4 roleColor = msg.role == "user" ? ImVec4(0.5f, 0.8f, 1.0f, 1.0f) : ImVec4(0.6f, 1.0f, 0.6f, 1.0f);
            ImGui::TextColored(roleColor, "%s", roleLabel.c_str());
            if (!msg.attachedFileName.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("[attached: %s]", msg.attachedFileName.c_str());
            }
            UiHelpers::RenderContentWithCodeBlocks("agentmsg" + std::to_string(i), msg.content, w);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
        if (g_agentRunning.load() && !g_agentLiveOutput.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s", Localization::T("assistant_label"));
            UiHelpers::RenderContentWithCodeBlocks("agentlive", g_agentLiveOutput, w);
        }
    }
    if (g_agentRunning.load()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild(); // AgentLog

    if (agentShowThinking) {
        ImGui::SameLine();
        ImGui::BeginChild("AgentThink", ImVec2(agentThinkW, logH), true);
        ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.3f, 1), "%s", Localization::T("thinking_label"));
        ImGui::Separator();
        {
            std::lock_guard<std::mutex> lock(g_agentMutex);
            UiHelpers::SelectableText("agentlivethink", g_agentLiveThinking, agentThinkW - 16, ImVec4(0, 0, 0, 0));
        }
        if (g_agentRunning.load()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

    ImGui::Separator();

    // --- Pending approval box (shown whenever the agent is waiting) ---
    if (awaitingApproval) {
        AgentAction& action = pendingActionCopy;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.25f, 0.18f, 0.05f, 1.0f));
        bool isEdit = approvalIsEdit;
        ImGui::BeginChild("ApprovalBox", ImVec2(0, isEdit ? 420 : 220), true);
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "%s", Localization::T("approval_needed_title"));
        ImGui::Separator();

        const char* actionLabel;
        if (action.type == AgentActionType::CreateFile) actionLabel = Localization::T("agent_action_create");
        else if (action.type == AgentActionType::DeleteFile) actionLabel = Localization::T("agent_action_delete");
        else actionLabel = Localization::T("agent_action_edit");

        ImGui::Text("%s %s", actionLabel, action.path.c_str());
        if (!action.summary.empty()) ImGui::TextWrapped("(%s)", action.summary.c_str());

        float w = ImGui::GetContentRegionAvail().x;
        if (action.type == AgentActionType::DeleteFile) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", Localization::T("agent_delete_warning"));
        } else if (isEdit) {
            std::filesystem::path absPath;
            std::string resolveErr;
            std::string oldContent;
            if (AgentEngine::ResolveSafePath(g_agentProjectFolder, action.path, absPath, resolveErr) &&
                std::filesystem::exists(absPath)) {
                std::ifstream f(absPath, std::ios::binary);
                std::ostringstream ss;
                ss << f.rdbuf();
                oldContent = ss.str();
            }
            if (oldContent.empty()) {
                // No previous version found on disk (shouldn't normally
                // happen for edit_file, but don't block the approval over it)
                ImGui::TextDisabled("%s", Localization::T("diff_no_previous_version"));
                UiHelpers::RenderContentWithCodeBlocks("pendingaction", "```\n" + action.content + "\n```", w);
            } else {
                UiHelpers::RenderLineDiff("pendingdiff", oldContent, action.content, w, 260);
            }
        } else {
            UiHelpers::RenderContentWithCodeBlocks("pendingaction", "```\n" + action.content + "\n```", w);
        }

        if (ImGui::Button(Localization::T("approve"), ImVec2(120, 0))) {
            g_agentApprovalDecision = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button(Localization::T("reject"), ImVec2(120, 0))) {
            g_agentApprovalDecision = 2;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    // --- Task input ---
    ImGui::Text("%s", Localization::T("agent_task_label"));
    UiHelpers::InputTextMultilineUnlimited("##agenttask", &g_agentTaskBuffer, ImVec2(-1, 90));

    if (!g_agentPendingAttachFileName.empty() || g_agentExtractingPdf.load()) {
        if (g_agentExtractingPdf.load()) {
            ImGui::TextDisabled("%s", g_agentAttachStatusMsg.empty() ? "Extracting..." : g_agentAttachStatusMsg.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1), "\xF0\x9F\x93\x8E %s", g_agentPendingAttachFileName.c_str());
            if (g_agentPendingIsImage) {
                ImGui::SameLine();
                if (ImGui::SmallButton((std::string(Localization::T("remove_attachment")) + "##agentremoveattach").c_str())) {
                    ClearAgentPendingAttachment();
                }
            }
        }
    } else if (!g_agentAttachStatusMsg.empty()) {
        ImGui::TextDisabled("%s", g_agentAttachStatusMsg.c_str());
    }

    if (ImGui::Button(Localization::T("attach_button"), ImVec2(100, 0))) {
        HandleAgentAttachClick();
    }
    ImGui::SameLine();

    bool canStart = g_agentServerRunning.load() && !g_agentProjectFolder.empty() &&
                    !g_agentRunning.load() && !g_agentTaskBuffer.empty();

    if (!g_agentRunning.load()) {
        ImGui::BeginDisabled(!canStart);
        if (ImGui::Button(Localization::T("start_task"), ImVec2(150, 0))) {
            std::string task = g_agentTaskBuffer;
            g_agentTaskBuffer.clear();
            g_agentCurrentTaskText = task;
            {
                std::lock_guard<std::mutex> lock(g_agentMutex);
                // Continue the current project's conversation instead of
                // wiping it — this is what lets the agent remember what it
                // already built when you ask a follow-up request. Use the
                // "New Project" button to start fresh.
                g_agentStatusMsg.clear();
            }
            std::thread(AgentWorker, task).detach();
        }
        ImGui::EndDisabled();
        if (g_agentProjectFolder.empty()) {
            ImGui::SameLine();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", Localization::T("agent_folder_required_hint"));
            ImGui::PopTextWrapPos();
        }
    } else {
        if (ImGui::Button(Localization::T("stop_agent"), ImVec2(150, 0))) {
            g_agentStopRequested = true;
            g_agentApprovalDecision = 2; // unblock if it was waiting on approval
        }
    }

    ImGui::SameLine();
    {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        if (!g_agentStatusMsg.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1), "%s", g_agentStatusMsg.c_str());
        }
    }

    ImGui::EndChild(); // AgentCenter
}

static void DrawHistoryTab() {
    if (ImGui::Button(Localization::T("new_conversation"))) {
        StartNewConversation();
    }
    ImGui::SameLine();

    ImGui::BeginDisabled(g_conversationList.empty());
    if (ImGui::Button(Localization::T("delete_all_button"))) {
        g_historyShowDeleteAllConfirm = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(g_historySelectedIds.empty());
    if (ImGui::Button(Localization::T("delete_selected_button"))) {
        for (auto& id : g_historySelectedIds) g_chatConvMgr.Delete(id);
        g_historySelectedIds.clear();
        RefreshConversationList();
    }
    ImGui::EndDisabled();

    if (g_historyShowDeleteAllConfirm) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", Localization::T("delete_all_confirm"));
        ImGui::SameLine();
        if (ImGui::SmallButton("Yes##histall")) {
            for (auto& s : g_conversationList) g_chatConvMgr.Delete(s.id);
            g_historySelectedIds.clear();
            g_historyShowDeleteAllConfirm = false;
            RefreshConversationList();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel##histall")) g_historyShowDeleteAllConfirm = false;
    }

    ImGui::SetNextItemWidth(300);
    UiHelpers::InputTextUnlimited("##historysearch", &g_historySearchBuffer);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", Localization::T("search_placeholder"));

    ImGui::Separator();

    if (g_conversationList.empty()) {
        ImGui::TextWrapped("%s", Localization::T("no_conversations_yet"));
        return;
    }

    std::string searchLower = AgentEngine::ToLowerStr(g_historySearchBuffer);

    ImGui::BeginChild("ConvList", ImVec2(0, 0), true);
    for (auto& summary : g_conversationList) {
        if (!searchLower.empty() &&
            AgentEngine::ToLowerStr(summary.title).find(searchLower) == std::string::npos) {
            continue;
        }

        ImGui::PushID(summary.id.c_str());

        bool selected = g_historySelectedIds.count(summary.id) > 0;
        if (ImGui::Checkbox("##sel", &selected)) {
            if (selected) g_historySelectedIds.insert(summary.id);
            else g_historySelectedIds.erase(summary.id);
        }
        ImGui::SameLine();

        ImGui::BulletText("%s", summary.title.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(Localization::T("load"))) {
            Conversation loaded;
            if (g_chatConvMgr.Load(summary.id, loaded)) {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                g_conversation = loaded;
                g_liveThinking.clear();
                g_liveOutput.clear();
            }
        }
        ImGui::SameLine();

        ImGui::SameLine();
        if (ImGui::SmallButton(Localization::T("export_button"))) {
            Conversation loaded;
            if (g_chatConvMgr.Load(summary.id, loaded)) {
                std::filesystem::create_directories("exports");
                std::string safeTitle = loaded.title.empty() ? "conversation" : loaded.title;
                for (auto& c : safeTitle) if (strchr("\\/:*?\"<>|", c)) c = '_';
                std::string outPath = "exports/" + safeTitle + "_" + summary.id + ".md";
                ExportConversationToMarkdown(loaded, outPath);
            }
        }

        if (g_pendingDeleteConvId == summary.id) {
            ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", Localization::T("delete_conversation_confirm"));
            ImGui::SameLine();
            if (ImGui::SmallButton("Yes")) {
                g_chatConvMgr.Delete(summary.id);
                g_pendingDeleteConvId.clear();
                g_historySelectedIds.erase(summary.id);
                RefreshConversationList();
                ImGui::PopID();
                continue;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel")) {
                g_pendingDeleteConvId.clear();
            }
        } else {
            if (ImGui::SmallButton(Localization::T("delete"))) {
                g_pendingDeleteConvId = summary.id;
            }
        }

        ImGui::PopID();
    }
    ImGui::EndChild();
}

static void DrawProjectsTab() {
    RefreshAgentConversationList();

    if (ImGui::Button(Localization::T("new_project"))) {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        g_agentConversation = g_agentConvMgr.NewConversation();
        g_agentConversation.id = "agent_" + g_agentConversation.id;
        g_agentCurrentTaskText.clear();
    }
    ImGui::SameLine();

    ImGui::BeginDisabled(g_agentConversationList.empty());
    if (ImGui::Button(Localization::T("delete_all_button"))) {
        g_projectsShowDeleteAllConfirm = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(g_projectsSelectedIds.empty());
    if (ImGui::Button(Localization::T("delete_selected_button"))) {
        for (auto& id : g_projectsSelectedIds) g_agentConvMgr.Delete(id);
        g_projectsSelectedIds.clear();
        RefreshAgentConversationList();
    }
    ImGui::EndDisabled();

    if (g_projectsShowDeleteAllConfirm) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", Localization::T("delete_all_confirm"));
        ImGui::SameLine();
        if (ImGui::SmallButton("Yes##projall")) {
            for (auto& s : g_agentConversationList) g_agentConvMgr.Delete(s.id);
            g_projectsSelectedIds.clear();
            g_projectsShowDeleteAllConfirm = false;
            RefreshAgentConversationList();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel##projall")) g_projectsShowDeleteAllConfirm = false;
    }

    ImGui::SetNextItemWidth(300);
    UiHelpers::InputTextUnlimited("##projectssearch", &g_projectsSearchBuffer);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", Localization::T("search_placeholder"));

    ImGui::Separator();

    if (g_agentConversationList.empty()) {
        ImGui::TextWrapped("%s", Localization::T("no_projects_yet"));
        return;
    }

    std::string searchLower = AgentEngine::ToLowerStr(g_projectsSearchBuffer);

    ImGui::BeginChild("ProjList", ImVec2(0, 0), true);
    for (auto& summary : g_agentConversationList) {
        if (!searchLower.empty() &&
            AgentEngine::ToLowerStr(summary.title).find(searchLower) == std::string::npos) {
            continue;
        }

        ImGui::PushID(summary.id.c_str());

        bool selected = g_projectsSelectedIds.count(summary.id) > 0;
        if (ImGui::Checkbox("##sel", &selected)) {
            if (selected) g_projectsSelectedIds.insert(summary.id);
            else g_projectsSelectedIds.erase(summary.id);
        }
        ImGui::SameLine();

        ImGui::BulletText("%s", summary.title.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(Localization::T("load"))) {
            Conversation loaded;
            if (g_agentConvMgr.Load(summary.id, loaded)) {
                std::lock_guard<std::mutex> lock(g_agentMutex);
                g_agentConversation = loaded;
                g_agentLiveThinking.clear();
                g_agentLiveOutput.clear();
            }
        }
        ImGui::SameLine();

        ImGui::SameLine();
        if (ImGui::SmallButton(Localization::T("export_button"))) {
            Conversation loaded;
            if (g_agentConvMgr.Load(summary.id, loaded)) {
                std::filesystem::create_directories("exports");
                std::string safeTitle = loaded.title.empty() ? "project" : loaded.title;
                for (auto& c : safeTitle) if (strchr("\\/:*?\"<>|", c)) c = '_';
                std::string outPath = "exports/" + safeTitle + "_" + summary.id + ".md";
                ExportConversationToMarkdown(loaded, outPath);
            }
        }

        if (g_pendingDeleteAgentConvId == summary.id) {
            ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", Localization::T("delete_project_confirm"));
            ImGui::SameLine();
            if (ImGui::SmallButton("Yes")) {
                g_agentConvMgr.Delete(summary.id);
                g_pendingDeleteAgentConvId.clear();
                g_projectsSelectedIds.erase(summary.id);
                RefreshAgentConversationList();
                ImGui::PopID();
                continue;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel")) {
                g_pendingDeleteAgentConvId.clear();
            }
        } else {
            if (ImGui::SmallButton(Localization::T("delete"))) {
                g_pendingDeleteAgentConvId = summary.id;
            }
        }

        ImGui::PopID();
    }
    ImGui::EndChild();
}

static void DrawModelsTab() {
    ImGui::TextWrapped("%s", Localization::T("gguf_only_note"));
    ImGui::Separator();

    ImGui::TextWrapped("%s", Localization::T("hf_token_label"));
    ImGui::SetNextItemWidth(400);
    ImGuiInputTextFlags tokenFlags = g_showToken ? 0 : ImGuiInputTextFlags_Password;
    ImGui::InputText("##hftoken", g_hfTokenBuffer, sizeof(g_hfTokenBuffer), tokenFlags);
    ImGui::SameLine();
    ImGui::Checkbox("show", &g_showToken);
    ImGui::SameLine();
    if (ImGui::Button(Localization::T("save_token"))) {
        SaveConfigFromState();
        std::string username;
        if (ModelDownloader::ValidateToken(g_hfTokenBuffer, username)) {
            g_tokenStatusMsg = std::string(Localization::T("token_saved_msg")) + " (" + username + ")";
        } else {
            g_tokenStatusMsg = Localization::T("token_saved_msg");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(Localization::T("clear_token"))) {
        memset(g_hfTokenBuffer, 0, sizeof(g_hfTokenBuffer));
        SaveConfigFromState();
        g_tokenStatusMsg.clear();
    }
    ImGui::TextDisabled("%s", Localization::T("hf_token_save_hint"));
    if (!g_tokenStatusMsg.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1), "%s", g_tokenStatusMsg.c_str());
    }

    ImGui::Separator();

    ImGui::Text("%s", Localization::T("hf_repo_label"));
    ImGui::SetNextItemWidth(500);
    ImGui::InputText("##hfrepo", g_hfRepoBuffer, sizeof(g_hfRepoBuffer));

    bool downloading = g_downloadState.inProgress.load();

    if (!g_fetchingFiles.load() && !downloading) {
        if (ImGui::Button(Localization::T("fetch_files"))) {
            std::string repo(g_hfRepoBuffer);
            std::string token(g_hfTokenBuffer);
            std::thread(FetchFilesWorker, repo, token).detach();
        }
    } else if (g_fetchingFiles.load()) {
        ImGui::TextDisabled("%s", Localization::T("fetching"));
    }

    if (!g_fetchError.empty()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", g_fetchError.c_str());
        ImGui::TextWrapped("%s", Localization::T("gated_model_hint"));
    }

    ImGui::Separator();

    if (!g_availableFiles.empty()) {
        ImGui::Text("%s", Localization::T("select_file_prompt"));
        ImGui::BeginChild("FileList", ImVec2(0, 150), true);
        for (int i = 0; i < (int)g_availableFiles.size(); i++) {
            bool selected = (g_selectedFileIdx == i);
            if (ImGui::Selectable(g_availableFiles[i].c_str(), selected)) {
                g_selectedFileIdx = i;
            }
        }
        ImGui::EndChild();

        if (!downloading) {
            ImGui::BeginDisabled(g_selectedFileIdx < 0);
            if (ImGui::Button(Localization::T("download"), ImVec2(150, 0))) {
                std::string repo(g_hfRepoBuffer);
                std::string token(g_hfTokenBuffer);
                std::string filename = g_availableFiles[g_selectedFileIdx];
                g_downloadState.cancelRequested = false;
                g_downloadState.errorMessage.clear();
                std::thread(DownloadWorker, repo, filename, token).detach();
            }
            ImGui::EndDisabled();
        }
    }

    if (downloading) {
        float pct = (float)(g_downloadState.percent.load() / 100.0);
        ImGui::ProgressBar(pct, ImVec2(300, 0));
        ImGui::SameLine();
        ImGui::Text("%s", g_downloadState.currentFile.c_str());
        if (ImGui::Button(Localization::T("cancel"))) {
            g_downloadState.cancelRequested = true;
        }
    }

    if (!g_downloadState.errorMessage.empty()) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", g_downloadState.errorMessage.c_str());
    }

    ImGui::Separator();
    ImGui::Text("%s", Localization::T("downloaded_models"));
    ImGui::BeginChild("LocalModels", ImVec2(0, 0), true);
    for (auto& model : g_localModels) {
        ImGui::BulletText("%s", model.c_str());
        ImGui::SameLine();
        std::string useChatId = std::string(Localization::T("use_in_chat")) + "##chat" + model;
        if (ImGui::SmallButton(useChatId.c_str())) {
            std::string fullPath = "models/" + model;
            strncpy(g_modelPathBuffer, fullPath.c_str(), sizeof(g_modelPathBuffer) - 1);
            SaveConfigFromState();
        }
        ImGui::SameLine();
        std::string useAgentId = std::string(Localization::T("use_in_agent")) + "##agent" + model;
        if (ImGui::SmallButton(useAgentId.c_str())) {
            std::string fullPath = "models/" + model;
            strncpy(g_agentModelPathBuffer, fullPath.c_str(), sizeof(g_agentModelPathBuffer) - 1);
            SaveConfigFromState();
        }
        ImGui::SameLine();
        std::string delId = std::string(Localization::T("delete")) + "##del" + model;
        if (ImGui::SmallButton(delId.c_str())) {
            std::filesystem::remove("models/" + model);
            RefreshLocalModels();
        }
    }
    ImGui::EndChild();
}

// ===================== main =====================

int main() {
    // Todos os caminhos do app (config.json, bin\llama-server.exe, models\,
    // conversations\, tools\) sao relativos. Se o app for aberto com um
    // diretorio de trabalho diferente (atalho com "Iniciar em" errado,
    // associacao de arquivo, linha de comando de outra pasta), NADA
    // funcionava: config nao carregava e o servidor nao subia. Fixar o CWD
    // na pasta do proprio executavel resolve todos esses casos de uma vez.
    {
        wchar_t exePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
            std::filesystem::path dir = std::filesystem::path(exePath).parent_path();
            SetCurrentDirectoryW(dir.c_str());
        }
    }

    auto& cfg = ConfigManager::Instance();
    cfg.Load();

    strncpy(g_hfTokenBuffer, cfg.hfToken.c_str(), sizeof(g_hfTokenBuffer) - 1);
    strncpy(g_modelPathBuffer, cfg.lastModelPath.c_str(), sizeof(g_modelPathBuffer) - 1);
    g_darkMode = cfg.darkMode;
    g_thinkingEnabled = cfg.thinkingEnabled;
    g_webSearchEnabled = cfg.webSearchEnabled;
    g_agentWebSearchEnabled = cfg.agentWebSearchEnabled;
    g_port = cfg.serverPort;
    g_sampling = cfg.sampling;
    strncpy(g_agentModelPathBuffer, cfg.agentModelPath.c_str(), sizeof(g_agentModelPathBuffer) - 1);
    strncpy(g_agentMmprojPathBuffer, cfg.agentMmprojPath.c_str(), sizeof(g_agentMmprojPathBuffer) - 1);
    g_agentProjectFolder = cfg.agentProjectFolder;
    g_agentPort = cfg.agentPort;
    g_agentFreeMode = cfg.agentFreeMode;
    g_agentThinkingEnabled = cfg.agentThinkingEnabled;
    strncpy(g_reviewerModelPathBuffer, cfg.reviewerModelPath.c_str(), sizeof(g_reviewerModelPathBuffer) - 1);
    strncpy(g_reviewerMmprojPathBuffer, cfg.reviewerMmprojPath.c_str(), sizeof(g_reviewerMmprojPathBuffer) - 1);
    g_reviewerPort = cfg.reviewerPort;
    g_useReviewerModel = cfg.useReviewerModel;
    g_useCustomReviewerPrompt = cfg.useCustomReviewerPrompt;
    g_customReviewerPrompt = cfg.customReviewerPrompt;
    g_debateModeEnabled = cfg.debateModeEnabled;
    g_debateRounds = cfg.debateRounds;
    g_extraServerArgs = cfg.extraServerArgs;
    Localization::SetLanguage(cfg.language);
    TemplateManager::Instance().Load();

    g_conversation = g_chatConvMgr.NewConversation();
    g_agentConversation = g_agentConvMgr.NewConversation();
    g_agentConversation.id = "agent_" + g_agentConversation.id;

    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    GLFWwindow* window = glfwCreateWindow(1280, 880, "DuckFaceLLM 2.0.0", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // GLFW windows don't automatically pick up the .exe's embedded icon
    // resource (app.rc) as the window icon, even though the taskbar/Alt+Tab
    // usually falls back to the .exe icon on their own. Setting it
    // explicitly here guarantees the duck icon shows on the window itself
    // too, regardless of that fallback behavior.
    {
        HWND hwnd = glfwGetWin32Window(window);
        HINSTANCE hInstance = GetModuleHandleA(nullptr);
        HICON hIcon = LoadIconA(hInstance, "IDI_APP_ICON");
        if (hIcon) {
            SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // ---------- Fontes ----------
    // A fonte padrao do ImGui (ProggyClean 13px, bitmap) parece "de
    // debug" e nao tem acentos completos nem chines. Carregamos a Segoe UI
    // do proprio Windows (sempre presente) em 17px com o range Latin
    // completo, e MESCLAMOS na mesma fonte os glifos chineses da Microsoft
    // YaHei — assim a UI inteira fica nitida e o idioma ZH funciona de
    // verdade em vez de renderizar '?????'.
    {
        ImFontConfig cfg17{};
        static const ImWchar latinRanges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin-1 (acentos PT/ES)
            0x0100, 0x017F, // Latin Extended-A
            0x2000, 0x206F, // pontuacao geral (aspas tipograficas, dashes)
            0x2190, 0x21FF, // setas
            0x25A0, 0x25FF, // formas geometricas (inclui o circulo de status)
            0,
        };
        const char* segoe = "C:\\Windows\\Fonts\\segoeui.ttf";
        ImFont* base = nullptr;
        if (std::filesystem::exists(segoe)) {
            base = io.Fonts->AddFontFromFileTTF(segoe, 17.0f, &cfg17, latinRanges);
        }
        if (base) {
            // Chines mesclado na mesma fonte (soh carrega os glifos comuns
            // para nao explodir o atlas). YaHei e a fonte de sistema de
            // interface chinesa desde o Windows Vista.
            const char* yahei  = "C:\\Windows\\Fonts\\msyh.ttc";
            const char* yahei2 = "C:\\Windows\\Fonts\\msyh.ttf"; // versoes antigas
            ImFontConfig merge{};
            merge.MergeMode = true;
            const ImWchar* cjk = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
            if (std::filesystem::exists(yahei))
                io.Fonts->AddFontFromFileTTF(yahei, 18.0f, &merge, cjk);
            else if (std::filesystem::exists(yahei2))
                io.Fonts->AddFontFromFileTTF(yahei2, 18.0f, &merge, cjk);

            // Simbolos diversos (engrenagem do botao de configuracoes,
            // etc) vem da Segoe UI Symbol, tambem fonte de sistema.
            static const ImWchar symRanges[] = { 0x2600, 0x27BF, 0 };
            const char* seguisym = "C:\\Windows\\Fonts\\seguisym.ttf";
            ImFontConfig mergeSym{};
            mergeSym.MergeMode = true;
            if (std::filesystem::exists(seguisym))
                io.Fonts->AddFontFromFileTTF(seguisym, 24.0f, &mergeSym, symRanges); // maior: engrenagem legivel
        } else {
            io.Fonts->AddFontDefault(); // fallback improvavel (Windows sem Segoe UI)
        }
    }

    g_darkMode.load() ? ThemeManager::ApplyDark() : ThemeManager::ApplyLight();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    RefreshLocalModels();
    RefreshConversationList();
    RefreshAgentConversationList();

    while (!glfwWindowShouldClose(window)) {
        // Quando ha trabalho em background (geracao, download, agente,
        // servidor carregando), renderizamos continuamente para o texto
        // fluir em tempo real. Parado, esperamos eventos com timeout — o
        // app continua responsivo mas o uso de CPU/GPU ocioso despenca.
        bool busy = g_isGenerating.load() || g_agentRunning.load() ||
                    g_downloadState.inProgress.load() || g_extractingPdf.load() ||
                    g_agentExtractingPdf.load() || g_fetchingFiles.load() ||
                    (g_serverRunning.load() && !g_serverReady.load()) ||
                    (g_agentServerRunning.load() && !g_agentServerReady.load()) ||
                    (g_reviewerServerRunning.load() && !g_reviewerServerReady.load());
        for (auto& d : g_debaters)
            if (d->running.load() && !d->ready.load()) { busy = true; break; }
        if (busy) glfwPollEvents();
        else glfwWaitEventsTimeout(0.10); // acorda a cada 100ms de qualquer forma

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("MainWindow", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        ImGui::Text("DuckFaceLLM");
        ImGui::SameLine();
        ImGui::TextDisabled("v2.0.0");
        ImGui::SameLine(0, 30);
        {
            int activeCount = 0;
            if (g_serverRunning.load()) activeCount++;
            if (g_agentServerRunning.load()) activeCount++;
            if (g_reviewerServerRunning.load()) activeCount++;
            for (auto& d : g_debaters) if (d->running.load()) activeCount++;

            ImGui::BeginDisabled(activeCount == 0);
            if (ImGui::SmallButton(Localization::T("stop_all_servers"))) {
                StopAllServers();
            }
            ImGui::EndDisabled();
            if (activeCount > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%d %s)", activeCount, Localization::T("servers_running_suffix"));
            }

            // Alternador de tema no canto direito do cabecalho: trocar
            // dark/light e algo que se quer fazer rapido, sem precisar ir
            // ate a aba Settings.
            {
                const char* themeLabel = g_darkMode.load()
                    ? Localization::T("theme_toggle_light")
                    : Localization::T("theme_toggle_dark");
                float btnW = ImGui::CalcTextSize(themeLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                ImGui::SameLine(ImGui::GetContentRegionMax().x - btnW);
                if (ImGui::SmallButton(themeLabel)) {
                    bool dark = !g_darkMode.load();
                    g_darkMode = dark;
                    dark ? ThemeManager::ApplyDark() : ThemeManager::ApplyLight();
                    SaveConfigFromState();
                }
            }
        }
        ImGui::Separator();

        // Ordem das abas reorganizada por fluxo de uso: primeiro onde se
        // TRABALHA (Chat, Agent), depois o que se REVISITA (Projects,
        // History), depois os RECURSOS que alimentam o trabalho
        // (Templates, Models) e por ultimo Settings — a convencao de
        // praticamente todo app e a configuracao ficar no fim/canto.
        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem(Localization::T("tab_chat"))) {
                DrawChatTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(Localization::T("tab_agent"))) {
                DrawAgentTab();
                ImGui::EndTabItem();
            }
            // History e Projects sao ambos "listas de conversas salvas",
            // entao vivem juntos numa unica aba com sub-abas — menos
            // poluicao na barra principal.
            if (ImGui::BeginTabItem(Localization::T("tab_conversations"))) {
                if (ImGui::BeginTabBar("ConversationsSubTabs")) {
                    if (ImGui::BeginTabItem(Localization::T("tab_history"))) {
                        DrawHistoryTab();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem(Localization::T("tab_projects"))) {
                        DrawProjectsTab();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(Localization::T("tab_templates"))) {
                DrawTemplatesTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(Localization::T("tab_models"))) {
                DrawModelsTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(Localization::T("tab_settings"))) {
                DrawSettingsTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImVec4 clear = g_darkMode.load() ? ImVec4(0.08f, 0.08f, 0.1f, 1) : ImVec4(0.95f, 0.95f, 0.95f, 1);
        glClearColor(clear.x, clear.y, clear.z, clear.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    g_agentStopRequested = true;
    g_agentApprovalDecision = 2;
    if (g_serverRunning.load()) g_server.Stop();
    if (g_agentServerRunning.load()) g_agentServer.Stop();
    if (g_reviewerServerRunning.load()) g_reviewerServer.Stop();
    for (auto& d : g_debaters) {
        if (d->running.load()) d->server.Stop();
    }
    SaveConfigFromState();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
