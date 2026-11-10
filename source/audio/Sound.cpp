#include "Sound.h"
#include <3ds.h>
#include <dirent.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>
#include "world/JavaRandom.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

namespace sound {
namespace {

// ---------------------------------------------------------------- pools (SoundPool)
struct Clip {
    int16_t* data = nullptr; // linear memory, mono
    u32 samples = 0, rate = 0;
    bool tried = false;
};

std::unordered_map<std::string, std::vector<std::string>> effectPool; // key -> .snd paths
std::unordered_map<std::string, Clip> clips;                          // path -> PCM (loaded on first use)
std::vector<std::string> musicPool;                                   // every music/newmusic track
JavaRandom rng{0};

bool ready = false;
float fxVol = 1.0f, musVol = 1.0f;

// "random/hurt1.snd" -> "random.hurt" (SoundPool.addSound with field_1657_b)
std::string poolKey(std::string rel) {
    size_t dot = rel.find('.');
    if (dot != std::string::npos) rel.resize(dot);
    while (!rel.empty() && std::isdigit((unsigned char)rel.back())) rel.pop_back();
    for (char& c : rel)
        if (c == '/') c = '.';
    return rel;
}

void scan(const std::string& dir, const std::string& rel, const std::string& prefix, bool effects) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string path = dir + "/" + n, r = rel.empty() ? n : rel + "/" + n;
        if (e->d_type == DT_DIR) {
            if (effects && r == "music") continue; // Axeolon's boss music is streamed
            scan(path, r, prefix, effects);
        } else if (effects && n.size() > 4 && n.compare(n.size() - 4, 4, ".snd") == 0) {
            effectPool[prefix + poolKey(r)].push_back(path);
        } else if (!effects && n.size() > 4 && n.compare(n.size() - 4, 4, ".ogg") == 0) {
            musicPool.push_back(path);
        }
    }
    closedir(d);
}

const Clip* clip(const std::string& path) {
    Clip& c = clips[path];
    if (c.tried) return c.data ? &c : nullptr;
    c.tried = true;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    char magic[4];
    u32 hdr[2];
    if (fread(magic, 1, 4, f) == 4 && std::memcmp(magic, "AXSN", 4) == 0 && fread(hdr, 4, 2, f) == 2 && hdr[1] > 0) {
        c.rate = hdr[0];
        c.samples = hdr[1];
        c.data = (int16_t*)linearAlloc(size_t(c.samples) * 2);
        if (c.data && fread(c.data, 2, c.samples, f) == c.samples) {
            DSP_FlushDataCache(c.data, c.samples * 2);
        } else if (c.data) {
            linearFree(c.data);
            c.data = nullptr;
        }
    }
    fclose(f);
    return c.data ? &c : nullptr;
}

// ---------------------------------------------------------------- effect voices
constexpr int FX_CHANNELS = 20; // 0..19; 21 a jukebox's record, 22 and 23 stream music
struct Voice {
    bool positional = false;
    double x = 0, y = 0, z = 0;
    float gain = 0, range = 16;
    u64 started = 0;
    ndspWaveBuf wb{};
};
Voice voices[FX_CHANNELS];
int nextVoice = 0;
double lx = 0, ly = 0, lz = 0;
float lyaw = 0;

void mixFor(const Voice& v, float out[12]) {
    std::memset(out, 0, sizeof(float) * 12);
    float g = v.gain * fxVol, pan = 0.0f;
    if (v.positional) {
        double dx = v.x - lx, dy = v.y - ly, dz = v.z - lz;
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        float att = 1.0f - float(dist) / v.range; // paulscode ATTENUATION_LINEAR
        if (att < 0.0f) att = 0.0f;
        g *= att;
        if (dist > 0.01) {
            float yr = lyaw * 3.1415927f / 180.0f;
            double rx = -std::cos(yr), rz = -std::sin(yr); // the listener's right
            pan = float((dx * rx + dz * rz) / dist) * 0.85f;
        }
    }
    out[0] = g * std::sqrt(0.5f * (1.0f - pan));
    out[1] = g * std::sqrt(0.5f * (1.0f + pan));
}

