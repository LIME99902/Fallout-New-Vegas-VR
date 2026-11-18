"""Put the FAMAS mod's C-More red dot on Hunter's KRISS Vector models (and hide the Vector's own holo sight).
usage: graft_cmore.py famas_sight_c-more.nif in.nif out.nif"""
import struct, sys
sys.path.insert(0, '/home/claude/hunter')
from split_handle import rd_hdr, wr, tname, avobj_end, geom_layout

NODE = ('NiNode', 'BSFadeNode')

def net_end(b):   # NiObjectNET: name, extra data refs, controller
    ne, = struct.unpack_from('<I', b, 4); return 4 + 4 + 4 * ne + 4

def refs(H, B, k):
    """(offset, is_string) of every reference / string index in block k"""
    t = tname(H, k); b = B[k]; out = []
    if t in ('BSShaderTextureSet',): return out
    if t in ('NiTriStripsData', 'NiTriShapeData'):
        L = geom_layout(b); out.append((L['ntri_at'] - 4, False)); return out
    out.append((0, True))
    ne, = struct.unpack_from('<I', b, 4)
    out += [(8 + 4 * i, False) for i in range(ne)]
    out.append((8 + 4 * ne, False))   # controller
    if t in NODE or t in ('NiTriStrips', 'NiTriShape'):
        o = 8 + 4 * ne + 4 + 4 + 12 + 36 + 4
        npr, = struct.unpack_from('<I', b, o); out += [(o + 4 + 4 * i, False) for i in range(npr)]
        o += 4 + 4 * npr; out.append((o, False)); o += 4     # collision
        if t in NODE:
            nc, = struct.unpack_from('<I', b, o); out += [(o + 4 + 4 * i, False) for i in range(nc)]; o += 4 + 4 * nc
            nf, = struct.unpack_from('<I', b, o); out += [(o + 4 + 4 * i, False) for i in range(nf)]
        else:
            out += [(o, False), (o + 4, False)]; o += 8           # data, skin
            nm, = struct.unpack_from('<I', b, o); o += 4
            out += [(o + 4 * i, True) for i in range(nm)]
    elif t == 'BSShaderPPLightingProperty':
        out.append((net_end(b) + 22, False))
    return out

def subtree(H, B, root):
    seen, todo = [], [root]
    while todo:
        k = todo.pop(0)
        if k in seen or k < 0: continue
        seen.append(k)
        for o, isstr in refs(H, B, k):
            if not isstr:
                r, = struct.unpack_from('<i', B[k], o)
                if r >= 0: todo.append(r)
    return seen

def main(src_sight, src, dst, dx=2.7, dy=-6.58, dz=0.0):
    sh, sb, _ = rd_hdr(open(src_sight, 'rb').read())
    H, B, footer = rd_hdr(open(src, 'rb').read())
    names = [s.decode('latin1') for s in H['strs']]
    if 'C-More Parts' in names: open(dst, 'wb').write(open(src, 'rb').read()); print('already has it'); return
    sn = [s.decode('latin1') for s in sh['strs']]
    root = [k for k in range(len(sb)) if tname(sh, k) in NODE and sn[struct.unpack_from('<i', sb[k], 0)[0]] == 'C-More Parts'][0]
    ks = subtree(sh, sb, root)
    newidx = {k: len(B) + i for i, k in enumerate(ks)}
    stramap = {}
    def s_idx(i):
        if i < 0: return i
        if i not in stramap: H['strs'].append(sh['strs'][i]); stramap[i] = len(H['strs']) - 1
        return stramap[i]
    for k in ks:
        b = bytearray(sb[k])
        for o, isstr in refs(sh, sb, k):
            v, = struct.unpack_from('<i', b, o)
            struct.pack_into('<i', b, o, s_idx(v) if isstr else (newidx[v] if v >= 0 else v))
        tn = sh['types'][sh['idx'][k] & 0x7FFF]
        if tn not in H['types']: H['types'].append(tn)
        H['idx'].append(H['types'].index(tn))
        B.append(b)
    # placed on the Vector's top rail
    nb = B[newidx[root]]; o = net_end(nb) + 4
    tx, ty, tz = struct.unpack_from('<3f', nb, o); struct.pack_into('<3f', nb, o, tx + dx, ty + dy, tz + dz)
    # hung under the model's root
    r0 = 0; e = avobj_end(B[r0]); nc, = struct.unpack_from('<I', B[r0], e)
    kids = list(struct.unpack_from('<%di' % nc, B[r0], e + 4))
    B[r0] = B[r0][:e] + struct.pack('<I', nc + 1) + struct.pack('<%di' % (nc + 1), *(kids + [newidx[root]])) + B[r0][e + 4 + 4 * nc:]
    # the Vector's own holo sight hidden (its 'Holo' node, when the model has one)
    for k in range(len(B)):
        if tname(H, k) in NODE:
            nm = H['strs'][struct.unpack_from('<i', B[k], 0)[0]].decode('latin1')
            if nm == 'Holo':
                fo = net_end(B[k]); f, = struct.unpack_from('<I', B[k], fo); struct.pack_into('<I', B[k], fo, f | 1)
    open(dst, 'wb').write(wr(H, B, footer))
    print('wrote', dst, 'blocks added', len(ks))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
