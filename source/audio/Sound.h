#pragma once
// SoundManager + SoundPool on the 3DS DSP (ndsp).
//  - Effects: romfs .snd files (PCM made from the game's .ogg by tools/gen_sounds.py), named the
//    way SoundPool names them ("random/hurt1.ogg" -> "random.hurt", one of the numbered ones at
//    random). Positioned like paulscode's linear attenuation: silent 16 blocks away (further when
//    the volume is over 1), panned by where the listener faces.
//  - Music: Ogg Vorbis streamed and decoded on a thread (stb_vorbis); a random track after 10-20
//    minutes of quiet, like playRandomMusicIfReady.
// Without the DSP firmware (sdmc:/3ds/dspfirm.cdc) everything here quietly does nothing.
#include <string>

namespace sound {

bool init();
void shutdown();
bool available();

// GameSettings.soundVolume / musicVolume, 0..1
void setVolumes(float effects, float music);
float effectsVolume();
float musicVolume();

// SoundManager.func_338_a: the listener's ears (posY is the eyes), yaw in degrees like Java
void setListener(double x, double y, double z, float yawDeg);
// SoundManager.playSound (World.playSoundEffect / playSoundAtEntity)
void play(const std::string& name, double x, double y, double z, float volume, float pitch);
// SoundManager.func_337_a: not positioned (GUI clicks, toasts), a quarter as loud
void playUi(const std::string& name, float volume, float pitch);

// Once a game tick: the background music
void tick();
// SoundManager.func_331_a: a record from a jukebox at (x, y, z) ("13", "cat"); "" stops it
void playRecord(const std::string& name, double x, double y, double z);
// Once a frame: re-pan playing effects for where the listener is now
void update();

} // namespace sound
