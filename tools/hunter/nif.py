"""Minimal NIF 20.2.0.7 (Fallout 3/NV) reader: shapes, their names, UVs, triangles, texture paths."""
import struct

class Nif:
    def __init__(self, path):
        d = self.d = open(path, 'rb').read()
        i = d.index(b'\n') + 1
        self.ver, = struct.unpack_from('<I', d, i); i += 4
        i += 1  # endian
        self.uv, nb, self.uv2 = struct.unpack_from('<III', d, i); i += 12
        # export info: 3 short strings (byte len)
        for _ in range(3):
            l = d[i]; i += 1 + l
        nt, = struct.unpack_from('<H', d, i); i += 2
        types = []
        for _ in range(nt):
            l, = struct.unpack_from('<I', d, i); i += 4; types.append(d[i:i+l].decode()); i += l
        idx = struct.unpack_from('<%dH' % nb, d, i); i += 2 * nb
        sizes = struct.unpack_from('<%dI' % nb, d, i); i += 4 * nb
        ns, _mx = struct.unpack_from('<II', d, i); i += 8
        self.strings = []
        for _ in range(ns):
            l, = struct.unpack_from('<I', d, i); i += 4; self.strings.append(d[i:i+l].decode('latin1')); i += l
        ng, = struct.unpack_from('<I', d, i); i += 4 + 4 * ng
        self.blocks = []
        for k in range(nb):
            self.blocks.append((types[idx[k] & 0x7FFF], i, sizes[k])); i += sizes[k]

    def s(self, n): return self.strings[n] if 0 <= n < len(self.strings) else None

    def avobj(self, o):
        d = self.d
        name, = struct.unpack_from('<i', d, o); o += 4
        ne, = struct.unpack_from('<I', d, o); o += 4 + 4 * ne
        o += 4  # controller
        o += 4  # flags (uint32 for Bethesda version > 26)
        o += 12 + 36 + 4
        npr, = struct.unpack_from('<I', d, o); o += 4
        props = struct.unpack_from('<%di' % npr, d, o); o += 4 * npr
        o += 4  # collision
        return self.s(name), props, o

    def shapes(self):
        out = []
        for k, (t, o, sz) in enumerate(self.blocks):
            if t != 'NiTriShape': continue
            name, props, p = self.avobj(o)
            data, = struct.unpack_from('<i', self.d, p)
            tex = None
            for pr in props:
                if pr >= 0 and self.blocks[pr][0] == 'BSShaderPPLightingProperty':
                    tex = self.texset(pr)
            out.append((k, name, data, tex))
        return out

    def texset(self, pr):
        # find the BSShaderTextureSet ref: last int32 before... scan block for a ref to a BSShaderTextureSet
        t, o, sz = self.blocks[pr]
        for j in range(o, o + sz - 3):
            r, = struct.unpack_from('<i', self.d, j)
            if 0 <= r < len(self.blocks) and self.blocks[r][0] == 'BSShaderTextureSet':
                tt, to, tsz = self.blocks[r]
                n, = struct.unpack_from('<i', self.d, to); q = to + 4; paths = []
                for _ in range(n):
                    l, = struct.unpack_from('<I', self.d, q); q += 4; paths.append(self.d[q:q+l].decode('latin1')); q += l
                return paths
        return None

    def geom(self, k):
        d = self.d; t, o, sz = self.blocks[k]
        o += 4  # group id
        nv, = struct.unpack_from('<H', d, o); o += 2
        o += 2  # keep, compress
        hv = d[o]; o += 1
        verts = []
        if hv: verts = [struct.unpack_from('<3f', d, o + 12 * i) for i in range(nv)]; o += 12 * nv
        vf, = struct.unpack_from('<H', d, o); o += 2
        hn = d[o]; o += 1
        if hn:
            o += 12 * nv
            if vf & 4096: o += 24 * nv
        o += 16
        hc = d[o]; o += 1
        if hc: o += 16 * nv
        uvs = []
        nuv = vf & 63
        if nuv:
            uvs = [struct.unpack_from('<2f', d, o + 8 * i) for i in range(nv)]
            o += 8 * nv * nuv
        o += 2 + 4  # consistency, additional data
        ntri, = struct.unpack_from('<H', d, o); o += 2
        o += 4
        ht = d[o]; o += 1
        tris = [struct.unpack_from('<3H', d, o + 6 * i) for i in range(ntri)] if ht else []
        self.lastverts = verts
        return uvs, tris
