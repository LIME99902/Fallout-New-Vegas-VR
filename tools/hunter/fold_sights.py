"""Fold the KRISS Vector's flip-up iron sights flat onto the top rail (rear one forward, front one back), so you can
see through a red dot between them. usage: fold_sights.py in.nif out.nif"""
import struct, sys, numpy as np
sys.path.insert(0, '/home/claude/hunter')
from split_handle import rd_hdr, wr, tname, avobj_end, geom_layout

# (x range of the sight, hinge x, hinge y, angle in degrees about the gun's side axis)
SIGHTS = [((-0.8, 1.1), 0.45, 6.25, -90.0),    # rear: tips forward
          ((15.5, 17.6), 16.0, 6.20, 90.0)]    # front: tips back

def main(src, dst):
    H, B, footer = rd_hdr(open(src, 'rb').read())
    S = [s.decode('latin1') for s in H['strs']]
    n = 0
    for k in range(len(B)):
        if tname(H, k) != 'NiTriShape': continue
        nm = S[struct.unpack_from('<i', B[k], 0)[0]]
        if nm not in ('Tri 9mmSMG 6', '9mmSMG.001 6'): continue
        o = avobj_end(B[k]); dk, = struct.unpack_from('<i', B[k], o)
        db = B[dk]; L = geom_layout(db); nv = L['nv']
        V = np.frombuffer(bytes(db[L['verts'][0]:L['verts'][0] + 12 * nv]), '<f4').reshape(nv, 3).copy()
        arrays = [L['verts'][0]] + ([L['norm'][0]] if L['norm'][1] else []) + ([L['tan'][0], L['bitan'][0]] if 'tan' in L else [])
        for (x0, x1), hx, hy, deg in SIGHTS:
            m = (V[:, 0] >= x0) & (V[:, 0] <= x1) & (V[:, 1] > hy + 0.3)
            if not m.any(): continue
            a = np.radians(deg); c, s = np.cos(a), np.sin(a)
            for ai, off in enumerate(arrays):
                A = np.frombuffer(bytes(db[off:off + 12 * nv]), '<f4').reshape(nv, 3).copy()
                X, Y = A[m, 0].copy(), A[m, 1].copy()
                if ai == 0: X -= hx; Y -= hy
                A[m, 0] = c * X - s * Y; A[m, 1] = s * X + c * Y
                if ai == 0: A[m, 0] += hx; A[m, 1] += hy
                db[off:off + 12 * nv] = A.astype('<f4').tobytes()
            n += int(m.sum())
    open(dst, 'wb').write(wr(H, B, footer))
    print(dst, 'vertices folded', n)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
