#!/usr/bin/env python3
"""Converts Axeolon's Blockbench models to the 3DS model format (.axm).

Reads every /axeolon/mob/*.bbmodel and /axeolon/block/*.bbmodel exactly the way the Java mod does
(AxeolonBBModel for mobs and for every animation, AxeolonBlockRig for block geometry), and writes
the same data in a small binary form the 3DS loads without a JSON parser:

  "AXM1"  u8 kind (0 mob/Box UV, 1 block rig/per-face UV)  u16 texW  u16 texH
  u16 bones:   str name, i16 parent, f32 origin[3], f32 rot[3]          (Blockbench values)
               u16 cubes: f32 from[3], to[3], origin[3], rot[3], f32 inflate, f32 uvOffset[2],
                          u8 flags (1 mirror_uv, 2 visible), str name,
                          u8 faceMask, per present face: f32 uv[4], u8 rot  (north east south west up down)
  u16 loose cubes (outliner entries that are not in a group), same cube record
  u16 anims:   str name, f32 length, u8 loop (0 once, 1 loop, 2 hold)
               u16 tracks: str bone, u8 channel (0 rot, 1 pos, 2 scale),
                           u16 keys: f32 time, f32 x, y, z, u8 interp (0 linear, 1 smooth, 2 step)
  strings are u8 length + UTF-8 bytes. Little-endian.

Keyframe values are stored as AxeolonBBModel holds them: Blockbench 5 files have X/Y rotation and X
position flipped back to the Blockbench 4 convention, Molang expressions read as the default.

usage: bbmodel_convert.py <minecraft/src/axeolon> <romfs dir>
"""
import json, os, struct, sys, glob

DIRS = ["north", "east", "south", "west", "up", "down"]


def num(o, d):
    # AxeolonJson.num: numbers, numeric strings; anything else is the default
    if isinstance(o, bool) or o is None:
        return d
    if isinstance(o, (int, float)):
        return float(o)
    if isinstance(o, str):
        try:
            return float(o.strip())
        except ValueError:
            return d
    return d


def vec3(o):
    v = [0.0, 0.0, 0.0]
    if isinstance(o, list):
        for k in range(min(3, len(o))):
            v[k] = num(o[k], 0.0)
    return v


class W:
    def __init__(self):
        self.b = bytearray()

    def u8(self, v): self.b += struct.pack('<B', v)
    def i16(self, v): self.b += struct.pack('<h', v)
    def u16(self, v): self.b += struct.pack('<H', v)
    def f32(self, *vs):
        for v in vs:
            self.b += struct.pack('<f', v)
    def s(self, t):
        e = (t or '').encode('utf-8')[:255]
        self.u8(len(e)); self.b += e


