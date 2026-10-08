import struct

TOKEN = b"PBG892FXX982ABC*"
with open("items.dat", "rb") as f:
    data = f.read()
pos = 0
def u8():
    global pos
    v = data[pos]; pos += 1; return v
def u16():
    global pos
    v = struct.unpack_from("<H", data, pos)[0]; pos += 2; return v
def i16():
    global pos
    v = struct.unpack_from("<h", data, pos)[0]; pos += 2; return v
def u32():
    global pos
    v = struct.unpack_from("<I", data, pos)[0]; pos += 4; return v
def i32():
    global pos
    v = struct.unpack_from("<i", data, pos)[0]; pos += 4; return v
def skip(n):
    global pos
    pos += n
def pstr():
    global pos
    ln = i16()
    s = data[pos:pos+ln]; pos += ln
    return s
version = u16(); count = u32()
items = {}
for _ in range(count):
    iid = u16(); skip(2)
    prop = u8(); cat = u8(); typ = u8(); skip(1)
    ln = i16()
    raw = data[pos:pos+ln]; pos += ln
    name = bytes(b ^ TOKEN[(i + iid) % len(TOKEN)] for i, b in enumerate(raw)).decode("ascii", errors="replace")
    tex = pstr(); skip(4); skip(1)
    ingredient = i32(); skip(4)
    collision = u8(); hits_raw = u8(); hit_reset = i32()
    cloth = u8(); rarity = i16(); skip(1)
    audio = pstr(); skip(4); skip(4)
    s1 = pstr(); s2 = pstr(); s3 = pstr(); s4 = pstr()
    skip(16); tick = i32(); skip(2); skip(2)
    e1 = pstr(); e2 = pstr(); e3 = pstr(); skip(80)
    if version >= 0x0b: x = pstr()
    if version >= 0x0c: skip(4); skip(9)
    if version >= 0x0d: skip(4)
    if version >= 0x0e: skip(4)
    if version >= 0x0f: skip(25); x = pstr()
    if version >= 0x10: x = pstr()
    if version >= 0x11: skip(4)
    if version >= 0x12: skip(4)
    if version >= 0x13: skip(9)
    if version >= 0x15: skip(2)
    info = b""
    if version >= 0x16: info = pstr()
    sp0 = sp1 = 0
    if version >= 0x17: sp0 = u16(); sp1 = u16()
    if version >= 0x18: skip(1)
    if version >= 0x19:
        ln2 = i16()
        if ln2 > 6: pos += ln2
        skip(4)
    if version >= 0x1a: skip(1)
    try: infotxt = info.decode("ascii", errors="replace")
    except: infotxt = ""
    items[iid] = dict(name=name, type=typ, prop=prop, cat=cat, info=infotxt, tick=tick, rarity=rarity, hits=hits_raw, col=collision, reset=hit_reset)

def safe(s):
    return s.encode("ascii", errors="replace").decode("ascii")

print("== type 0x37 BOOTH (all) ==")
for k in sorted(items):
    if items[k]["type"] == 0x37:
        v = items[k]
        print(f"{k}: '{safe(v['name'])}' prop={v['prop']:#x} cat={v['cat']:#x} hits={v['hits']} reset={v['reset']}")
        print(f"   info: {safe(v['info'])[:500]}")

print("== type 0x50 LOCK_BOT (all) ==")
for k in sorted(items):
    if items[k]["type"] == 0x50:
        v = items[k]
        print(f"{k}: '{safe(v['name'])}' prop={v['prop']:#x} hits={v['hits']}")
        print(f"   info: {safe(v['info'])[:500]}")

print("== items with trade/swap/sell in info ==")
for k in sorted(items):
    v = items[k]
    hay = (v["name"] + " " + v["info"]).lower()
    if "trade" in hay or "swap" in hay:
        print(f"{k}: '{safe(v['name'])}' type={v['type']:#x}")
        print(f"   info: {safe(v['info'])[:400]}")

print("== vending type 0x3e (all) ==")
for k in sorted(items):
    if items[k]["type"] == 0x3e:
        v = items[k]
        print(f"{k}: '{safe(v['name'])}'")
        print(f"   info: {safe(v['info'])[:400]}")
