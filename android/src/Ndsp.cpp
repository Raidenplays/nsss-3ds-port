// The 3DS DSP's 24 sound channels as a software mixer on AAudio, so source/audio/Sound.cpp (effects,
// streamed music, records, positional panning) works unchanged: each channel plays its queue of
// PCM16 buffers at its own rate, with a left / right volume from its mix.
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <cmath>
#include <deque>
#include <mutex>
#include "3ds.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "axeolon", __VA_ARGS__)

namespace {

struct Channel {
    std::deque<ndspWaveBuf*> queue;
    double pos = 0;       // in the front buffer, in frames
    float rate = 32728.0f;
    bool stereo = false;
    float left = 1.0f, right = 1.0f;
};

std::mutex gMx;
Channel gCh[24];
AAudioStream* gStream = nullptr;
int gRate = 48000;

aaudio_data_callback_result_t mixCb(AAudioStream*, void*, void* data, int32_t frames) {
    float* out = static_cast<float*>(data);
    for (int32_t i = 0; i < frames * 2; ++i) out[i] = 0.0f;
    std::lock_guard<std::mutex> l(gMx);
    for (Channel& c : gCh) {
        double step = double(c.rate) / double(gRate);
        int32_t f = 0;
        while (f < frames && !c.queue.empty()) {
            ndspWaveBuf* b = c.queue.front();
            b->status = NDSP_WBUF_PLAYING;
            const s16* pcm = b->data_pcm16;
            u32 n = b->nsamples;
            while (f < frames) {
                u32 i = u32(c.pos);
                if (i >= n) break;
                float sl, sr;
                if (c.stereo) {
                    sl = pcm[i * 2] / 32768.0f;
                    sr = pcm[i * 2 + 1] / 32768.0f;
                } else {
                    sl = sr = pcm[i] / 32768.0f;
                }
                out[f * 2] += sl * c.left;
                out[f * 2 + 1] += sr * c.right;
                c.pos += step;
                ++f;
            }
            if (u32(c.pos) >= n) {
                c.pos -= n;
                if (b->looping) continue;
                b->status = NDSP_WBUF_DONE;
                c.queue.pop_front();
                if (c.queue.empty()) c.pos = 0;
            }
        }
    }
    for (int32_t i = 0; i < frames * 2; ++i) out[i] = out[i] > 1.0f ? 1.0f : out[i] < -1.0f ? -1.0f : out[i];
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void errorCb(AAudioStream*, void*, aaudio_result_t err) { LOG("audio stream error %d", int(err)); }

} // namespace

Result ndspInit() {
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return -1;
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(b, mixCb, nullptr);
    AAudioStreamBuilder_setErrorCallback(b, errorCb, nullptr);
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &gStream);
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK || !gStream) return -1;
    gRate = AAudioStream_getSampleRate(gStream);
    AAudioStream_requestStart(gStream);
    return 0;
}

void ndspExit() {
    if (gStream) {
        AAudioStream_requestStop(gStream);
        AAudioStream_close(gStream);
        gStream = nullptr;
    }
}

void ndspSetOutputMode(int) {}
void ndspChnSetInterp(int, int) {}

void ndspChnReset(int id) {
    std::lock_guard<std::mutex> l(gMx);
    Channel& c = gCh[id];
    for (ndspWaveBuf* b : c.queue) b->status = NDSP_WBUF_DONE;
    c = Channel();
}

void ndspChnSetRate(int id, float rate) {
    std::lock_guard<std::mutex> l(gMx);
    gCh[id].rate = rate;
}

void ndspChnSetFormat(int id, u16 format) {
    std::lock_guard<std::mutex> l(gMx);
    gCh[id].stereo = format == NDSP_FORMAT_STEREO_PCM16;
}

void ndspChnSetMix(int id, float mix[12]) {
    std::lock_guard<std::mutex> l(gMx);
    gCh[id].left = mix[0];
    gCh[id].right = mix[1];
}

void ndspChnWaveBufAdd(int id, ndspWaveBuf* buf) {
    std::lock_guard<std::mutex> l(gMx);
    buf->status = NDSP_WBUF_QUEUED;
    gCh[id].queue.push_back(buf);
}

void ndspChnWaveBufClear(int id) {
    std::lock_guard<std::mutex> l(gMx);
    for (ndspWaveBuf* b : gCh[id].queue) b->status = NDSP_WBUF_DONE;
    gCh[id].queue.clear();
    gCh[id].pos = 0;
}

bool ndspChnIsPlaying(int id) {
    std::lock_guard<std::mutex> l(gMx);
    return !gCh[id].queue.empty();
}
