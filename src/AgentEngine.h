#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <system_error>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <windows.h>
#include <nlohmann/json.hpp>
#include "UiHelpers.h"

using json = nlohmann::json;

enum class AgentActionType {
    CreateFile,
    EditFile,
    DeleteFile,
    ReadFile,
    ListFiles,
    Done,
    Invalid
};

struct AgentAction {
    AgentActionType type = AgentActionType::Invalid;
    std::string path;
    std::string content;
    std::string summary;
    std::string rawText;
};

namespace AgentEngine {

// Converts a UTF-8 string (as returned by our Windows folder/file pickers)
// into a std::filesystem::path safely, regardless of the system's ANSI code
// page. Constructing std::filesystem::path directly from a UTF-8
// std::string on Windows can silently mangle accented characters (very
// common in PT-BR folder names like "Área de Trabalho"), which was causing
// the sandbox path check to fail and file writes to be silently rejected.
inline std::filesystem::path Utf8ToPath(const std::string& utf8) {
    if (utf8.empty()) return std::filesystem::path();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return std::filesystem::path(utf8);
    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), wlen);
    if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
    return std::filesystem::path(wide);
}

inline AgentActionType ParseActionType(const std::string& s) {
    std::string clean = s;
    while (!clean.empty() && (clean.back() == '.' || clean.back() == ':' || clean.back() == ' ')) clean.pop_back();
    if (clean == "create_file") return AgentActionType::CreateFile;
    if (clean == "edit_file") return AgentActionType::EditFile;
    if (clean == "delete_file") return AgentActionType::DeleteFile;
    if (clean == "read_file") return AgentActionType::ReadFile;
    if (clean == "list_files") return AgentActionType::ListFiles;
    if (clean == "done") return AgentActionType::Done;
    return AgentActionType::Invalid;
}