void startVoice(const std::string& name, bool positional, double x, double y, double z, float volume, float pitch,
                float range) {
    auto it = effectPool.find(name);
    if (it == effectPool.end() || it->second.empty()) return;
    const std::string& path = it->second[rng.nextInt(int(it->second.size()))];
    const Clip* c = clip(path);
    if (!c) return;
    // a free channel, or else the one that started longest ago
    int ch = -1;
    for (int i = 0; i < FX_CHANNELS; ++i) {
        int k = (nextVoice + i) % FX_CHANNELS;
        if (!ndspChnIsPlaying(k)) { ch = k; break; }
    }
    if (ch < 0) {
        ch = 0;
        for (int i = 1; i < FX_CHANNELS; ++i)
            if (voices[i].started < voices[ch].started) ch = i;
    }
    nextVoice = (ch + 1) % FX_CHANNELS;
    Voice& v = voices[ch];
    ndspChnWaveBufClear(ch);
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    if (pitch < 0.5f) pitch = 0.5f; // OpenAL's pitch range
    if (pitch > 2.0f) pitch = 2.0f;
    ndspChnSetRate(ch, float(c->rate) * pitch);
    ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
    v.positional = positional;
    v.x = x;
    v.y = y;
    v.z = z;
    v.range = range;
    v.gain = volume > 1.0f ? 1.0f : volume;
    v.started = svcGetSystemTick();
    float mix[12];
    mixFor(v, mix);
    ndspChnSetMix(ch, mix);
    std::memset(&v.wb, 0, sizeof v.wb);
    v.wb.data_vaddr = c->data;
    v.wb.nsamples = c->samples;
    ndspChnWaveBufAdd(ch, &v.wb);
}

// ---------------------------------------------------------------- music streams
constexpr int FRAMES = 4096, BUFS = 4;
struct Track {
    int ch = 0;
    stb_vorbis* v = nullptr;
    int channels = 2;
    bool loop = false, ended = false;
    float vol = 0;                     // fade, 0..1
    int16_t* pcm[BUFS] = {};
    ndspWaveBuf wb[BUFS]{};
};
Track tracks[2];
Track record;                      // SoundManager's "streaming" source: a jukebox's record
double recX = 0, recY = 0, recZ = 0;
LightLock musicLock;
Thread musicThread = nullptr;
volatile bool musicRun = false;

void closeTrack(Track& t) { // with musicLock held
    ndspChnWaveBufClear(t.ch);
    if (t.v) stb_vorbis_close(t.v);
    t.v = nullptr;
    t.ended = false;
}

