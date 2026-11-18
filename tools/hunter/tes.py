"""Minimal Fallout: New Vegas plugin (TES4 format) reader/writer."""
import struct, zlib

class Rec:
    def __init__(self, typ, fid, flags=0, subs=None, vc=0, fver=15, unk=0):
        self.typ, self.fid, self.flags = typ, fid, flags
        self.subs = subs if subs is not None else []   # list of [sig, bytes]
        self.vc, self.fver, self.unk = vc, fver, unk
    def get(self, sig, default=None):
        for s, d in self.subs:
            if s == sig: return d
        return default
    def all(self, sig): return [d for s, d in self.subs if s == sig]
    def set(self, sig, data):
        for s in self.subs:
            if s[0] == sig: s[1] = data; return
        self.subs.append([sig, data])
    def edid(self):
        e = self.get('EDID'); return e[:-1].decode('latin1') if e else None
    def copy(self): return Rec(self.typ, self.fid, self.flags, [[s, d] for s, d in self.subs], self.vc, self.fver, self.unk)
    def pack(self):
        body = b''
        for s, d in self.subs:
            if len(d) > 0xFFFF:
                body += b'XXXX' + struct.pack('<HI', 4, len(d)) + s.encode() + struct.pack('<H', 0) + d
            else:
                body += s.encode() + struct.pack('<H', len(d)) + d
        flags = self.flags & ~0x00040000
        return self.typ.encode() + struct.pack('<IIIIHH', len(body), flags, self.fid, self.vc, self.fver, self.unk) + body

def parse_subs(data):
    subs, i, big = [], 0, None
    while i < len(data):
        sig = data[i:i+4].decode('latin1'); ln = struct.unpack_from('<H', data, i+4)[0]; i += 6
        if sig == 'XXXX': big = struct.unpack_from('<I', data, i)[0]; i += ln; continue
        if big is not None: ln = big; big = None
        subs.append([sig, data[i:i+ln]]); i += ln
    return subs

class Grup:
    def __init__(self, label, gtype, items=None, stamp=0, unk=0):
        self.label, self.gtype, self.items, self.stamp, self.unk = label, gtype, items if items is not None else [], stamp, unk
    def pack(self):
        body = b''.join(x.pack() for x in self.items)
        lab = self.label if isinstance(self.label, bytes) else struct.pack('<I', self.label)
        return b'GRUP' + struct.pack('<I', len(body) + 24) + lab + struct.pack('<iII', self.gtype, self.stamp, self.unk) + body

def read_plugin(path, want=None):
    """returns (header Rec, list of top-level items). want: set of top-level GRUP labels to parse (others skipped)."""
    data = open(path, 'rb').read()
    def rd(i, end):
        out = []
        while i < end:
            typ = data[i:i+4].decode('latin1')
            if typ == 'GRUP':
                size, = struct.unpack_from('<I', data, i+4)
                label = data[i+8:i+12]; gtype, stamp, unk = struct.unpack_from('<iII', data, i+12)
                if gtype == 0 and want is not None and label.decode('latin1') not in want:
                    i += size; continue
                g = Grup(label if gtype == 0 else struct.unpack('<I', label)[0], gtype, rd(i+24, i+size), stamp, unk)
                out.append(g); i += size
            else:
                size, flags, fid, vc, fver, unk = struct.unpack_from('<IIIIHH', data, i+4)
                body = data[i+24:i+24+size]
                if flags & 0x00040000:
                    body = zlib.decompress(body[4:])
                out.append(Rec(typ, fid, flags, parse_subs(body), vc, fver, unk)); i += 24 + size
        return out
    items = rd(0, len(data))
    return items[0], items[1:]

def walk(items):
    for x in items:
        if isinstance(x, Grup): yield from walk(x.items)
        else: yield x

def z(s): return s.encode('latin1') + b'\0'
def u32(*v): return struct.pack('<' + 'I' * len(v), *v)
def f32(*v): return struct.pack('<' + 'f' * len(v), *v)