inline std::string TrimStr(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

inline std::string ToLowerStr(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// Parses the plain-text delimited action format:
//
//   ACTION: create_file
//   PATH: relative/path.ext
//   SUMMARY: short note
//   CONTENT_START
//   ...raw file content, verbatim, no escaping needed...
//   CONTENT_END
//
// This is deliberately NOT JSON: local models frequently fail to escape
// quotes/newlines correctly inside JSON string values, which made every
// action with real code content fail to parse. Plain delimiters need no
// escaping at all, which is dramatically more reliable in practice.
// Models sometimes copy a path straight out of an HTML tag they just wrote
// (e.g. <link href="style.css?v=1.0">) and reuse it as the file path for
// create_file/edit_file. Characters like '?' and '#' are valid in URLs but
// forbidden in real Windows filenames - trying to open such a path just
// fails, and the resulting error ("could not open file") looks exactly
// like a permissions problem even though it's actually an invalid name.
// Strip the URL-only parts here so the actual file gets created correctly.
inline std::string SanitizeRelativePath(const std::string& rawPath) {
    std::string path = rawPath;
    size_t qPos = path.find('?');
    if (qPos != std::string::npos) path = path.substr(0, qPos);
    size_t hashPos = path.find('#');
    if (hashPos != std::string::npos) path = path.substr(0, hashPos);
    return path;
}

inline AgentAction ParseAction(const std::string& modelOutput) {
    AgentAction result;
    result.rawText = modelOutput;

    std::string blockText;
    auto segments = UiHelpers::ParseCodeBlocks(modelOutput);
    for (auto& seg : segments) {
        if (seg.isCode) {
            std::string lang = ToLowerStr(seg.language);
            if (lang.find("action") != std::string::npos || lang.empty()) {
                blockText = seg.content;
                break;
            }
        }
    }
    if (blockText.empty()) {
        // No fenced ```action block found (the model may have skipped the
        // fence entirely, e.g. responding with just the bare word "done").
        // Fall through to parsing the raw response directly - the per-line
        // parser below already tolerates bare action keywords and missing
        // "ACTION:" prefixes, so this covers that case instead of giving up.
        blockText = modelOutput;
    }

    std::istringstream stream(blockText);
    std::string line;
    bool inContent = false;
    std::ostringstream contentBuf;
    bool sawAction = false;

    while (std::getline(stream, line)) {
        std::string trimmedLine = TrimStr(line);

        if (inContent) {
            if (ToLowerStr(trimmedLine) == "content_end") {
                inContent = false;
                continue;
            }
            contentBuf << line << "\n";
            continue;
        }

        if (ToLowerStr(trimmedLine) == "content_start") {
            inContent = true;
            continue;
        }

        auto startsWithKey = [&](const char* key) -> bool {
            return trimmedLine.size() >= strlen(key) &&
                   ToLowerStr(trimmedLine.substr(0, strlen(key))) == ToLowerStr(key);
        };

        if (startsWithKey("ACTION:")) {
            result.type = ParseActionType(TrimStr(trimmedLine.substr(7)));
            sawAction = true;
        } else if (startsWithKey("PATH:")) {
            result.path = SanitizeRelativePath(TrimStr(trimmedLine.substr(5)));
        } else if (startsWithKey("SUMMARY:")) {
            result.summary = TrimStr(trimmedLine.substr(8));
        } else if (!sawAction) {
            // Be lenient: models sometimes write the action name on its own
            // line (e.g. just "done" or "create_file") instead of the
            // "ACTION: done" form we ask for. Recognizing this prevents a
            // whole class of "it said it was done/edited but nothing
            // actually happened" bugs caused by an otherwise-clear response
            // being rejected over a strict formatting technicality.
            std::string bareWord = ToLowerStr(trimmedLine);
            while (!bareWord.empty() && (bareWord.back() == '.' || bareWord.back() == ':')) bareWord.pop_back();
            AgentActionType maybeType = ParseActionType(bareWord);
            if (maybeType != AgentActionType::Invalid) {
                result.type = maybeType;
                sawAction = true;
            }
        }
    }

    result.content = contentBuf.str();
    // Remove exactly one trailing newline we always add after the last line.
    if (!result.content.empty() && result.content.back() == '\n') result.content.pop_back();

    if (!sawAction) {
        // Backward-compatible fallback: some models (especially ones that
        // already have JSON-formatted examples earlier in the conversation
        // history, from before this app switched formats) keep responding
        // with the old {"action": ..., "path": ..., "content": ...} JSON
        // shape despite instructions. Rather than fail every time, try to
        // parse that too before giving up.
        try {
            json j = json::parse(blockText);
            std::string actionStr = j.value("action", j.value("ACTION", ""));
            AgentActionType parsedType = ParseActionType(actionStr);
            if (parsedType != AgentActionType::Invalid) {
                result.type = parsedType;
                result.path = j.value("path", j.value("PATH", ""));
                result.content = j.value("content", j.value("CONTENT", ""));
                result.summary = j.value("summary", j.value("SUMMARY", ""));
                return result;
            }
        } catch (...) {
            // not valid JSON either: fall through to Invalid below
        }
        result.type = AgentActionType::Invalid;
    }
    return result;
}

// Resolves relativePath against projectFolder and verifies the result
// stays strictly inside it (blocks "../" escapes, absolute paths pointing
// elsewhere, etc). This is the sandbox boundary — the agent can never
// touch anything outside the chosen project folder.
inline bool ResolveSafePath(const std::string& projectFolder, const std::string& relativePathRaw,
                             std::filesystem::path& outAbsPath, std::string& errorOut) {
    if (projectFolder.empty()) {
        errorOut = "No project folder is set.";
        return false;
    }
    if (relativePathRaw.empty()) {
        errorOut = "Empty path.";
        return false;
    }

    std::string relativePath = SanitizeRelativePath(relativePathRaw);
    if (relativePath.empty()) {
        errorOut = "Path became empty after removing URL-style suffix (?..., #...): " + relativePathRaw;
        return false;
    }

    // Any of these remaining in a relative path are forbidden by Windows
    // and will fail to open regardless of folder permissions - catching
    // this here gives a much clearer error than the generic filesystem one.
    const std::string forbidden = "<>:\"|*";
    for (char c : forbidden) {
        if (relativePath.find(c) != std::string::npos) {
            errorOut = std::string("Path contains a character not allowed in Windows filenames ('") + c +
                       "'): " + relativePath;
            return false;
        }
    }

    std::filesystem::path base;
    std::filesystem::path candidate;
    try {
        base = std::filesystem::weakly_canonical(Utf8ToPath(projectFolder));
        candidate = std::filesystem::weakly_canonical(base / Utf8ToPath(relativePath));
    } catch (std::exception& e) {
        errorOut = std::string("Invalid path: ") + e.what();
        return false;
    }

    // SECURITY: a plain string-prefix comparison is NOT enough here.
    // "C:\Users\X\proj2" starts with "C:\Users\X\proj", so a sibling folder
    // sharing the prefix would wrongly pass and let the agent write OUTSIDE
    // the sandbox. Require the character right after the prefix to be a
    // path separator (or the paths to be identical), and compare
    // case-insensitively since Windows paths are case-insensitive and
    // weakly_canonical doesn't normalize case.
    auto toLowerW = [](std::wstring s) {
        for (auto& c : s) c = (wchar_t)towlower(c);
        return s;
    };
    std::wstring baseStr = toLowerW(base.wstring());
    std::wstring candStr = toLowerW(candidate.wstring());
    while (!baseStr.empty() && (baseStr.back() == L'\\' || baseStr.back() == L'/')) baseStr.pop_back();

    bool inside = false;
    if (candStr.size() == baseStr.size() && candStr == baseStr) {
        inside = true;
    } else if (candStr.size() > baseStr.size() &&
               candStr.compare(0, baseStr.size(), baseStr) == 0) {
        wchar_t next = candStr[baseStr.size()];
        inside = (next == L'\\' || next == L'/');
    }
    if (!inside) {
        errorOut = "Path escapes the project folder - blocked for safety.";
        return false;
    }

    outAbsPath = candidate;
    return true;
}

inline std::string ExecuteReadOnly(const AgentAction& action, const std::string& projectFolder, bool& success) {
    success = false;
    std::filesystem::path absPath;
    std::string err;
    if (!ResolveSafePath(projectFolder, action.path, absPath, err)) {
        return "ERROR: " + err;
    }

    try {
        if (action.type == AgentActionType::ReadFile) {
            std::ifstream f(absPath, std::ios::binary);
            if (!f.is_open()) return "ERROR: file not found: " + action.path;
            std::ostringstream ss;
            ss << f.rdbuf();
            success = true;
            return ss.str();
        }

        if (action.type == AgentActionType::ListFiles) {
            if (!std::filesystem::exists(absPath)) return "ERROR: folder not found: " + action.path;
            std::ostringstream ss;
            for (auto& entry : std::filesystem::directory_iterator(absPath)) {
                ss << (entry.is_directory() ? "[dir] " : "[file] ")
                   << entry.path().filename().string() << "\n";
            }
            success = true;
            return ss.str();
        }
    } catch (std::exception& e) {
        // A filesystem_error here (permission issues, broken reparse
        // points, etc) must never escape uncaught — an exception thrown
        // out of a detached background thread terminates the ENTIRE
        // application instantly with no warning, which is exactly what
        // was likely causing the app to "close itself" during agent runs.
        return std::string("ERROR: filesystem error: ") + e.what();
    }

    return "ERROR: not a read-only action";
}

// Executes a write action (create_file/edit_file). Only call this AFTER
// approval has been granted (or immediately, if free mode is on) — this
// function itself does not ask for confirmation. Returns the absolute
// path written to on success, via absPathOut, so the caller can read it
// back for the model's self-review step.
// Before overwriting (edit_file) or deleting a file, keeps a timestamped
// copy in a hidden backup folder inside the project, so an approved change
// you didn't actually want isn't permanently unrecoverable. This is
// deliberately best-effort and silent: a backup failure should never block
// the actual requested operation.
inline void BackupFileBeforeChange(const std::filesystem::path& absPath, const std::string& projectFolder) {
    if (!std::filesystem::exists(absPath)) return; // nothing to preserve (brand new file)

    try {
        std::filesystem::path base = Utf8ToPath(projectFolder);
        std::filesystem::path rel = std::filesystem::relative(absPath, base);
        std::filesystem::path backupDir = base / ".dffastllm_backup" / rel.parent_path();
        std::filesystem::create_directories(backupDir);

        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tmBuf{};
        localtime_s(&tmBuf, &tt);
        std::ostringstream ts;
        ts << std::put_time(&tmBuf, "%Y%m%d_%H%M%S");

        std::filesystem::path backupPath = backupDir / (rel.filename().string() + "." + ts.str() + ".bak");
        std::filesystem::copy_file(absPath, backupPath, std::filesystem::copy_options::overwrite_existing);
    } catch (...) {
        // best-effort only
    }
}

inline bool ExecuteWrite(const AgentAction& action, const std::string& projectFolder,
                          std::filesystem::path& absPathOut, std::string& errorOut) {
    if (!ResolveSafePath(projectFolder, action.path, absPathOut, errorOut)) {
        return false;
    }

    try {
        BackupFileBeforeChange(absPathOut, projectFolder);
        std::filesystem::create_directories(absPathOut.parent_path());
        std::ofstream f(absPathOut, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) {
            errorOut = "Could not open file for writing (check folder permissions): " + action.path;
            return false;
        }
        f << action.content;
        f.flush();
        f.close();

        // Never just trust the write call succeeded — actually verify the
        // file exists on disk afterward and its size matches what we tried
        // to write. This is what lets the app tell the model the truth
        // ("it says done but nothing was actually written") instead of
        // reporting success based only on the API call not throwing.
        if (!std::filesystem::exists(absPathOut)) {
            errorOut = "Write call completed but the file does not exist afterward "
                       "(possibly blocked by antivirus, a locked file, or a permissions issue).";
            return false;
        }
        std::error_code sizeErr;
        auto actualSize = std::filesystem::file_size(absPathOut, sizeErr);
        if (!sizeErr && actualSize != action.content.size()) {
            errorOut = "File was written but its size on disk (" + std::to_string(actualSize) +
                       " bytes) doesn't match the content sent (" + std::to_string(action.content.size()) +
                       " bytes) - the write may be incomplete.";
            return false;
        }

        return true;
    } catch (std::exception& e) {
        errorOut = std::string("Filesystem error: ") + e.what();
        return false;
    }
}

// Deletes a file inside the sandboxed project folder. Verifies the file
// actually existed beforehand and is actually gone afterward, rather than
// just trusting the remove() call.
inline bool ExecuteDelete(const AgentAction& action, const std::string& projectFolder, std::string& errorOut) {
    std::filesystem::path absPath;
    if (!ResolveSafePath(projectFolder, action.path, absPath, errorOut)) {
        return false;
    }

    try {
        if (!std::filesystem::exists(absPath)) {
            errorOut = "File does not exist, nothing to delete: " + action.path;
            return false;
        }
        BackupFileBeforeChange(absPath, projectFolder);
        std::error_code removeErr;
        std::filesystem::remove(absPath, removeErr);
        if (removeErr) {
            errorOut = "Filesystem error while deleting: " + removeErr.message();
            return false;
        }
        if (std::filesystem::exists(absPath)) {
            errorOut = "Delete call completed but the file is still there afterward "
                       "(possibly locked by another program).";
            return false;
        }
        return true;
    } catch (std::exception& e) {
        errorOut = std::string("Filesystem error: ") + e.what();
        return false;
    }
}

// System prompt teaching the model the action protocol. `extraExpertise`,
// if non-empty, is an auto-selected or forced prompt-template's
// instructions (e.g. the built-in "C++ Programming" template), appended so
// the agent gets domain-specific best practices on top of the action
// protocol itself.
// Default system prompt for the optional second "Reviewer" model. Used
// whenever the user hasn't turned on a custom prompt for it — always
// available so reviewing works out of the box with zero configuration.
inline std::string DefaultReviewerSystemPrompt() {
    return
        "You are a Senior Software Engineer acting as an expert Code Reviewer, focused on correctness, "
        "security, performance, and project organization. You will be told whether the action just performed "
        "was create_file (a brand new file) or edit_file (a modification of an existing file), shown the "
        "real list of files that currently exist in the project folder, and the specific file that was just "
        "written to disk. Perform these checks, in order:\n\n"

        "0) FILE PRESENCE CHECK (always do this first, regardless of create or edit): Independently confirm "
        "the file path is actually present in the file listing you were given - don't just trust that the "
        "action was labeled a success. If it is missing from the listing, this is a critical failure: the "
        "create_file/edit_file action did not really take effect, and the implementer must retry it.\n\n"

        "1) STRUCTURAL CHECK: Confirm the file is at a sensible path for this kind of project (e.g. CSS in a "
        "styles/ or css/ folder if that's the project's convention, components where components belong, "
        "etc). If the file references other files (imports, <link>, <script src>, relative paths) that do "
        "NOT appear in the file listing, that is a real problem: those files must actually be created with "
        "their own create_file action - not just mentioned in text. Flag this explicitly.\n\n"

        "2) TECHNICAL CHECK - adapt to the action type:\n"
        "   - If this was create_file: check the new file is complete and self-consistent, and doesn't "
        "duplicate or conflict with something that already exists for the same purpose.\n"
        "   - If this was edit_file: check the edit didn't remove or break anything that was previously "
        "needed elsewhere in the project (compare against what the other files expect from this one).\n"
        "   In both cases, look for: bugs and logic errors, bad practices/code smells, security issues "
        "(exposed keys/secrets, injection risks), missing optimizations or redundant code, poor readability "
        "or inconsistent naming, and inconsistencies with the other files already listed (e.g. HTML using a "
        "class/id that this CSS file never defines, or vice versa; a function called from elsewhere with the "
        "wrong name or signature).\n\n"

        "IMPORTANT - the implementer only makes real changes through the create_file/edit_file actions. If "
        "your feedback requires a change, be explicit that it must be applied via a create_file or edit_file "
        "action to the actual file (with the complete corrected file content) - never accept or imply that "
        "describing a change in words is enough. Some models are lazy and only talk about fixing something "
        "without ever actually calling create_file/edit_file; do not let that pass as done.\n\n"

        "RESPONSE FORMAT - follow this exactly:\n"
        "- If the file is correct, complete, and consistent with the rest of the project: respond with "
        "EXACTLY the single word APPROVED and nothing else.\n"
        "- Otherwise, respond in exactly two short sections:\n"
        "  File Status: (confirm the path is correct and present on disk, or state exactly where it should "
        "be instead, or flag that it's missing entirely)\n"
        "  Required Changes: a direct, numbered list of what must change and why - no fluff\n"
        "- Do NOT write or rewrite the file's code yourself. You are the reviewer, not the implementer - your "
        "job is to describe precisely what is wrong so the implementer can fix it with its own create_file "
        "or edit_file action.";
}

inline std::string BuildAgentSystemPrompt(const std::string& projectFolder, const std::string& extraExpertise) {
    std::ostringstream ss;
    ss <<
        "You are an autonomous coding agent working ONLY inside this project folder:\n"
        << projectFolder << "\n\n"
        "Take exactly ONE action per response. Respond with ONLY a single fenced code block "
        "labeled 'action', in this EXACT plain-text format (no JSON, no escaping needed for the content):\n\n"
        "```action\n"
        "ACTION: create_file\n"
        "PATH: relative/path.ext\n"
        "SUMMARY: short note about this step\n"
        "CONTENT_START\n"
        "...the full, exact file content goes here, verbatim, spanning as many lines as needed...\n"
        "CONTENT_END\n"
        "```\n\n"
        "Valid ACTION values:\n"
        "- create_file: needs PATH and CONTENT_START/CONTENT_END (full file contents)\n"
        "- edit_file: needs PATH and CONTENT_START/CONTENT_END (the COMPLETE new file contents, not a diff)\n"
        "- delete_file: needs PATH only (no content block) - permanently deletes that file\n"
        "- read_file: needs PATH only (no content block)\n"
        "- list_files: needs PATH only (use \".\" for the project root)\n"
        "- done: needs SUMMARY only (use this ONLY when the entire task is fully complete)\n\n"
        "Rules:\n"
        "- All paths are relative to the project folder above. Never use \"..\" or absolute paths.\n"
        "- Never wrap CONTENT_START/CONTENT_END or the file content itself in extra quotes or escaping.\n"
        "- After each action you'll be shown the result (including the file you just wrote, for you to "
        "double-check). If it looks wrong or incomplete, send another edit_file action for the same path "
        "to fix it before moving on. Only proceed to the next step once that file is correct.\n"
        "- Prefer list_files/read_file first if you're unsure what already exists.\n"
        "- When the ENTIRE requested task is complete and verified, respond with the \"done\" action.\n";

    if (!extraExpertise.empty()) {
        ss << "\nAdditional expertise for this specific task:\n" << extraExpertise << "\n";
    }

    return ss.str();
}

// Recursively lists every file (not directories) inside the project
// folder, as paths relative to it — used for the final "review before
// done" pass, so the model checks against what's actually on disk instead
// of just trusting its own memory of what it wrote.
inline std::vector<std::string> ListFilesRecursive(const std::string& projectFolder) {
    std::vector<std::string> out;
    std::filesystem::path base;
    try {
        base = Utf8ToPath(projectFolder);
        if (!std::filesystem::exists(base)) return out;
        for (auto& entry : std::filesystem::recursive_directory_iterator(base)) {
            auto rel = std::filesystem::relative(entry.path(), base);
            std::string relStr = rel.generic_string();
            if (relStr.rfind(".dffastllm_backup", 0) == 0) continue; // hide our own backups from the model
            if (entry.is_regular_file()) {
                out.push_back(relStr);
            }
        }
    } catch (...) {
        // leave out empty on any filesystem error
    }
    return out;
}

// Rough token-count estimate (chars/4, a commonly used approximation for
// English/code text) — good enough to proactively trigger a context
// compaction before we actually hit the server's hard context limit. We
// don't have access to the real tokenizer here, so this is intentionally
// conservative.
inline int EstimateTokens(const std::string& text) {
    return (int)(text.size() / 4) + 1;
}

} // namespace AgentEngine
