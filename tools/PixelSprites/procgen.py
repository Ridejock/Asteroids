import numpy as np
from PIL import Image
OUT=(31,39,58)
def outline(img):
    a=img[...,3]>0; p=np.pad(a,1); n=p[:-2,1:-1]|p[2:,1:-1]|p[1:-1,:-2]|p[1:-1,2:]
    e=n&~a; img[e]=(*OUT,255); return img
def pad(img,m=1):
    h,w=img.shape[:2]; o=np.zeros((h+2*m,w+2*m,4),np.uint8); o[m:m+h,m:m+w]=img; return o
def even(img):
    h,w=img.shape[:2]; o=np.zeros((h+h%2,w+w%2,4),np.uint8); o[:h,:w]=img; return o
def shade(pal,t):
    t=np.clip(t,0,0.999); return np.array(pal)[(t*len(pal)).astype(int)]

# ---------------- enemy saucer (points down) ----------------
def enemy(W=44,H=42):
    img=np.zeros((H,W,4),np.uint8); cx=(W-1)/2
    hull=[(41,49,70),(55,54,81),(85,91,114),(121,125,147),(162,163,182),(199,202,215),(235,236,243)]
    purp=[(48,22,64),(78,34,102),(118,52,148),(160,84,196),(200,130,230)]
    red=[(110,14,34),(176,24,44),(232,54,60),(255,128,110)]
    Y,X=np.mgrid[0:H,0:W]
    # disc: ellipse centered, rx=20.5 ry=11 at y=17
    dx=(X-cx)/20.5; dy=(Y-17)/11.0; disc=dx**2+dy**2<=1
    # lower hull bulge (belly pointing down) & prong
    bel=((X-cx)/12.5)**2+((Y-24)/10)**2<=1
    prong=(np.abs(X-cx)<=3.0-(Y-30)*0.3)&(Y>=28)&(Y<=39)
    # side prongs pointing down
    fins=np.zeros_like(disc)
    for s in (-1,1):
        fx=cx+s*(15-(Y-22)*0.35); fins|=(np.abs(X-fx)<=2.6-(Y-22)*0.25)&(Y>=20)&(Y<=33)
    m=disc|bel|prong|fins
    # lighting: top-left light
    nz=np.sqrt(np.clip(1-dx**2-dy**2,0,1))
    L=0.12+0.8*np.clip(nz*0.55-dx*0.3-dy*0.3+0.1,0,1)
    col=shade(hull,L)
    # purple band ring on rim
    rim=disc&(dx**2+dy**2>0.62)
    col[rim]=shade(purp,0.35+0.5*(-dx[rim]*0.3-dy[rim]*0.5+0.4))
    # belly / fins / prong in purple
    lower=(bel|fins|prong)&~disc
    col[lower]=shade(purp,0.15+0.5*(1-(Y[lower]-20)/18)-0.2*(X[lower]-cx)/14)
    # dome (cockpit) - red glass, top
    dm=((X-cx)/7.5)**2+((Y-13)/5.5)**2<=1
    dd=((X-cx)/7.5)**2+((Y-13)/5.5)**2
    col[dm]=shade(red,0.95-dd[dm]*0.8-((X[dm]-cx)/7.5)*0.2-((Y[dm]-13)/5.5)*0.25)
    # highlight speck on dome
    for (yy,xx) in [(10,int(cx)-3),(10,int(cx)-2),(11,int(cx)-3)]: col[yy,xx]=(255,200,190)
    # rim lights: red dots along equator
    for ang in np.linspace(-0.85,0.85,7):
        lx=int(round(cx+np.sin(ang)*17.5)); ly=int(round(17+np.cos(ang)*6.8))
        col[ly,lx]=red[3]; col[ly,lx+1 if lx<cx else lx-1]=red[2]; col[ly+1,lx]=red[1]
    # panel lines
    for k in (-9,-4.5,4.5,9):
        xx=int(round(cx+k)); 
        for yy in range(6,26):
            if disc[yy,xx] and not dm[yy,xx] and not rim[yy,xx]: col[yy,xx]=hull[1]
    for yy in (18,):
        row=disc[yy]&~dm[yy]&~rim[yy]; col[yy][row]=hull[1]
    fl=(fins)&~disc&(Y>=31); col[fl]=red[2]
    col[prong&(Y>=34)]=red[2]; col[prong&(Y>=36)]=red[3]
    img[m,:3]=col[m]; img[m,3]=255
    # mirror left half onto right for perfect symmetry
    img[:,W//2:]=img[:,:W//2][:,::-1] if W%2==0 else img[:,W//2:]
    return even(outline(pad(crop(img))))
def crop(img):
    ys,xs=np.nonzero(img[...,3]); return img[ys.min():ys.max()+1,xs.min():xs.max()+1]

# ---------------- asteroids ----------------
ROCK=[(38,34,40),(56,50,54),(76,68,68),(98,88,82),(122,110,98),(148,134,116),(176,162,140),(204,192,170)]
def asteroid(D,seed):
    rng=np.random.default_rng(seed); R=D/2-1.5; c=D/2-0.5
    Y,X=np.mgrid[0:D,0:D].astype(float); ang=np.arctan2(Y-c,X-c); r=np.hypot(X-c,Y-c)
    # lumpy radius via random fourier
    rad=np.ones_like(ang)
    for k in range(2,6): rad+=rng.uniform(0.03,0.11)*np.cos(k*ang+rng.uniform(0,6.3))*(1.2 if k==2 else 1)
    rad*=R/rad.max()*1.0
    m=r<=rad
    nx=(X-c)/R; ny=(Y-c)/R; nz=np.sqrt(np.clip(1-nx**2-ny**2,0,1))
    L=0.08+0.95*np.clip(nz*0.5-nx*0.45-ny*0.55+0.22,0,1)
    # low-freq blotches
    b=np.zeros_like(L)
    for _ in range(6):
        bx,by,br=rng.uniform(0,D),rng.uniform(0,D),rng.uniform(D*0.1,D*0.3)
        b+=rng.uniform(-0.12,0.12)*np.exp(-((X-bx)**2+(Y-by)**2)/(br**2))
    L+=b
    # rim darkening toward shadow edge
    L-=np.clip((r/rad-0.8)*1.2,0,1)*np.clip(nx+ny+0.3,0,1)*0.6
    ncr=max(1,int(D/9))+rng.integers(0,2)
    for _ in range(ncr):
        for _t in range(30):
            a_=rng.uniform(0,6.3); dd=rng.uniform(0,0.6)*R; cr=rng.uniform(max(1.4,D*0.07),max(2.0,D*0.16))
            px,py=c+np.cos(a_)*dd,c+np.sin(a_)*dd
            if np.hypot(px-c,py-c)+cr<R*0.92: break
        q=np.hypot(X-px,Y-py)/cr
        inn=q<=1
        # crater: inner dark on upper-left side (shadow), lit on lower-right inner wall, rim highlight upper-left outside
        dirx=(X-px)/cr; diry=(Y-py)/cr
        L[inn]=np.minimum(L[inn],0.28+0.30*np.clip(dirx[inn]+diry[inn],-1,1)*0.5+0.12)
        rimz=(q>1)&(q<=1.0+1.6/cr)&((dirx+diry)<0.2)
        L[rimz]+=0.14
    col=shade(ROCK,L)
    img=np.zeros((D,D,4),np.uint8); img[m,:3]=col[m]; img[m,3]=255
    return even(outline(pad(crop(img))))

if __name__=='__main__':
    S='/workspace/sprites/'
    Image.fromarray(enemy()).save(S+'enemy.png')
    for name,D,seeds in [('large',54,(11,27)),('medium',28,(5,42)),('small',14,(3,8))]:
        for i,sd in enumerate(seeds,1):
            Image.fromarray(asteroid(D,sd)).save(f'{S}asteroid_{name}_{i}.png')
