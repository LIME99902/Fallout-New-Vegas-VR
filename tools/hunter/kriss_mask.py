import pickle, numpy as np
from PIL import Image, ImageDraw, ImageFilter
V,UV,TR,lab=pickle.load(open('/home/claude/hunter/data/kbody.pkl','rb'))
x0,x1=V[:,0].min(),V[:,0].max(); y0,y1=V[:,1].min(),V[:,1].max()
sc=(2048-40)/(x1-x0)
X=lambda x:20+(x-x0)*sc; Y=lambda y:512-(y-(y0+y1)/2)*sc
xrear=x0+(800-20)/sc; xfront=x0+(1868-20)/sc
black=set()
for k in range(lab.max()+1):
    m=lab==k; c=V[m].mean(0); mn=V[m].min(0)
    if c[0] < xrear or c[0] > xfront or mn[1] > y1-1.05 or UV[m][:,1].mean() > 0.895: black.add(k)
S=2048
mask=np.zeros((S,S),np.float32)
for a,b,c in TR:
    P=UV[[a,b,c]]; P=(P-np.floor(P.mean(0)))*S; Q=V[[a,b,c]]   # (wrapped UVs brought back into the picture)
    xs0,ys0=np.floor(P.min(0)).astype(int); xs1,ys1=np.ceil(P.max(0)).astype(int)
    xs0=max(xs0,0); ys0=max(ys0,0); xs1=min(xs1,S-1); ys1=min(ys1,S-1)
    if xs1<xs0 or ys1<ys0: continue
    gx,gy=np.meshgrid(np.arange(xs0,xs1+1)+0.5,np.arange(ys0,ys1+1)+0.5)
    (ax,ay),(bx,by),(cx,cy)=P
    den=(by-cy)*(ax-cx)+(cx-bx)*(ay-cy)
    if abs(den)<1e-9: continue
    w0=((by-cy)*(gx-cx)+(cx-bx)*(gy-cy))/den; w1=((cy-ay)*(gx-cx)+(ax-cx)*(gy-cy))/den; w2=1-w0-w1
    ins=(w0>=-0.02)&(w1>=-0.02)&(w2>=-0.02)
    if lab[a] in black: val=ins.astype(np.float32)
    else:
        px=X(w0*Q[0,0]+w1*Q[1,0]+w2*Q[2,0]); py=Y(w0*Q[0,1]+w1*Q[1,1]+w2*Q[2,1])
        val=(ins&(px>1612)&(py>335)&(py<458)).astype(np.float32)
    sub=mask[ys0:ys1+1,xs0:xs1+1]; np.maximum(sub,val,out=sub)
m=Image.fromarray((mask*255).astype('uint8')).filter(ImageFilter.MaxFilter(3))
m.save('/tmp/claude-0/kmask.png')
