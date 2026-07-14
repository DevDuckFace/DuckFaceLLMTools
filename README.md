# DuckFaceLLM               ![image alt](https://github.com/DevDuckFace/DuckFaceLLMTools/blob/7a31106abf4ef9e9baad903070413d550dfb3cf2/dasd213.png)(https://github.com/DevDuckFace/DuckFaceLLMTools/releases/download/dfllmtools/DuckFaceLLM_Setup_2.0.0.exe)
![image alt](https://github.com/DevDuckFace/DuckFaceLLMTools/blob/490c83d5116f35eb0fec349f04af9cdcc961d6c7/Screenshot_5.png)
A C++ desktop GUI app that runs local LLMs through a bundled **llama.cpp**
server. Features:

- Runs any **GGUF** model locally through `llama-server.exe` (llama.cpp's
  engine only executes `.gguf` files — that's the one format this app
  supports, on purpose, so there's no half-working "download but can't run"
  path);
- Downloads `.gguf` models directly from **Hugging Face**, including
  private/gated models using **your own access token**;
- **Continuous conversations with real context**: every message sends the
  full conversation history to the model, so it actually remembers earlier
  turns instead of restarting fresh each time. Conversations are
  auto-saved to disk and listed in a **History** tab, so you can close the
  app and resume any past conversation later;
- Adjustable **sampling parameters**: temperature, top-k, top-p, min-p,
  repeat penalty, max tokens and context size — all editable from the GUI
  and persisted between sessions;
- Built-in **and user-created prompt template library**: specialized system
  prompts for coding (C++, Python, web, SQL, shell), debugging, code review,
  teaching, translation, summarization, writing and regex, ready to use out
  of the box. DFFastLLM automatically picks the best-matching template based
  on what you type — similar to how this very assistant adapts its approach
  depending on the task. You can **add your own templates from the GUI** (no
  code editing, no limit on how many), and **delete any template — including
  the built-in ones** — with a one-click restore if you change your mind;
- **Streaming generation** with native **thinking/reasoning separation**:
  uses llama-server's `/v1/chat/completions` endpoint with `--jinja` and
  `--reasoning-format auto`, so models that support reasoning (DeepSeek-R1,
  QwQ, Qwen3-thinking, etc.) show their reasoning in a separate live panel,
  cleanly split from the final answer;
- **Dark/light mode** and **4 languages**: English, Portuguese (Brazil),
  Spanish and Chinese;
- **Agent tab**: an autonomous coding agent that creates and edits files by
  itself inside a project folder you choose, using its own model/server
  configuration. Strictly sandboxed to that one folder — it can never touch
  anything outside it. Choose between always asking for your approval
  before each file change, or letting it work freely without asking (still
  confined to the folder either way);
- Runs as a **pure GUI app on Windows** — no console/terminal window opens
  alongside it;

---

## 1. Prerequisites

| Tool | Purpose |
|---|---|
| [CMake](https://cmake.org/download/) >= 3.15 | Build system |
| [Visual Studio 2022](https://visualstudio.microsoft.com/) (Desktop C++ workload) | MSVC compiler |
| [vcpkg](https://github.com/microsoft/vcpkg) | C++ dependency manager |
| [Ninja](https://github.com/ninja-build/ninja/releases) | Build tool used by vcpkg (install manually if vcpkg can't auto-download it) |
| `llama-server.exe` from [llama.cpp releases](https://github.com/ggml-org/llama.cpp/releases) | Inference engine |

---

## 2. One-time environment setup

```bat
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
bootstrap-vcpkg.bat

setx VCPKG_ROOT C:\vcpkg
```
Close and reopen your terminal so `VCPKG_ROOT` is picked up.

If `setup_dependencies.bat` fails trying to auto-download Ninja (SSL/proxy
errors are common in restricted networks), install it manually:
```bat
winget install Ninja-build.Ninja
```
then reopen the terminal and try again.

---

## 3. Install project dependencies

From the project root:

```bat
setup_dependencies.bat
```

Installs via vcpkg: `curl`, `glfw3`, `imgui` (glfw+opengl3 bindings) and
`nlohmann-json`. First run can take a while since it builds these from
source.

---

## 4. Get llama-server.exe

1. Go to https://github.com/ggml-org/llama.cpp/releases
2. Pick the latest release, then choose the asset matching your hardware:
   - CPU only, modern hardware: `llama-<build>-bin-win-avx2-x64.zip`
   - CPU only, very old hardware: `-avx-x64.zip`
   - NVIDIA GPU: `-cuda-cu12.x-x64.zip` (any cu12.x/cu13.x works if your
     driver supports it — check with `nvidia-smi`; drivers are backward
     compatible with older CUDA builds)
   - AMD GPU: `-hip-x64.zip`
3. Extract, and copy **`llama-server.exe`** into `bin\` in this project.
4. If it's a CUDA build, also copy the runtime DLLs (often a separate
   `cudart-llama-bin-win-cuXX-x64.zip`) into `bin\` — without them the
   server won't start.

---

## 5. Build

```bat
build.bat
```

Output: `build\Release\DFFastLLM.exe`.

To test locally before packaging the installer, copy `bin\llama-server.exe`
(and any DLLs) into `build\Release\bin\`.

---


## 6. Using the app

### Download a model
1. Open the **Models** tab.
2. (Optional, for private/gated models) Paste your **Hugging Face access
   token** and click **Save Token** — it's stored locally in `config.json`
   next to the executable and reused automatically every time you open the
   app.
   - Create a token at https://huggingface.co/settings/tokens
   - For gated models you also need to accept the license on the model's
     Hugging Face page, even with a valid token.
3. Type a repository name (e.g. `bartowski/Meta-Llama-3.1-8B-Instruct-GGUF`)
   and click **Fetch .gguf files**.
4. Pick a quantization (`Q4_K_M` is a solid size/quality tradeoff) and click
   **Download**. Progress bar included; cancel anytime.
5. The downloaded file shows up under **Downloaded models** — click **Use**
   to select it in the Chat tab.

> DFFastLLM only lists and downloads `.gguf` files, because that's the only
> format its inference engine (llama.cpp) can actually run. If a repo has no
> GGUF version, look for a community re-upload (users like `bartowski` or
> `unsloth` publish GGUF conversions of most popular models).

### Tune generation quality (fixing rambling/hallucination)
Go to the **Parameters** tab. If the model rambles, contradicts itself, or
drifts off-topic:
- Lower **Temperature** to `0.3–0.6`.
- Raise **Repeat penalty** to `1.1–1.3`.
- Lower **Top-P** to around `0.85–0.9`.

These are saved automatically and applied to every generation. The app also
sends requests through llama-server's chat-template-aware endpoint
(`--jinja`), which by itself fixes a lot of rambling caused by prompts not
matching the model's expected format.

### Chat
1. Go to the **Chat** tab, confirm the model path, click **Start Server**.
2. Type your prompt and click **Generate**.
3. If **Thinking Mode** is on and the model supports native reasoning, its
   reasoning streams live in a separate panel while the final answer streams
   in its own panel — never mixed together.
4. **Prompt Templates**: with "Auto-select best template" checked (default),
   DFFastLLM inspects your prompt and automatically applies a specialized
   system prompt if it recognizes the task (e.g. asking for C++ code applies
   the C++ template, asking to fix an error applies the debugging template).
   You can see which template was used under the Generate button, or force
   one manually via the dropdown.

### Conversations & context
DFFastLLM sends the entire conversation history to the model on every
message, so follow-up questions actually build on what was said before,
instead of the model starting fresh each time.

- Every exchange is auto-saved to the `conversations\` folder as you go —
  no manual "save" step needed.
- Click **New Conversation** (Chat tab or History tab) to start a fresh
  thread without losing the old one.
- The **History** tab lists every saved conversation by title and lets you
  **Load** or **Delete** any of them.

### Resizable columns & selectable text
Drag the thin handles between the Prompt, Thinking and Output panels to
resize them to your liking. All generated text (reasoning and answers) is
shown in read-only fields you can click-and-drag to select and copy with
Ctrl+C, like normal text — nothing is a static, uncopyable label.

### Code blocks
Whenever a response contains a fenced code block (` ```like this``` `),
DFFastLLM renders it in its own dark panel, separate from the surrounding
prose, with a one-click **Copy** button for just that snippet. Each message
also has a **Copy all** button next to it to grab the entire response at
once.

### The Agent tab
Beyond chatting, DFFastLLM can act as an autonomous coding agent that creates
and edits files by itself inside a folder you choose.

1. Go to the **Agent** tab. Pick a model, start its server, and (optionally)
   an mmproj file — same as the Chat tab, but with its own independent
   server so you can run a different model for agent work if you want.
2. Click **Choose Folder** and pick the project folder the agent is allowed
   to work in. **The agent can never touch anything outside this folder** —
   every path it proposes is resolved and checked against it before any
   file is written; anything that would escape the folder (via `..` or an
   absolute path) is blocked automatically.
3. Decide on autonomy:
   - **Off (default)**: the agent asks you to **Approve** or **Reject**
     every file it wants to create or edit, showing you the exact content
     first.
   - **On** ("let the agent work freely"): it proceeds without asking —
     the project folder sandbox is still enforced, but there's no
     per-file review step.
4. Type what you want built in the task box and click **Start Task**. The
   agent will plan, create/edit files, read back what it wrote, and keep
   going on its own turn after turn until it reports the task is done (or
   you click **Stop Agent**).

The agent's own conversation (its reasoning, if Thinking Mode is on, and
every action it took) is saved separately from your regular Chat
conversations and shown live in the activity log.

**Honest limitations:**
- The agent follows a simple, custom action protocol (a single JSON action
  per response) rather than any particular model's native "function
  calling" — this works with most instruction-tuned GGUF models, but
  works best with models that are good at following structured
  instructions (coding-tuned models tend to do noticeably better here than
  small general-purpose chat models).
- It edits/creates files as full-file rewrites, not diffs — simpler and
  more reliable for a local model to produce correctly, but it means large
  files get resent in full each time they're touched.
- There's no automatic rollback/undo built in yet — if you approve
  something you didn't mean to, you'll need to fix it manually (or restore
  from your own version control, which is strongly recommended for any
  folder you point the agent at).
Go to the **Prompt Templates** tab. In the "Add a new template" section:
1. **Name** — anything descriptive, e.g. "Rust Programming".
2. **Trigger keywords** — comma-separated words that, when found anywhere in
   your prompt, cause this template to be auto-selected. Example: `rust,
   cargo, .rs, tokio`. Matching is case-insensitive and substring-based.
3. **System prompt** — the actual instructions sent to the model whenever
   this template is picked.
4. Click **Add template**. It's saved immediately and shows up in the list
   below (tagged `custom` vs `built-in`) and in the manual override dropdown
   in the Chat tab.

You can add as many as you want, and **delete any template — including the
built-in ones** — from the same list (with a confirmation step). If you
delete a built-in by mistake, click **Restore deleted built-in templates**
at the top of the tab to bring all of them back at once. When more than one
template's keywords match a prompt, the one with the most matching keywords
wins.

---

## Technical notes

- **Thinking Mode depends on the model.** Reasoning is only shown separately
  when the model natively supports it (DeepSeek-R1, QwQ, Qwen3-thinking,
  etc.) and llama-server successfully parses it via `--reasoning-format
  auto`. Regular models (Llama, Mistral, Gemma, etc.) simply won't populate
  that panel — there's nothing to show.
- **GGUF only, on purpose.** llama.cpp's engine cannot execute raw
  `.safetensors` weights. This app does not offer a "download full
  repository" option for that reason — it would just leave you with files
  the app can't run. If you want to convert a model yourself, use llama.cpp's
  `convert_hf_to_gguf.py` script separately, then place the resulting
  `.gguf` in the `models\` folder.
- **Hugging Face token** is saved in plain text in `config.json` next to the
  executable. It is not encrypted — treat that file as sensitive.
- **GPU offload**: `LlamaServerManager.cpp` passes `-ngl 999` to push as many
  layers as possible onto the GPU. Ignored automatically on CPU-only builds.
- **No console window**: the executable is built with the Windows GUI
  subsystem (`WIN32` in CMake + `/ENTRY:mainCRTStartup`), and the
  `llama-server.exe` child process is started with `CREATE_NO_WINDOW`. If you
  still see a console appear, double check you rebuilt after pulling this
  version of `CMakeLists.txt`.
