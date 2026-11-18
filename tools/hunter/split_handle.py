"""Cut the KRISS Vector's charging handle (the part sticking out of the left side of the receiver) out of the body mesh
into its own shape, 'FoldChargingHandle', so FNVR can swing it out and pull it back.
usage: split_handle.py in.nif out.nif"""
import struct, sys, numpy as np

def rd_hdr(d):
    i = d.index(b'\n') + 1
    H = {'line': d[:i]}
    H['ver'], = struct.unpack_from('<I', d, i); i += 4
    H['endian'] = d[i]; i += 1
    H['uv'], nb, H['uv2'] = struct.unpack_from('<III', d, i); i += 12
    ex = []
    for _ in range(3):
        l = d[i]; ex.append(d[i:i + 1 + l]); i += 1 + l
    H['export'] = ex
    nt, = struct.unpack_from('<H', d, i); i += 2
    types = []
    for _ in range(nt):
        l, = struct.unpack_from('<I', d, i); i += 4; types.append(d[i:i + l]); i += l
    idx = list(struct.unpack_from('<%dH' % nb, d, i)); i += 2 * nb
    sizes = list(struct.unpack_from('<%dI' % nb, d, i)); i += 4 * nb
    ns, mx = struct.unpack_from('<II', d, i); i += 8
    strs = []
    for _ in range(ns):
        l, = struct.unpack_from('<I', d, i); i += 4; strs.append(d[i:i + l]); i += l
    ng, = struct.unpack_from('<I', d, i); i += 4
    groups = d[i:i + 4 * ng]; i += 4 * ng
    blocks = []
    for k in range(nb):
        blocks.append(bytearray(d[i:i + sizes[k]])); i += sizes[k]
    footer = d[i:]
    H.update(types=types, idx=idx, strs=strs, ng=ng, groups=groups)
    return H, blocks, footer

def wr(H, blocks, footer):
    o = bytearray(H['line']) + struct.pack('<I', H['ver']) + bytes([H['endian']]) + struct.pack('<III', H['uv'], len(blocks), H['uv2'])
    for e in H['export']: o += e
    o += struct.pack('<H', len(H['types']))
    for t in H['types']: o += struct.pack('<I', len(t)) + t
    o += struct.pack('<%dH' % len(blocks), *H['idx'])
    o += struct.pack('<%dI' % len(blocks), *[len(b) for b in blocks])
    o += struct.pack('<II', len(H['strs']), max(len(s) for s in H['strs']))
    for s in H['strs']: o += struct.pack('<I', len(s)) + s
    o += struct.pack('<I', H['ng']) + H['groups']
    for b in blocks: o += b
    return bytes(o + footer)

def tname(H, k): return H['types'][H['idx'][k] & 0x7FFF].decode()

def avobj_end(b):   # NiAVObject (FO3): name, extra list, controller, flags u32, transform, properties, collision
    o = 4; ne, = struct.unpack_from('<I', b, o); o += 4 + 4 * ne + 4 + 4 + 12 + 36 + 4
    npr, = struct.unpack_from('<I', b, o); o += 4 + 4 * npr + 4
    return o

def geom_layout(b):
    """offsets of the NiTriShapeData arrays"""
    L = {}; o = 4
    nv, = struct.unpack_from('<H', b, o); o += 2; L['nv'] = nv
    o += 2
    hv = b[o]; o += 1
    L['verts'] = (o, 12 if hv else 0); o += 12 * nv if hv else 0
    vf, = struct.unpack_from('<H', b, o); o += 2; L['vf'] = vf
    hn = b[o]; o += 1
    L['norm'] = (o, 12 if hn else 0); o += 12 * nv if hn else 0
    if hn and vf & 4096:
        L['tan'] = (o, 12); o += 12 * nv; L['bitan'] = (o, 12); o += 12 * nv
    L['center'] = o; o += 16
    hc = b[o]; o += 1
    L['col'] = (o, 16 if hc else 0); o += 16 * nv if hc else 0
    nuv = vf & 63
    L['uv'] = (o, 8 * nuv); o += 8 * nv * nuv
    o += 2 + 4
    L['ntri_at'] = o
    nt, = struct.unpack_from('<H', b, o); o += 2
    o += 4
    ht = b[o]; o += 1
    L['tris'] = o; L['ntri'] = nt if ht else 0
    o += 6 * L['ntri']
    L['tail'] = o
    return L

