#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <cstring>
#include <unordered_map>
#include <algorithm>

// A segment of a message: either plain text or a fenced code block
// (```lang ... ```). Used to render code separately with its own
// one-click copy button, distinct from the surrounding prose.
struct TextSegment {
    bool isCode = false;
    std::string language;
    std::string content;
};

namespace UiHelpers {

// Wraps ImGui's InputText(Multiline) to grow a std::string dynamically as
// the user types, instead of being capped by a fixed char[] buffer size.
// This is the standard ImGuiInputTextFlags_CallbackResize pattern.
inline int ResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

inline bool InputTextMultilineUnlimited(const char* label, std::string* str, ImVec2 size,
                                         ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputTextMultiline(label, str->data(), str->capacity() + 1, size, flags,
                                      ResizeCallback, (void*)str);
}

inline bool InputTextUnlimited(const char* label, std::string* str, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputText(label, str->data(), str->capacity() + 1, flags, ResizeCallback, (void*)str);
}

// Hard-wraps text to fit within wrapWidth pixels, using the current font's
// metrics — the same logic ImGui uses internally for TextWrapped, but
// applied to build a string with real '\n' breaks so a read-only
// InputTextMultiline (which does NOT auto word-wrap on its own) displays it
// properly instead of requiring horizontal scrolling.
inline std::string WrapText(const std::string& text, float wrapWidth) {
    if (wrapWidth < 20.0f) wrapWidth = 20.0f;

    // Safety cap: word-wrapping a huge blob of text (e.g. a large generated
    // file shown back for review) character-by-character can make the UI
    // unresponsive for several seconds, which can look like the app froze
    // or crashed. Truncate what we actually lay out on screen — the Copy
    // button still copies the full original text regardless.
    const size_t kMaxWrapChars = 200000;
    bool truncated = false;
    const std::string* sourcePtr = &text;
    std::string truncatedCopy;
    if (text.size() > kMaxWrapChars) {
        truncatedCopy = text.substr(0, kMaxWrapChars);
        truncated = true;
        sourcePtr = &truncatedCopy;
    }
    const std::string& source = *sourcePtr;

    std::string result;
    result.reserve(source.size() + source.size() / 20);

    size_t lineStart = 0;
    while (lineStart <= source.size()) {
        size_t nl = source.find('\n', lineStart);
        std::string line = (nl == std::string::npos) ? source.substr(lineStart) : source.substr(lineStart, nl - lineStart);

        std::string current;
        size_t wpos = 0;
        while (wpos < line.size()) {
            size_t spacePos = line.find(' ', wpos);
            std::string word = (spacePos == std::string::npos)
                ? line.substr(wpos)
                : line.substr(wpos, spacePos - wpos + 1); // include the trailing space

            std::string trial = current + word;
            if (ImGui::CalcTextSize(trial.c_str()).x > wrapWidth && !current.empty()) {
                result += current;
                result += '\n';
                current = word;
            } else {
                current = trial;
            }

            // A single "word" longer than the whole column (long URL, path,
            // minified CSS/JS line, base64 blob, etc): hard-break it using
            // binary search for the break point instead of shrinking one
            // character at a time — the latter is O(n) CalcTextSize calls
            // per break (O(n^2) overall for one long run), which is what
            // was freezing the UI on large files.
            while (ImGui::CalcTextSize(current.c_str()).x > wrapWidth && current.size() > 1) {
                size_t lo = 1, hi = current.size();
                while (lo < hi) {
                    size_t mid = (lo + hi + 1) / 2;
                    if (ImGui::CalcTextSize(current.substr(0, mid).c_str()).x <= wrapWidth) lo = mid;
                    else hi = mid - 1;
                }
                if (lo >= current.size()) break;
                result += current.substr(0, lo);
                result += '\n';
                current = current.substr(lo);
            }

            wpos = (spacePos == std::string::npos) ? line.size() : spacePos + 1;
        }
        result += current;

        if (nl == std::string::npos) break;
        result += '\n';
        lineStart = nl + 1;
    }

    if (truncated) {
        result += "\n\n[... content truncated for display (too large to show in full) - "
                  "use the Copy button to get the complete text ...]";
    }
    return result;
}

inline std::vector<TextSegment> ParseCodeBlocks(const std::string& text) {
    std::vector<TextSegment> segments;
    size_t pos = 0;
    const std::string fence = "```";

    while (pos < text.size()) {
        size_t fenceStart = text.find(fence, pos);
        if (fenceStart == std::string::npos) {
            TextSegment seg;
            seg.isCode = false;
            seg.content = text.substr(pos);
            if (!seg.content.empty()) segments.push_back(seg);
            break;
        }

        if (fenceStart > pos) {
            TextSegment seg;
            seg.isCode = false;
            seg.content = text.substr(pos, fenceStart - pos);
            segments.push_back(seg);
        }

        size_t langLineEnd = text.find('\n', fenceStart + 3);
        std::string lang;
        size_t codeStart;
        if (langLineEnd != std::string::npos) {
            lang = text.substr(fenceStart + 3, langLineEnd - (fenceStart + 3));
            codeStart = langLineEnd + 1;
        } else {
            codeStart = fenceStart + 3;
        }

        size_t fenceEnd = text.find(fence, codeStart);
        TextSegment codeSeg;
        codeSeg.isCode = true;
        codeSeg.language = lang;
        if (fenceEnd != std::string::npos) {
            codeSeg.content = text.substr(codeStart, fenceEnd - codeStart);
            pos = fenceEnd + 3;
        } else {
            // unterminated fence (still streaming): show what we have so far
            codeSeg.content = text.substr(codeStart);
            pos = text.size();
        }
        // strip a single trailing newline for cleaner display
        if (!codeSeg.content.empty() && codeSeg.content.back() == '\n') {
            codeSeg.content.pop_back();
        }
        segments.push_back(codeSeg);
    }

    return segments;
}

// Renders read-only, mouse-selectable, Ctrl+C-copyable text using an
// InputTextMultiline in read-only mode (ImGui text widgets aren't
// selectable on their own; this is the standard workaround).
// Caches the wrapped version of a piece of text per widget id, so
// word-wrapping (which walks every character and calls ImGui::CalcTextSize
// repeatedly) only reruns when the width actually changes (e.g. dragging a
// column splitter) or the text itself changes - not on every single frame,
// which was wasted work for messages that never change once rendered.
struct WrapCacheEntry {
    float width = -1.0f;
    size_t contentHash = 0;
    std::string wrapped;
};
inline std::unordered_map<std::string, WrapCacheEntry>& WrapCache() {
    static std::unordered_map<std::string, WrapCacheEntry> cache;
    return cache;
}

inline void SelectableText(const char* idSuffix, const std::string& content, float width, ImVec4 bgColor, bool wrap = true) {
    if (content.empty()) return;

    std::string display;
    if (wrap) {
        auto& entry = WrapCache()[idSuffix];
        size_t h = std::hash<std::string>{}(content);
        if (entry.width == width && entry.contentHash == h) {
            display = entry.wrapped;
        } else {
            display = WrapText(content, width - 12.0f);
            entry.width = width;
            entry.contentHash = h;
            entry.wrapped = display;
        }
    } else {
        display = content;
    }

    int lineCount = 1;
    for (char c : display) if (c == '\n') lineCount++;
    float lineHeight = ImGui::GetTextLineHeight();
    float height = (lineCount + 1) * lineHeight * 1.15f;
    if (height < lineHeight * 2) height = lineHeight * 2;
    if (height > 3000.0f) height = 3000.0f;

    std::string idStr = std::string("##seltext_") + idSuffix;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, bgColor);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

