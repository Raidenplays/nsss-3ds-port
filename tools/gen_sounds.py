#!/usr/bin/env python3
# Sound effects -> raw PCM the 3DS DSP plays without decoding: "AXSN", u32 sample rate,
# u32 sample count, then mono signed 16-bit samples. Music and records stay Ogg Vorbis
# (streamed and decoded with stb_vorbis on the second core).
# usage: gen_sounds.py <romfs dir>   (converts every effect .ogg in place: x.ogg -> x.snd)
import os, struct, subprocess, sys

RATE = 32000
EFFECT_DIRS = ['sound/newsound', 'sound/sound']

def main():
    romfs = sys.argv[1]
    n = 0
    for d in EFFECT_DIRS:
        for dp, _, files in os.walk(os.path.join(romfs, d)):
            for fn in sorted(files):
                if not fn.endswith('.ogg'): continue
                src = os.path.join(dp, fn)
                pcm = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', src, '-ac', '1', '-ar', str(RATE),
                                               '-f', 's16le', '-'])
                with open(src[:-4] + '.snd', 'wb') as f:
                    f.write(b'AXSN' + struct.pack('<II', RATE, len(pcm) // 2))
                    f.write(pcm)
                os.remove(src)
                n += 1
    print('sound effects converted: %d' % n)

if __name__ == '__main__':
    main()