def main(src, dst):
    d = open(src, 'rb').read()
    H, B, footer = rd_hdr(d)
    S = [s.decode('latin1') for s in H['strs']]
    if 'FoldChargingHandle' in S: open(dst, 'wb').write(d); print('already split'); return
    body = [k for k in range(len(B)) if tname(H, k) == 'NiTriShape' and S[struct.unpack_from('<i', B[k], 0)[0]] == 'Tri 9mmSMG 1']
    if not body: print('no body shape - copied as is'); open(dst, 'wb').write(d); return
    sk = body[0]
    o = avobj_end(B[sk]); dk, = struct.unpack_from('<i', B[sk], o)
    db = B[dk]; L = geom_layout(db); nv = L['nv']
    V = np.frombuffer(bytes(db[L['verts'][0]:L['verts'][0] + 12 * nv]), '<f4').reshape(nv, 3)
    T = np.frombuffer(bytes(db[L['tris']:L['tris'] + 6 * L['ntri']]), '<u2').reshape(-1, 3)
    # the handle: triangles whose vertices all sit out on the left side (z < -1.0) between x 13.5 and 19
    c = V[T].mean(1)
    sel = (V[T][:, :, 2].max(1) < -0.30) & (c[:, 0] > 13.5) & (c[:, 0] < 19.2) & (c[:, 1] > 2.9) & (c[:, 1] < 3.75) & (V[T][:, :, 2].min(1) < -1.0)
    # grow to the whole connected piece (shared vertices) within the same box
    used = set(T[sel].ravel().tolist())
    for _ in range(6):
        add = np.array([(not sel[i]) and any(v in used for v in T[i]) and c[i, 0] > 13.5 and c[i, 0] < 19.2 and c[i, 1] > 2.9 and c[i, 1] < 3.75 and c[i, 2] < -0.3 for i in range(len(T))])
        if not add.any(): break
        sel |= add; used = set(T[sel].ravel().tolist())
    print('handle triangles', int(sel.sum()), 'of', len(T))
    if sel.sum() < 4: open(dst, 'wb').write(d); print('handle not found - copied as is'); return
    keep = T[~sel]; ht = T[sel]
    # body: fewer triangles
    nb_ = bytearray(db[:L['ntri_at']]) + struct.pack('<HI', len(keep), 3 * len(keep)) + bytes([1]) + keep.astype('<u2').tobytes() + db[L['tail']:]
    B[dk] = nb_
    # the handle's own data: just its vertices
    vi = sorted(set(ht.ravel().tolist())); remap = {v: i for i, v in enumerate(vi)}; n2 = len(vi)
    def arr(key):
        off, sz = L[key]
        return b''.join(bytes(db[off + sz * v: off + sz * (v + 1)]) for v in vi) if sz else b''
    hd = bytearray(db[:4]) + struct.pack('<H', n2) + db[6:8] + bytes([db[L['verts'][0] - 1]]) + arr('verts')
    hd += struct.pack('<H', L['vf']) + bytes([db[L['verts'][0] + 12 * nv + 2]]) + arr('norm')
    if 'tan' in L: hd += arr('tan') + arr('bitan')
    hv = V[vi]; ctr = (hv.min(0) + hv.max(0)) / 2; rad = float(np.linalg.norm(hv - ctr, axis=1).max())
    hd += struct.pack('<4f', *ctr, rad)
    hd += bytes([db[L['center'] + 16]]) + arr('col')
    off, sz = L['uv']
    if sz:
        nuv = sz // 8
        for s in range(nuv):
            base = off + s * 8 * nv
            hd += b''.join(bytes(db[base + 8 * v: base + 8 * v + 8]) for v in vi)
    hd += bytes(db[L['ntri_at'] - 6:L['ntri_at']])   # consistency flags + additional data
    t2 = np.vectorize(remap.get)(ht).astype('<u2')
    hd += struct.pack('<HI', len(t2), 3 * len(t2)) + bytes([1]) + t2.tobytes() + struct.pack('<H', 0)
    # the handle's shape: a copy of the body's, own name and data
    H['strs'].append(b'FoldChargingHandle')
    hs = bytearray(B[sk]); struct.pack_into('<i', hs, 0, len(H['strs']) - 1); struct.pack_into('<i', hs, o, len(B) + 1)
    data_type = H['idx'][dk]; shape_type = H['idx'][sk]
    B.append(hs); H['idx'].append(shape_type)
    B.append(hd); H['idx'].append(data_type)
    newk = len(B) - 2
    # hang it under the body's parent node
    for k in range(len(B) - 2):
        if tname(H, k) not in ('NiNode', 'BSFadeNode'): continue
        e = avobj_end(B[k]); nc, = struct.unpack_from('<I', B[k], e)
        kids = list(struct.unpack_from('<%di' % nc, B[k], e + 4))
        if sk in kids:
            B[k] = B[k][:e] + struct.pack('<I', nc + 1) + struct.pack('<%di' % (nc + 1), *(kids + [newk])) + B[k][e + 4 + 4 * nc:]
            break
    open(dst, 'wb').write(wr(H, B, footer))
    print('wrote', dst)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
