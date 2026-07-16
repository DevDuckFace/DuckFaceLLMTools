#pragma once
#include <windows.h>
#include <sapi.h>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

// Sintese de voz via SAPI do Windows — offline, sem dependencias, usa as
// vozes instaladas no sistema (em Windows pt-BR ha vozes em portugues).
// Speak() e assincrono; Stop() interrompe imediatamente (usado quando o
// usuario "corta" a resposta falando por cima, no modo livre).
class Tts {
public:
    static Tts& Instance() {
        static Tts inst;
        return inst;
    }

    bool Speak(const std::string& utf8Text) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!EnsureVoice()) return false;
        ApplyPendingDevice();
        ApplyPendingVoice();

        int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, nullptr, 0);
        std::wstring wtext(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8Text.c_str(), -1, wtext.data(), wlen);

        // PURGEBEFORESPEAK: fala nova sempre substitui a anterior
        HRESULT hr = m_voice->Speak(wtext.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
        return SUCCEEDED(hr);
    }

    void Stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_voice) m_voice->Speak(L"", SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
    }

    // Enumera tokens de uma categoria SAPI e devolve as descricoes.
    static std::vector<std::string> EnumCategory(const wchar_t* categoryId) {
        std::vector<std::string> out;
        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ISpObjectTokenCategory* cat = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                        IID_ISpObjectTokenCategory, (void**)&cat))) {
            if (SUCCEEDED(cat->SetId(categoryId, FALSE))) {
                IEnumSpObjectTokens* en = nullptr;
                if (SUCCEEDED(cat->EnumTokens(nullptr, nullptr, &en))) {
                    ISpObjectToken* tok = nullptr;
                    while (en->Next(1, &tok, nullptr) == S_OK) {
                        LPWSTR desc = nullptr;
                        if (SUCCEEDED(tok->GetStringValue(nullptr, &desc)) && desc) {
                            int len = WideCharToMultiByte(CP_UTF8, 0, desc, -1, nullptr, 0, nullptr, nullptr);
                            std::string s(len > 0 ? len - 1 : 0, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, desc, -1, s.data(), len, nullptr, nullptr);
                            out.push_back(s);
                            CoTaskMemFree(desc);
                        }
                        tok->Release();
                    }
                    en->Release();
                }
            }
            cat->Release();
        }
        if (hrInit == S_OK) CoUninitialize();
        return out;
    }

    // Vozes instaladas: as SAPI5 classicas E as "OneCore" (as vozes
    // modernas do Windows 10/11, geralmente bem mais naturais, como a
    // Maria/Daniel em pt-BR). As duas listas sao concatenadas: primeiro
    // SAPI5, depois OneCore — a mesma ordem usada por SetVoiceIndex.
    static std::vector<std::string> ListVoices() {
        auto v = EnumCategory(SPCAT_VOICES);
        auto oc = EnumCategory(L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices");
        v.insert(v.end(), oc.begin(), oc.end());
        return v;
    }

    // Escolhe a voz pelo indice da lista acima (-1 = padrao). Assim como
    // o dispositivo, so ANOTA — a aplicacao acontece na thread do Speak.
    void SetVoiceIndex(int index) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingVoice = index;
        m_voiceDirty = true;
    }

    // Lista os dispositivos de SAIDA de audio conhecidos pela SAPI.
    static std::vector<std::string> ListOutputDevices() {
        std::vector<std::string> out;
        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ISpObjectTokenCategory* cat = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                        IID_ISpObjectTokenCategory, (void**)&cat))) {
            if (SUCCEEDED(cat->SetId(SPCAT_AUDIOOUT, FALSE))) {
                IEnumSpObjectTokens* en = nullptr;
                if (SUCCEEDED(cat->EnumTokens(nullptr, nullptr, &en))) {
                    ISpObjectToken* tok = nullptr;
                    while (en->Next(1, &tok, nullptr) == S_OK) {
                        LPWSTR desc = nullptr;
                        // A descricao do token e o valor-string padrao dele
                        if (SUCCEEDED(tok->GetStringValue(nullptr, &desc)) && desc) {
                            int len = WideCharToMultiByte(CP_UTF8, 0, desc, -1, nullptr, 0, nullptr, nullptr);
                            std::string s(len > 0 ? len - 1 : 0, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, desc, -1, s.data(), len, nullptr, nullptr);
                            out.push_back(s);
                            CoTaskMemFree(desc);
                        }
                        tok->Release();
                    }
                    en->Release();
                }
            }
            cat->Release();
        }
        if (hrInit == S_OK) CoUninitialize();
        return out;
    }

    // Escolhe o dispositivo de saida (-1 = padrao do sistema).
    // IMPORTANTE: aqui so ANOTAMOS a escolha. Criar/usar o ISpVoice na
    // thread da interface e depois falar por outra thread quebra o COM
    // (chamada cross-apartment sem marshaling) — era a causa do erro
    // "sintese de voz falhou". A aplicacao real acontece dentro do
    // Speak(), na MESMA thread que fala.
    void SetOutputDevice(int index) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingDevice = index;
        m_deviceDirty = true;
    }

    bool IsSpeaking() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_voice) return false;
        SPVOICESTATUS status{};
        if (FAILED(m_voice->GetStatus(&status, nullptr))) return false;
        return status.dwRunningState == SPRS_IS_SPEAKING;
    }

