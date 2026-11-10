#!/usr/bin/env python3
"""Boat and minecart textures for romfs, like Axeolon makes them at runtime (AxeolonTextures):
the oak boat recoloured to each wood's planks (every shade moved to the same place in the other
wood's range of shades). usage: gen_vehicles.py <textures dir> <romfs dir>"""
import os, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_assets import write_rgba

FILES = ["oak", "spruce", "birch", "drogwood", "thramwood",
         "charred_oak", "charred_spruce", "charred_birch", "charred_drogwood", "charred_thramwood"]
PLANK_TEX = [4, 198, 214, 174, 201, 184, 185, 186, 187, 203]

def lum(p):  # AxeolonTextures.lum: (r*299 + g*587 + b*114) / 1000
    r, g, b = p[0], p[1], p[2]
    return (r * 299 + g * 587 + b * 114) // 1000

def main(tex, romfs):
    terrain = Image.open(os.path.join(tex, 'terrain.png')).convert('RGBA')
    vs = terrain.width // 16
    def shades(t):
        tile = PLANK_TEX[t]
        px = [terrain.getpixel(((tile % 16) * vs + x, (tile // 16) * vs + y)) for x in range(vs) for y in range(vs)]
        return sorted(px, key=lum)  # stable, like Collections.sort
    oak = Image.open(os.path.join(tex, 'item', 'boat.png')).convert('RGBA')
    write_rgba(oak, os.path.join(romfs, 'textures/item/boat.rgba'))
    src = shades(0)
    lo, hi = lum(src[0]), lum(src[-1])
    for t in range(1, len(FILES)):
        to = shades(t)
        out = oak.copy()
        for x in range(oak.width):
            for y in range(oak.height):
                p = oak.getpixel((x, y))
                if p[3] == 0: continue
                f = (lum(p) - lo) / (hi - lo) if hi > lo else 0.5
                f = max(0.0, min(1.0, f))
                c = to[int(f * (len(to) - 1) + 0.5)]  # Math.round
                out.putpixel((x, y), (c[0], c[1], c[2], p[3]))
        write_rgba(out, os.path.join(romfs, 'axeolon/boat/%s.rgba' % FILES[t]))
    write_rgba(Image.open(os.path.join(tex, 'item', 'cart.png')), os.path.join(romfs, 'textures/item/cart.rgba'))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