bool openTrack(Track& t, const std::string& path, bool loop, float vol) { // with musicLock held
    closeTrack(t);
    int err = 0;
    t.v = stb_vorbis_open_filename(path.c_str(), &err, nullptr);
    if (!t.v) return false;
    stb_vorbis_info in = stb_vorbis_get_info(t.v);
    t.channels = in.channels >= 2 ? 2 : 1;
    t.loop = loop;
    t.ended = false;
    t.vol = vol;
    ndspChnReset(t.ch);
    ndspChnSetInterp(t.ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(t.ch, float(in.sample_rate));
    ndspChnSetFormat(t.ch, t.channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {musVol * vol, musVol * vol};
    ndspChnSetMix(t.ch, mix);
    for (auto& w : t.wb) {
        std::memset(&w, 0, sizeof w);
        w.status = NDSP_WBUF_DONE; // free for the thread to fill
    }
    return true;
}

bool trackPlaying(const Track& t) { return t.v && !(t.ended && !ndspChnIsPlaying(t.ch)); }

void fill(Track& t) {
    if (!t.v || t.ended) return;
    for (int i = 0; i < BUFS; ++i) {
        ndspWaveBuf& w = t.wb[i];
        if (w.status != NDSP_WBUF_DONE && w.status != NDSP_WBUF_FREE) continue;
        int n = stb_vorbis_get_samples_short_interleaved(t.v, t.channels, t.pcm[i], FRAMES * t.channels);
        if (n <= 0 && t.loop) {
            stb_vorbis_seek_start(t.v);
            n = stb_vorbis_get_samples_short_interleaved(t.v, t.channels, t.pcm[i], FRAMES * t.channels);
        }
        if (n <= 0) {
            t.ended = true;
            return;
        }
        std::memset(&w, 0, sizeof w);
        w.data_vaddr = t.pcm[i];
        w.nsamples = u32(n);
        DSP_FlushDataCache(t.pcm[i], size_t(n) * t.channels * 2);
        ndspChnWaveBufAdd(t.ch, &w);
    }
}

void musicMain(void*) {
    while (musicRun) {
        LightLock_Lock(&musicLock);
        fill(tracks[0]);
        fill(tracks[1]);
        fill(record);
        LightLock_Unlock(&musicLock);
        svcSleepThread(15 * 1000000LL);
    }
}

void setTrackVolume(Track& t) {
    float mix[12] = {musVol * t.vol, musVol * t.vol};
    if (t.channels == 1) mix[1] = mix[0];
    ndspChnSetMix(t.ch, mix);
}

// SoundManager's music state
int ticksBeforeMusic = 0;
std::string forcedName, forcedIntro, forcedLoop;
bool forcedInLoop = false, fadingOutAll = false;
int cur = 0, old = -1;
int notPlayingTicks = 0;
u64 forcedStarted = 0;
constexpr int FADE_TICKS = 40;

std::string musicPath(const std::string& name, const char* part) {
    std::string p = "romfs:/axeolon/sound/music/" + name + part;
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return "";
    fclose(f);
    return p;
}

void startForcedTrack(const std::string& path, bool loop) { // with musicLock held
    openTrack(tracks[cur], path, loop, tracks[cur].vol);
    forcedStarted = osGetTime();
    notPlayingTicks = 0;
}

} // namespace

bool init() {
    if (ready) return true;
    if (R_FAILED(ndspInit())) return false;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    rng.setSeed(int64_t(osGetTime()));
    ticksBeforeMusic = rng.nextInt(12000);
    scan("romfs:/sound/newsound", "", "", true);
    scan("romfs:/sound/sound", "", "", true);
    scan("romfs:/axeolon/sound", "", "axeolon.", true);
    scan("romfs:/sound/music", "", "", false);
    scan("romfs:/sound/newmusic", "", "", false);
    LightLock_Init(&musicLock);
    for (int k = 0; k < 2; ++k) {
        tracks[k].ch = 22 + k;
        for (int i = 0; i < BUFS; ++i) tracks[k].pcm[i] = (int16_t*)linearAlloc(FRAMES * 2 * 2);
    }
    record.ch = 21;
    for (int i = 0; i < BUFS; ++i) record.pcm[i] = (int16_t*)linearAlloc(FRAMES * 2 * 2);
    musicRun = true;
    // Decoding goes on the second core when the system lends it (TerrainWorker shares it, lower)
    musicThread = threadCreate(musicMain, nullptr, 64 * 1024, 0x18, 1, false);
    if (!musicThread) musicThread = threadCreate(musicMain, nullptr, 64 * 1024, 0x2F, -2, false);
    ready = true;
    return true;
}

void shutdown() {
    if (!ready) return;
    musicRun = false;
    if (musicThread) {
        threadJoin(musicThread, U64_MAX);
        threadFree(musicThread);
        musicThread = nullptr;
    }
    for (Track* t : {&tracks[0], &tracks[1], &record}) {
        closeTrack(*t);
        for (auto& p : t->pcm) {
            if (p) linearFree(p);
            p = nullptr;
        }
    }
    for (int i = 0; i < 24; ++i) ndspChnWaveBufClear(i);
    for (auto& kv : clips)
        if (kv.second.data) linearFree(kv.second.data);
    clips.clear();
    ndspExit();
    ready = false;
}

bool available() { return ready; }

void setVolumes(float effects, float music) {
    fxVol = effects;
    musVol = music;
    if (!ready) return;
    LightLock_Lock(&musicLock);
    if (musVol <= 0.0f && forcedName.empty()) closeTrack(tracks[cur]); // onSoundOptionsChanged: stop BgMusic
    for (Track& t : tracks)
        if (t.v) setTrackVolume(t);
    LightLock_Unlock(&musicLock);
}
float effectsVolume() { return fxVol; }
float musicVolume() { return musVol; }

void setListener(double x, double y, double z, float yawDeg) {
    lx = x;
    ly = y;
    lz = z;
    lyaw = yawDeg;
}

void play(const std::string& name, double x, double y, double z, float volume, float pitch) {
    if (!ready || fxVol <= 0.0f || volume <= 0.0f) return;
    float range = 16.0f;
    if (volume > 1.0f) range *= volume;
    startVoice(name, true, x, y, z, volume, pitch, range);
}

void playUi(const std::string& name, float volume, float pitch) {
    if (!ready || fxVol <= 0.0f) return;
    if (volume > 1.0f) volume = 1.0f;
    startVoice(name, false, 0, 0, 0, volume * 0.25f, pitch, 16.0f);
}

void update() {
    if (!ready) return;
    if (record.v) { // the record is heard from the jukebox: half volume, fading out over 64 blocks
        Voice v;
        v.positional = true;
        v.x = recX;
        v.y = recY;
        v.z = recZ;
        v.gain = 0.5f;
        v.range = 64.0f;
        float mix[12];
        LightLock_Lock(&musicLock);
        mixFor(v, mix);
        if (record.channels == 1) mix[0] = mix[1] = (mix[0] + mix[1]) * 0.5f;
        ndspChnSetMix(record.ch, mix);
        LightLock_Unlock(&musicLock);
    }
    for (int i = 0; i < FX_CHANNELS; ++i)
        if (voices[i].positional && ndspChnIsPlaying(i)) {
            float mix[12];
            mixFor(voices[i], mix);
            ndspChnSetMix(i, mix);
        }
}

void playMusic(const std::string& name) {
    if (!ready) return;
    if (name == forcedName && !fadingOutAll) return;
    std::string intro = musicPath(name, "_intro.ogg"), loop = musicPath(name, "_loop.ogg");
    if (intro.empty() && loop.empty()) return;
    LightLock_Lock(&musicLock);
    bool crossfade = !forcedName.empty() && tracks[cur].v;
    forcedName = name;
    forcedIntro = intro;
    forcedLoop = loop;
    forcedInLoop = intro.empty();
    fadingOutAll = false;
    if (crossfade) {
        if (old >= 0) closeTrack(tracks[old]);
        old = cur;
        cur ^= 1;
        tracks[cur].vol = 0.0f;
    } else {
        closeTrack(tracks[0]);
        closeTrack(tracks[1]);
        old = -1;
        tracks[cur].vol = 1.0f;
    }
    startForcedTrack(intro.empty() ? loop : intro, intro.empty());
    LightLock_Unlock(&musicLock);
}

void playRecord(const std::string& name, double x, double y, double z) {
    if (!ready) return;
    LightLock_Lock(&musicLock);
    closeTrack(record); // whatever was on stops
    if (!name.empty() && fxVol > 0.0f) {
        std::string path = "romfs:/sound/streaming/" + name + ".ogg";
        if (FILE* f = fopen(path.c_str(), "rb")) {
            fclose(f);
            if (forcedName.empty()) { // the background music makes way
                closeTrack(tracks[0]);
                closeTrack(tracks[1]);
            }
            recX = x;
            recY = y;
            recZ = z;
            openTrack(record, path, false, 1.0f);
            float silent[12] = {0};
            ndspChnSetMix(record.ch, silent); // update() sets the real mix
        }
    }
    LightLock_Unlock(&musicLock);
}

void stopMusic() {
    if (forcedName.empty()) return;
    fadingOutAll = true;
}

void tick() {
    if (!ready) return;
    LightLock_Lock(&musicLock);
    if (!forcedName.empty()) { // tickForcedMusic
        float step = 1.0f / FADE_TICKS;
        if (old >= 0) {
            tracks[old].vol -= step;
            if (tracks[old].vol <= 0.0f) {
                closeTrack(tracks[old]);
                old = -1;
            } else {
                setTrackVolume(tracks[old]);
            }
        }
        if (fadingOutAll) {
            tracks[cur].vol -= step;
            if (tracks[cur].vol <= 0.0f) {
                closeTrack(tracks[0]);
                closeTrack(tracks[1]);
                old = -1;
                forcedName.clear();
                fadingOutAll = false;
                LightLock_Unlock(&musicLock);
                return;
            }
        } else if (tracks[cur].vol < 1.0f) {
            tracks[cur].vol = std::fmin(1.0f, tracks[cur].vol + step);
        }
        if (tracks[cur].v) {
            setTrackVolume(tracks[cur]);
            if (!forcedInLoop && !forcedLoop.empty() && osGetTime() - forcedStarted > 1000) {
                notPlayingTicks = trackPlaying(tracks[cur]) ? 0 : notPlayingTicks + 1;
                if (notPlayingTicks >= 2) {
                    forcedInLoop = true;
                    startForcedTrack(forcedLoop, true);
                }
            }
        }
    } else if (musVol > 0.0f && !musicPool.empty()) { // playRandomMusicIfReady
        if (!trackPlaying(tracks[0]) && !trackPlaying(tracks[1]) && !trackPlaying(record)) {
            if (ticksBeforeMusic > 0) {
                --ticksBeforeMusic;
            } else {
                ticksBeforeMusic = rng.nextInt(12000) + 12000;
                const std::string& p = musicPool[rng.nextInt(int(musicPool.size()))];
                closeTrack(tracks[0]);
                closeTrack(tracks[1]);
                cur = 0;
                old = -1;
                openTrack(tracks[0], p, false, 1.0f);
            }
        }
    }
    LightLock_Unlock(&musicLock);
}

} // namespace sound