private:
    Tts() = default;
    ~Tts() {
        if (m_voice) { m_voice->Release(); m_voice = nullptr; }
        if (m_comInit) CoUninitialize();
    }

    bool EnsureVoice() {
        if (m_voice) return true;
        // COM inicializado de forma preguicosa na primeira fala. Se a
        // thread ja tiver COM (S_FALSE/RPC_E_CHANGED_MODE), seguimos.
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        m_comInit = (hr == S_OK);
        hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL,
                              IID_ISpVoice, (void**)&m_voice);
        return SUCCEEDED(hr) && m_voice;
    }

    // Aplica a escolha de dispositivo pendente. Chamar SOMENTE com o
    // mutex tomado e na thread que usa o m_voice (a thread do Speak).
    void ApplyPendingDevice() {
        if (!m_deviceDirty || !m_voice) return;
        m_deviceDirty = false;
        if (m_pendingDevice < 0) { m_voice->SetOutput(nullptr, TRUE); return; }
        ISpObjectTokenCategory* cat = nullptr;
        if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                     IID_ISpObjectTokenCategory, (void**)&cat))) return;
        if (SUCCEEDED(cat->SetId(SPCAT_AUDIOOUT, FALSE))) {
            IEnumSpObjectTokens* en = nullptr;
            if (SUCCEEDED(cat->EnumTokens(nullptr, nullptr, &en))) {
                ISpObjectToken* tok = nullptr;
                int i = 0;
                while (en->Next(1, &tok, nullptr) == S_OK) {
                    if (i == m_pendingDevice) { m_voice->SetOutput(tok, TRUE); tok->Release(); break; }
                    tok->Release();
                    i++;
                }
                en->Release();
            }
        }
        cat->Release();
    }

    void ApplyPendingVoice() {
        if (!m_voiceDirty || !m_voice) return;
        m_voiceDirty = false;
        if (m_pendingVoice < 0) { m_voice->SetVoice(nullptr); return; }
        // percorre SAPI5 e depois OneCore, na mesma ordem de ListVoices()
        const wchar_t* cats[] = { SPCAT_VOICES,
            L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices" };
        int i = 0;
        for (const wchar_t* catId : cats) {
            ISpObjectTokenCategory* cat = nullptr;
            if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                         IID_ISpObjectTokenCategory, (void**)&cat))) continue;
            if (SUCCEEDED(cat->SetId(catId, FALSE))) {
                IEnumSpObjectTokens* en = nullptr;
                if (SUCCEEDED(cat->EnumTokens(nullptr, nullptr, &en))) {
                    ISpObjectToken* tok = nullptr;
                    while (en->Next(1, &tok, nullptr) == S_OK) {
                        if (i == m_pendingVoice) {
                            m_voice->SetVoice(tok);
                            tok->Release();
                            en->Release();
                            cat->Release();
                            return;
                        }
                        tok->Release();
                        i++;
                    }
                    en->Release();
                }
            }
            cat->Release();
        }
    }

    ISpVoice* m_voice = nullptr;
    bool m_comInit = false;
    int m_pendingDevice = -1;
    bool m_deviceDirty = false;
    int m_pendingVoice = -1;
    bool m_voiceDirty = false;
    std::mutex m_mutex;
};
