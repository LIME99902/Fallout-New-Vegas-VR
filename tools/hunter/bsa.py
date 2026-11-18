import struct, zlib, os
def bsa_list(path):
    fh=open(path,'rb'); h=fh.read(36)
    _,ver,off,af,nfo,nfi,lfo,lfi,ff=struct.unpack('<4sIIIIIIII',h)
    fh.seek(off); fr=[struct.unpack('<QII',fh.read(16)) for _ in range(nfo)]
    recs=[]
    for _,cnt,_o in fr:
        n=fh.read(1)[0]; name=fh.read(n)[:-1].decode('latin1')
        for _ in range(cnt):
            _h,sz,fo=struct.unpack('<QII',fh.read(16)); recs.append([name,sz,fo])
    names=fh.read(lfi).split(b'\0')
    out={}
    for (folder,sz,fo),nm in zip(recs,names):
        out[(folder+'\\'+nm.decode('latin1')).lower()]=(sz,fo)
    return fh,(af,ver),out
def bsa_read(fh,afv,ent):
    af,ver=afv
    sz,fo=ent; comp=bool(af&4)^bool(sz&0x40000000); sz&=0x3FFFFFFF
    fh.seek(fo); d=fh.read(sz)
    if af&0x100 and ver>=104: d=d[1+d[0]:]
    if comp: d=zlib.decompress(d[4:])
    return d