    std::string mutableCopy = display;
    mutableCopy.reserve(mutableCopy.size() + 1);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_ReadOnly;
    if (!wrap) flags |= ImGuiInputTextFlags_NoHorizontalScroll; // code: let the child handle scrolling instead

    ImGui::SetNextItemWidth(width);
    ImGui::InputTextMultiline(
        idStr.c_str(),
        mutableCopy.data(),
        mutableCopy.capacity() + 1,
        ImVec2(width, height),
        flags
    );

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// Renders one full message's content: prose segments as selectable text,
// code segments in a distinct dark panel with a one-click "Copy" button.
// A "Copy all" button for the whole raw message is drawn above the content
// by the caller (see main.cpp) since it needs the raw, unparsed text.
inline void RenderContentWithCodeBlocks(const std::string& idSuffix, const std::string& content, float width) {
    auto segments = ParseCodeBlocks(content);
    int codeBlockIdx = 0;

    for (auto& seg : segments) {
        if (!seg.isCode) {
            SelectableText((idSuffix + std::string("_p") + std::to_string(codeBlockIdx)).c_str(),
                            seg.content, width, ImVec4(0, 0, 0, 0));
        } else {
            std::string blockId = idSuffix + "_code" + std::to_string(codeBlockIdx);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
            ImGui::BeginChild(("codeblock##" + blockId).c_str(), ImVec2(width, 0), true,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);

            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s",
                                seg.language.empty() ? "code" : seg.language.c_str());
            ImGui::SameLine(width - 70);
            if (ImGui::SmallButton(("Copy##" + blockId).c_str())) {
                ImGui::SetClipboardText(seg.content.c_str());
            }
            ImGui::Separator();

            SelectableText(blockId.c_str(), seg.content, width - 16, ImVec4(0, 0, 0, 0), /*wrap=*/false);

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        codeBlockIdx++;
    }
}

// Draws a thin vertical drag handle between two columns. Updates *ratio
// (0..1 share of usableWidth) based on horizontal mouse drag while active.
// Returns true if the ratio changed this frame.
inline bool VerticalSplitter(const char* id, float height, float* ratio, float usableWidth,
                              float minRatio = 0.12f, float maxRatio = 0.75f,
                              bool ratioControlsRightSide = false) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.34f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.55f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
    ImGui::Button(id, ImVec2(6.0f, height));
    ImGui::PopStyleColor(3);