def cube_record(w, e):
    w.f32(*vec3(e.get('from'))); w.f32(*vec3(e.get('to')))
    w.f32(*vec3(e.get('origin'))); w.f32(*vec3(e.get('rotation')))
    w.f32(num(e.get('inflate'), 0.0))
    uvo = e.get('uv_offset')
    if isinstance(uvo, list):
        w.f32(num(uvo[0], 0.0), num(uvo[1], 0.0))
    else:
        w.f32(0.0, 0.0)
    flags = (1 if e.get('mirror_uv') is True else 0) | (0 if e.get('visibility') is False else 2)
    w.u8(flags)
    w.s(e.get('name'))
    # per-face UVs, as AxeolonBlockRig reads them
    faces = []
    fs = e.get('faces') or {}
    mask = 0
    for d in range(6):
        f = fs.get(DIRS[d]) if isinstance(fs, dict) else None
        if f is None or f.get('texture') is None or f.get('texture') is False:
            continue
        uv = f.get('uv')
        if not isinstance(uv, list) or len(uv) < 4:
            continue
        mask |= 1 << d
        faces.append(([num(uv[i], 0.0) for i in range(4)], (int(num(f.get('rotation'), 0.0)) // 90) & 3))
    w.u8(mask)
    for uv, rot in faces:
        w.f32(*uv); w.u8(rot)


def convert(path, kind):
    root = json.load(open(path, encoding='utf-8'))
    meta = root.get('meta') or {}
    bb5 = False
    fv = meta.get('format_version')
    if fv is not None:
        v = str(fv)
        try:
            bb5 = int(v.split('.')[0]) >= 5
        except ValueError:
            bb5 = False
    res = root.get('resolution') or {}
    tw = int(num(res.get('width'), 64.0 if kind == 0 else 16.0))
    th = int(num(res.get('height'), 32.0 if kind == 0 else 16.0))

    cubes = {}
    for e in root.get('elements') or []:
        if e.get('type') is None or e.get('type') == 'cube':
            cubes[e.get('uuid')] = e

    bones = []  # (name, parent, origin, rot, [cube elements])

    def group(g, parent):
        idx = len(bones)
        entry = [g.get('name'), parent, vec3(g.get('origin')), vec3(g.get('rotation')), []]
        bones.append(entry)
        for k in g.get('children') or []:
            if isinstance(k, dict):
                group(k, idx)
            elif k in cubes:
                entry[4].append(cubes[k])

    loose = []
    for o in root.get('outliner') or []:
        if isinstance(o, dict):
            group(o, -1)
        elif o in cubes:
            loose.append(cubes[o])

    w = W()
    w.b += b'AXM1'
    w.u8(kind); w.u16(tw); w.u16(th)
    w.u16(len(bones))
    for name, parent, origin, rot, cs in bones:
        w.s(name); w.i16(parent); w.f32(*origin); w.f32(*rot)
        w.u16(len(cs))
        for e in cs:
            cube_record(w, e)
    w.u16(len(loose))
    for e in loose:
        cube_record(w, e)

    anims = root.get('animations') or []
    # Java keeps animations in a HashMap by name: a later one with the same name wins
    byname = {}
    for a in anims:
        byname[a.get('name')] = a
    w.u16(len(byname))
    for name, a in byname.items():
        w.s(name)
        w.f32(num(a.get('length'), 1.0))
        lp = a.get('loop')
        w.u8(1 if lp == 'loop' else (2 if lp == 'hold' else 0))
        tracks = {}
        # AxeolonBBModel: a later animator for the same bone replaces the earlier channel
        for an in (a.get('animators') or {}).values():
            bone = an.get('name')
            kfs = an.get('keyframes')
            if bone is None or kfs is None:
                continue
            for ci, ch in enumerate(('rotation', 'position', 'scale')):
                sel = [k for k in kfs if k.get('channel') == ch]
                if not sel:
                    continue
                sel = sorted(sel, key=lambda k: num(k.get('time'), 0.0))  # stable, like Java's insertion sort
                d = 1.0 if ch == 'scale' else 0.0
                keys = []
                for k in sel:
                    dps = k.get('data_points')
                    dp = dps[0] if isinstance(dps, list) and dps else {}
                    x, y, z = num(dp.get('x'), d), num(dp.get('y'), d), num(dp.get('z'), d)
                    if bb5 and ch == 'rotation':
                        x, y = -x, -y
                    if bb5 and ch == 'position':
                        x = -x
                    ip = k.get('interpolation')
                    keys.append((num(k.get('time'), 0.0), x, y, z, 1 if ip == 'catmullrom' else (2 if ip == 'step' else 0)))
                tracks[(bone, ci)] = keys
        w.u16(len(tracks))
        for (bone, ci), keys in tracks.items():
            w.s(bone); w.u8(ci); w.u16(len(keys))
            for t, x, y, z, ip in keys:
                w.f32(t, x, y, z); w.u8(ip)
    return bytes(w.b)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    axe, romfs = sys.argv[1], sys.argv[2]
    n = 0
    for sub, kind in (('mob', 0), ('block', 1)):
        out = os.path.join(romfs, 'axeolon', sub)
        os.makedirs(out, exist_ok=True)
        for f in sorted(glob.glob(os.path.join(axe, sub, '*.bbmodel'))):
            data = convert(f, kind)
            name = os.path.splitext(os.path.basename(f))[0]
            open(os.path.join(out, name + '.axm'), 'wb').write(data)
            n += 1
    print('converted', n, 'models')
    return 0


if __name__ == '__main__':
    sys.exit(main())
