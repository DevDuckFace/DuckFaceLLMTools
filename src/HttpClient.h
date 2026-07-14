#pragma once
#include <string>
#include <vector>
#include <functional>

struct SamplingParams {
    float temperature   = 0.7f;
    int   topK          = 40;
    float topP          = 0.9f;
    float minP          = 0.05f;
    float repeatPenalty = 1.1f;
    int   maxTokens     = 2048;
    int   ctxSize       = 4096;
};

// One turn in a conversation, as sent to the model. role is "system",
// "user" or "assistant". imageBase64/imageMime are optional — set them to
// attach an image to a "user" turn for vision-capable models (requires the
// server to be started with an mmproj file loaded).
struct ChatTurn {
    std::string role;
    std::string content;
    std::string imageBase64;
    std::string imageMime;
};

class HttpClient {
public:
    // Streams a chat completion given the FULL conversation history (system
    // prompt + all prior turns + the new user message already appended by
    // the caller). Sending the whole history on every call is what makes
    // the model actually continue the conversation instead of restarting
    // from scratch each time.
    static bool ChatStream(const std::vector<ChatTurn>& history,
                            int port,
                            const SamplingParams& params,
                            bool enableThinking,
                            std::function<void(bool isReasoning, const std::string& text)> onDelta,
                            std::function<bool()> shouldStop);

    // Fallback: manually splits <think>...</think> in case the server does
    // not return reasoning_content separately.
    struct SplitResult { std::string thinking; std::string finalAnswer; };
    static SplitResult SplitThinking(const std::string& fullText);

    static bool CheckHealth(int port);
};
