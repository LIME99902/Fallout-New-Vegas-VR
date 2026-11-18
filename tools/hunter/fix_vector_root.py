"""The KRISS Vector's body sits in a node named '9mmSMG' - the name the 9mm SMG reload animation moves, so with
FNVR's manual reloading the whole gun dropped out of the hand while it reloaded. The node is renamed (the animation
no longer finds it) and Hunter's C-More sight is hung inside it so it always stays on the gun.
usage: fix_vector_root.py in.nif out.nif"""
import struct, sys
sys.path.insert(0, '/home/claude/hunter')
from split_handle import rd_hdr, wr, tname, avobj_end

def kids(B, k):
    e = avobj_end(B[k]); nc, = struct.unpack_from('<I', B[k], e)
    return e, nc, list(struct.unpack_from('<%di' % nc, B[k], e + 4))

def set_kids(B, k, new):
    e, nc, _ = kids(B, k)
    B[k] = B[k][:e] + struct.pack('<I', len(new)) + struct.pack('<%di' % len(new), *new) + B[k][e + 4 + 4 * nc:]

def main(src, dst):
    H, B, footer = rd_hdr(open(src, 'rb').read())
    S = [s.decode('latin1') for s in H['strs']]
    name = lambda k: S[struct.unpack_from('<i', B[k], 0)[0]]
    body = [k for k in range(len(B)) if tname(H, k) == 'NiNode' and name(k) in ('9mmSMG', 'KrissBody')]
    if not body: open(dst, 'wb').write(open(src, 'rb').read()); print(dst, 'no 9mmSMG node'); return
    bk = body[0]
    si = struct.unpack_from('<i', B[bk], 0)[0]; H['strs'][si] = b'KrissBody'
    cm = [k for k in range(len(B)) if tname(H, k) == 'NiNode' and name(k) == 'C-More Parts']
    if cm:
        ck = cm[0]
        for k in range(len(B)):
            if tname(H, k) in ('NiNode', 'BSFadeNode'):
                _, _, ks = kids(B, k)
                if ck in ks and k != bk: set_kids(B, k, [x for x in ks if x != ck])
        _, _, ks = kids(B, bk)
        if ck not in ks: set_kids(B, bk, ks + [ck])   # (the body node sits at the root's origin, so the sight keeps its place)
    open(dst, 'wb').write(wr(H, B, footer))
    print(dst, 'fixed', '(C-More moved into the body)' if cm else '')

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