    bool changed = false;
    if (ImGui::IsItemActive() && usableWidth > 1.0f) {
        float deltaRatio = ImGui::GetIO().MouseDelta.x / usableWidth;
        // If *ratio tracks the column to the RIGHT of this handle, dragging
        // the handle to the right should SHRINK that column (and grow the
        // one on the left) — the opposite sign from tracking a left column.
        if (ratioControlsRightSide) deltaRatio = -deltaRatio;
        if (deltaRatio != 0.0f) {
            *ratio += deltaRatio;
            if (*ratio < minRatio) *ratio = minRatio;
            if (*ratio > maxRatio) *ratio = maxRatio;
            changed = true;
        }
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    ImGui::SameLine();
    return changed;
}

// Renders a simple line-based diff between oldContent and newContent:
// unchanged lines in the default color, removed lines in red with a
// leading "-", added lines in green with a leading "+". Used in the
// Agent's approval box for edit_file, so you can see what actually
// changed instead of having to re-read the whole file to tell.
inline void RenderLineDiff(const char* idSuffix, const std::string& oldContent,
                            const std::string& newContent, float width, float height) {
    auto splitLines = [](const std::string& s) {
        std::vector<std::string> lines;
        size_t start = 0;
        while (start <= s.size()) {
            size_t nl = s.find('\n', start);
            if (nl == std::string::npos) { lines.push_back(s.substr(start)); break; }
            lines.push_back(s.substr(start, nl - start));
            start = nl + 1;
        }
        return lines;
    };

    std::vector<std::string> oldLines = splitLines(oldContent);
    std::vector<std::string> newLines = splitLines(newContent);
    size_t n = oldLines.size(), m = newLines.size();

    ImGui::BeginChild((std::string("diffview##") + idSuffix).c_str(), ImVec2(width, height), true);

    // O(n*m) LCS-based diff; fine for normal source files, but skip it for
    // huge ones (the DP table would get too big/slow) and just show the
    // new content plainly instead.
    if ((long long)n * (long long)m > 2000000) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "(File too large for a line-by-line diff - showing new content only)");
        ImGui::Separator();
        for (auto& line : newLines) ImGui::TextUnformatted(line.c_str());
        ImGui::EndChild();
        return;
    }

    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = (int)n - 1; i >= 0; i--) {
        for (int j = (int)m - 1; j >= 0; j--) {
            dp[i][j] = (oldLines[i] == newLines[j]) ? dp[i + 1][j + 1] + 1 : std::max(dp[i + 1][j], dp[i][j + 1]);
        }
    }

    size_t i = 0, j = 0;
    while (i < n && j < m) {
        if (oldLines[i] == newLines[j]) {
            ImGui::TextUnformatted(oldLines[i].c_str());
            i++; j++;
        } else if (dp[i + 1][j] >= dp[i][j + 1]) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "- %s", oldLines[i].c_str());
            i++;
        } else {
            ImGui::TextColored(ImVec4(0.45f, 1.0f, 0.45f, 1.0f), "+ %s", newLines[j].c_str());
            j++;
        }
    }
    while (i < n) { ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "- %s", oldLines[i].c_str()); i++; }
    while (j < m) { ImGui::TextColored(ImVec4(0.45f, 1.0f, 0.45f, 1.0f), "+ %s", newLines[j].c_str()); j++; }

    ImGui::EndChild();
}

} // namespace UiHelpers
