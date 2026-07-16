#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <cmath>
#include <cstdint>

// Captura do microfone via waveIn (winmm): 16 kHz, mono, 16-bit PCM — o
// formato que o whisper.cpp espera. Mantem um buffer acumulado de amostras
// e expoe o nivel RMS atual (para a animacao de som e para a deteccao de
// fala/silencio do modo livre).
class AudioCapture {
public:
    static AudioCapture& Instance() {
        static AudioCapture inst;
        return inst;
    }

    static constexpr int kSampleRate = 16000;

    // Dispositivo de entrada: -1 = padrao do sistema (WAVE_MAPPER).
    // Vale a partir do proximo Start().
    void SetDeviceId(int id) { m_deviceId = id; }

    // Lista os dispositivos de captura disponiveis (nomes do Windows).
    static std::vector<std::string> ListInputDevices() {
        std::vector<std::string> out;
        UINT n = waveInGetNumDevs();
        for (UINT i = 0; i < n; i++) {
            WAVEINCAPSA caps{};
            if (waveInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
                out.push_back(caps.szPname);
        }
        return out;
    }

    bool Start() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_running) return true;
        m_samples.clear();

        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 1;
        fmt.nSamplesPerSec = kSampleRate;
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = fmt.nChannels * fmt.wBitsPerSample / 8;
        fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

        UINT dev = (m_deviceId >= 0 && m_deviceId < (int)waveInGetNumDevs())
                       ? (UINT)m_deviceId : WAVE_MAPPER;
        if (waveInOpen(&m_hWaveIn, dev, &fmt, (DWORD_PTR)WaveInProc,
                       (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
            m_hWaveIn = nullptr;
            return false; // sem microfone / sem permissao
        }

        // Buffers de ~100ms cada, revezando: enquanto um enche, o outro é
        // processado. Poucos e curtos = baixa latencia para o VAD.
        for (int i = 0; i < kNumBuffers; i++) {
            m_bufData[i].resize(kSampleRate / 10); // 100 ms
            WAVEHDR& h = m_headers[i];
            ZeroMemory(&h, sizeof(h));
            h.lpData = (LPSTR)m_bufData[i].data();
            h.dwBufferLength = (DWORD)(m_bufData[i].size() * sizeof(int16_t));
            waveInPrepareHeader(m_hWaveIn, &h, sizeof(h));
            waveInAddBuffer(m_hWaveIn, &h, sizeof(h));
        }
        m_running = true;
        waveInStart(m_hWaveIn);
        return true;
    }

    void Stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running) return;
        m_running = false; // o callback ignora buffers a partir daqui
        waveInStop(m_hWaveIn);
        waveInReset(m_hWaveIn);
        for (int i = 0; i < kNumBuffers; i++)
            waveInUnprepareHeader(m_hWaveIn, &m_headers[i], sizeof(WAVEHDR));
        waveInClose(m_hWaveIn);
        m_hWaveIn = nullptr;
        m_level = 0.0f;
    }

    bool IsRunning() const { return m_running; }

    // Nivel RMS (0..1) da ultima fatia de audio — para animacao e VAD.
    float Level() const { return m_level.load(); }

    // Copia e LIMPA as amostras acumuladas ate agora.
    std::vector<int16_t> Drain() {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        std::vector<int16_t> out = std::move(m_samples);
        m_samples.clear();
        return out;
    }

    // Quantidade de amostras acumuladas (sem consumir).
    size_t Pending() {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        return m_samples.size();
    }

    // Monta um arquivo WAV (16k mono 16-bit) em memoria a partir de amostras.
    static std::vector<uint8_t> MakeWav(const std::vector<int16_t>& samples) {
        std::vector<uint8_t> wav;
        uint32_t dataSize = (uint32_t)(samples.size() * 2);
        uint32_t riffSize = 36 + dataSize;
        auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) wav.push_back((v >> (8 * i)) & 0xFF); };
        auto put16 = [&](uint16_t v) { for (int i = 0; i < 2; i++) wav.push_back((v >> (8 * i)) & 0xFF); };
        wav.insert(wav.end(), {'R','I','F','F'}); put32(riffSize);
        wav.insert(wav.end(), {'W','A','V','E','f','m','t',' '});
        put32(16); put16(1); put16(1); put32(kSampleRate);
        put32(kSampleRate * 2); put16(2); put16(16);
        wav.insert(wav.end(), {'d','a','t','a'}); put32(dataSize);
        const uint8_t* p = (const uint8_t*)samples.data();
        wav.insert(wav.end(), p, p + dataSize);
        return wav;
    }

    // RMS (0..1) de um trecho de amostras — usado pelo VAD do modo livre.
    static float Rms(const int16_t* s, size_t n) {
        if (n == 0) return 0.0f;
        double acc = 0;
        for (size_t i = 0; i < n; i++) acc += (double)s[i] * (double)s[i];
        return (float)(sqrt(acc / n) / 32768.0);
    }

private:
    AudioCapture() = default;
    ~AudioCapture() { if (m_running) Stop(); }

    static void CALLBACK WaveInProc(HWAVEIN, UINT uMsg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR) {
        if (uMsg != WIM_DATA) return;
        auto* self = (AudioCapture*)inst;
        auto* hdr = (WAVEHDR*)p1;
        if (!self->m_running) return;
        size_t n = hdr->dwBytesRecorded / sizeof(int16_t);
        const int16_t* data = (const int16_t*)hdr->lpData;
        self->m_level = Rms(data, n);
        {
            std::lock_guard<std::mutex> lock(self->m_dataMutex);
            // Trava de seguranca: nunca acumula mais de 5 min de audio
            if (self->m_samples.size() < (size_t)kSampleRate * 300)
                self->m_samples.insert(self->m_samples.end(), data, data + n);
        }
        waveInAddBuffer(self->m_hWaveIn, hdr, sizeof(WAVEHDR)); // devolve o buffer
    }

    static constexpr int kNumBuffers = 4;
    HWAVEIN m_hWaveIn = nullptr;
    WAVEHDR m_headers[kNumBuffers]{};
    std::vector<int16_t> m_bufData[kNumBuffers];
    std::vector<int16_t> m_samples;
    std::mutex m_mutex, m_dataMutex;
    std::atomic<bool> m_running{false};
    std::atomic<float> m_level{0.0f};
    int m_deviceId = -1;
};
