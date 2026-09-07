#!/usr/bin/env python3
"""Offline island authoring tool. Run explicitly; never called by CMake or the game.
Only Python's standard library is required. Outputs are deterministic for SEED.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

SEED = 731942
N = 513
STEP = 4.0
HALF = (N - 1) * STEP / 2
SEA = 0.0


def smooth(a, b, x):
    t = max(0., min(1., (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def random_at(x, z):
    n = (int(x) * 374761393 + int(z) * 668265263 + SEED * 1447) & 0xffffffff
    n = ((n ^ (n >> 13)) * 1274126177) & 0xffffffff
    return ((n ^ (n >> 16)) & 0xffff) / 32767.5 - 1


def noise(x, z):
    ix, iz = math.floor(x), math.floor(z)
    fx, fz = smooth(0, 1, x - ix), smooth(0, 1, z - iz)
    a = random_at(ix, iz) * (1 - fx) + random_at(ix + 1, iz) * fx
    b = random_at(ix, iz + 1) * (1 - fx) + random_at(ix + 1, iz + 1) * fx
    return a * (1 - fz) + b * fz


def fbm(x, z):
    return sum(noise(x * 2**i, z * 2**i) / 2**i for i in range(5)) / 1.9375


def natural_height(x, z):
    angle = math.atan2(z / .9, (x + 20) / 1.07)
    radius = math.hypot((x + 20) / 1.07, z / .9)
    coast = 740 * (1 + .06 * math.sin(angle * 3) + .045 * math.cos(angle * 7))
    inland = 1 - smooth(coast - 95, coast + 30, radius)
    mountains = (155 * math.exp(-((x + 110)**2 / 170**2 + (z + 440)**2 / 145**2))
                 + 88 * math.exp(-((x - 340)**2 / 150**2 + (z + 420)**2 / 155**2))
                 + 66 * math.exp(-((x + 650)**2 / 120**2 + (z + 280)**2 / 160**2)))
    ridges = 0.72 + .36 * (1 - abs(fbm(x / 75, z / 75)))
    land = 14 + 8 * fbm(x / 150, z / 150) + mountains * ridges
    h = -38 * (1 - inland) + land * inland
    channel_x = -430 + 14 * math.sin(z / 110)
    channel = 1 - smooth(27, 55, abs(x - channel_x))
    h = h * (1 - channel) + min(h, -7) * channel
    # A gentler intertidal profile creates beaches and visible shallow water.
    return h * (.55 + .45 * smooth(8,25,abs(h)))


def main(output):
    output.mkdir(parents=True, exist_ok=True)
    records = []
    roads = []
    pads = []
    buildings = 0

    def box(x, y, z, sx, sy, sz, color, solid=True, facade=False, yaw=0):
        nonlocal buildings
        buildings += int(facade)
        records.append(('B' if solid else 'D', x, y + sy / 2, z, sx, sy, sz, *color, int(facade), yaw))

    def road(points, width=7):
        for a, b in zip(points, points[1:]):
            roads.append((a, b, width))

    def tree(x, y, z, size=1):
        box(x, y, z, .65, 3.1*size, .65, (.32,.23,.16))
        box(x, y+2.8*size, z, 3.8*size, 3.6*size, 3.8*size, (.20,.38,.22), False)
        box(x+.3, y+5.4*size, z, 2.7*size, 1.6*size, 2.7*size, (.27,.45,.25), False)

    def district(cx, cz, nx, nz, elevation, label, low_rise=False):
        # Each district is a level engineered terrace, blended into the hills.
        xmin, zmin = cx - nx*30, cz - nz*30
        pads.append((cx, cz, nx*30+10, nz*30+10, elevation))
        for i in range(nx+1):
            x = xmin + i*60
            road([(x,elevation,zmin), (x,elevation,zmin+nz*60)],9)
        for j in range(nz+1):
            z = zmin + j*60
            road([(xmin,elevation,z), (xmin+nx*60,elevation,z)],9)
        for ix in range(nx):
            for iz in range(nz):
                x, z = xmin+ix*60+30, zmin+iz*60+30
                park = (label == 'Downtown' and ix == 3 and iz == 3) or (label != 'Downtown' and ix == nx//2 and iz == nz//2)
                yard = label == 'Downtown' and ix == 1 and iz == 3
                box(x,elevation,z,44,.16,44,(.56,.57,.55))
                if park or yard:
                    box(x,elevation+.17,z,34,.02,34,(.23,.39,.23) if park else (.39,.39,.39),False)
                    if yard:
                        records.append(('R',x-8,z,6,14,0,elevation+.16,elevation+3.66,.65,.58,.38))
                        records.append(('R',x+8,z,6,14,1,elevation+.16,elevation+3.66,.65,.58,.38))
                    else:
                        for k in (-1,0,1):
                            box(x+k*9,elevation+.16,z,3,.6,1,(.48,.29,.15))
                            box(x+k*9,elevation+.75,z+.5,3,.65,.15,(.48,.29,.15),False)
                else:
                    for lot in range(2):
                        bx = x + (-9 if lot == 0 else 9)
                        seed = ix*7+iz+int(cx+cz)+lot*13
                        h = (5 + seed%9) if low_rise else (10+seed%34)
                        color = (.48+(seed%3)*.08,.46+(seed%4)*.05,.43+(seed%5)*.04)
                        box(bx,elevation+.16,z,16,h,30,color,True,True)
                        box(bx,elevation+h+.16,z,16.5,.4,30.5,(.25,.28,.30),False)
                        box(bx,elevation+h+.56,z+3,4,1,5,(.42,.45,.47),False)
                        box(bx,elevation+2.8,z-15.4,12,.3,1.1,(.18,.42,.45) if lot else (.72,.31,.18),False)
                        box(bx,elevation+.16,z-15.02,1.6,2.2,.04,(.12,.19,.22),False)
                for side in (-1,1):
                    tree(x+side*16.5,elevation+.16,z+17.5)
                    lx = x+side*20.5
                    box(lx,elevation+.16,z-16,.18,5.5,.18,(.18,.22,.25))
                    box(lx,elevation+5.65,z-16.6,.6,.18,1.6,(.95,.88,.63),False)
                if (ix + iz) % 2 == 0:
                    route = [(x-19,elevation+.16,z-19),(x+19,elevation+.16,z-19),(x+19,elevation+.16,z+19),(x-19,elevation+.16,z+19)]
                    records.append(('P',.4+(ix%3)*.14,.4+(iz%3)*.12,.58,1.5+(ix%3)*.15,len(route),*(v for p in route for v in p)))
                if ix%2 == 0 and iz%2 == 0:
                    rx, rz = x-30, z-30
                    route = [(rx+12,elevation,rz+3.5),(rx+56.5,elevation,rz+3.5),(rx+56.5,elevation,rz+56.5),(rx+3.5,elevation,rz+56.5),(rx+3.5,elevation,rz+3.5)]
                    records.append(('V',(ix+iz)%2,90,6,len(route),*(v for p in route for v in p)))
        # Markings are baked geometry, not generated at startup.
        for ix in range(nx+1):
            for iz in range(nz+1):
                x,z = xmin+ix*60,zmin+iz*60
                for side in (-1,1):
                    for stripe in range(-3,4):
                        box(x+stripe*1.8,elevation+.015,z+side*6.5,.85,.025,2.6,(.83,.84,.78),False)
                        box(x+side*6.5,elevation+.015,z+stripe*1.8,2.6,.025,.85,(.83,.84,.78),False)
        for ix in range(nx+1):
            for iz in range(nz):
                for off in (20,30,40):
                    box(xmin+ix*60,elevation+.015,zmin+iz*60+off,.16,.025,4,(.85,.73,.35),False)
        for iz in range(nz+1):
            for ix in range(nx):
                for off in (20,30,40):
                    box(xmin+ix*60+off,elevation+.015,zmin+iz*60,4,.025,.16,(.85,.73,.35),False)

    district(0,0,6,6,8,'Downtown')
    district(390,90,4,3,26,'East gardens',True)
    district(90,-330,3,3,32,'Highland',True)
    district(-610,90,3,3,8,'West harbor',True)
    # Connections are authored gentle grades; heightmap is cut/filled around them.
    road([(180,8,0),(230,14,0),(270,26,0)])
    road([(0,8,-180),(0,20,-210),(0,32,-240)])
    road([(-180,8,120),(-300,10,120),(-378,12,120)])
    road([(-482,12,120),(-492,12,120),(-520,8,120)])
    road([(-180,8,-120),(-260,13,-180),(-378,12,-180)])
    road([(-482,12,-180),(-550,14,-180),(-610,8,0)])
    scenic = [(180,32,-330),(240,46,-350),(290,64,-410),(230,92,-480),(158,110,-500),(130,110,-500),(102,110,-497),
              (20,106,-490),(-90,91,-420),(-160,63,-330),(-220,36,-250),(-260,13,-180)]
    road(scenic,5)
    south = [(-180,8,120),(-220,10,240),(-150,13,340),(0,16,390),(150,18,390),
             (290,18,320),(390,26,180)]
    road(south,6)
    # Bridges cross the tidal strait. Endpoints overlap road earthworks.
    for z in (120,-180):
        box(-430,10.8,z,108,1.2,15,(.31,.34,.36))
        records.append(('L',7.5,2,-484,12,z,-376,12,z))
        for x in (-464,-396): box(x,25.5,z,1.4,1,13,(.62,.35,.24))
        for side in (-1,1): box(-430,25.5,z+side*6,68,.7,.5,(.62,.35,.24))
        for side in (-1,1):
            box(-430,12,z+side*7.2,108,1.1,.35,(.67,.69,.66))
            for x in (-464,-396):
                box(x,-8,z+side*5.5,2,20,2,(.46,.49,.48))
                box(x,12,z+side*6,1.1,15,1.1,(.62,.35,.24))
        for x in range(-478,-380,10):
            box(x,12.01,z,4,.025,.16,(.85,.73,.35),False)
    # Mountain lookout terrace and a small lakeside-style picnic park.
    pads.append((130,-510,23,20,110))
    box(130,109.8,-510,42,.2,34,(.48,.49,.45))
    for x in (114,130,146): box(x,110,-518,3,.6,1,(.48,.29,.15))
    # Harbor piers extend into the channel; supports are visible in shallow water.
    for z in (60,180):
        box(-510,6.8,z,58,1.2,12,(.46,.40,.30))
        for x in (-532,-508,-486):
            for side in (-1,1): box(x,-8,z+side*4,1,14,1,(.3,.29,.26))
    for type_,pos,yaw in [(1,(5.5,8,12),0),(0,(-12,8,5.5),90),(2,(5.5,8,-18),180),
                          (0,(395.5,26,0),0),(1,(5.5,32,-330),0),(2,(-628,8,5.5),90)]:
        records.append(('V',type_,yaw,0,1,*pos))

    arterial=[(180,8,0),(230,14,0),(270,26,0),(390,26,0),(390,26,180),(290,18,320),
              (150,18,390),(0,16,390),(-150,13,340),(-220,10,240),(-180,8,120),(0,8,120),(0,8,0)]
    bridges=[(-180,8,120),(-300,10,120),(-378,12,120),(-482,12,120),(-520,8,120),(-640,8,120),(-640,8,0),
             (-610,8,0),(-550,14,-180),(-482,12,-180),(-378,12,-180),(-260,13,-180),(-180,8,-120)]
    mountain=[(180,32,-330),*scenic[1:],(-180,8,-120),(0,8,-120),(0,8,-180),(0,20,-210),(0,32,-240),(0,32,-300),(180,32,-300)]
    for route,yaw in ((arterial,90),(bridges,-90),(mountain,72)):
        records.append(('V',0,yaw,7,len(route),*(v for p in route for v in p)))
    for a,b,width in roads: records.append(('L',width,2,*a,*b))

    print('Sampling fractal heightmap and shaping terraces...', flush=True)
    heights = []
    for j in range(N):
        z = j*STEP-HALF
        for i in range(N):
            x = i*STEP-HALF
            h = natural_height(x,z)
            for cx,cz,rx,rz,y in pads:
                d = max(abs(x-cx)-rx,abs(z-cz)-rz)
                influence = 1-smooth(0,42,d)
                h = h*(1-influence)+y*influence
            heights.append(h)
    masks = [0.]*(N*N)
    # Rasterize only each road's local bounding rectangle, keeping baking cheap.
    for a,b,width in roads:
        ax,ay,az = a; bx,by,bz = b
        length2 = (bx-ax)**2+(bz-az)**2
        reach = width+12
        imin=max(0,math.floor((min(ax,bx)-reach+HALF)/STEP)); imax=min(N-1,math.ceil((max(ax,bx)+reach+HALF)/STEP))
        jmin=max(0,math.floor((min(az,bz)-reach+HALF)/STEP)); jmax=min(N-1,math.ceil((max(az,bz)+reach+HALF)/STEP))
        for j in range(jmin,jmax+1):
            for i in range(imin,imax+1):
                x,z=i*STEP-HALF,j*STEP-HALF
                t=max(0,min(1,((x-ax)*(bx-ax)+(z-az)*(bz-az))/length2))
                d=math.hypot(x-ax-t*(bx-ax),z-az-t*(bz-az))
                influence=1-smooth(width+1,reach,d)
                k=j*N+i
                heights[k]=heights[k]*(1-influence)+(ay+t*(by-ay))*influence
                masks[k]=max(masks[k],1-smooth(width-1,width+1,d))
    # Scatter natural vegetation away from roads, terrace edges and the shoreline.
    for cell_z in range(-620,621,19):
        for cell_x in range(-740,701,19):
            if random_at(cell_x,cell_z) < -.05: continue
            x=cell_x+random_at(cell_x+31,cell_z)*7
            z=cell_z+random_at(cell_x,cell_z+57)*7
            if any(abs(x-cx)<rx+25 and abs(z-cz)<rz+25 for cx,cz,rx,rz,y in pads): continue
            i,j=round((x+HALF)/STEP),round((z+HALF)/STEP); k=j*N+i
            h=heights[k]
            if h<4 or h>100 or masks[k]>.01: continue
            if max(masks[k-2],masks[k+2],masks[k-2*N],masks[k+2*N])>.01: continue
            slope=max(abs(heights[k+1]-h),abs(heights[k+N]-h))/STEP
            if slope>.65: continue
            tree(i*STEP-HALF,h,j*STEP-HALF,1+random_at(z,x)*.25)
    def baked_height(x,z):
        gx,gz=(x+HALF)/STEP,(z+HALF)/STEP
        ix,iz=min(N-2,int(gx)),min(N-2,int(gz))
        u,v=gx-ix,gz-iz
        a,b=heights[iz*N+ix],heights[iz*N+ix+1]
        c,d=heights[(iz+1)*N+ix],heights[(iz+1)*N+ix+1]
        return a+u*(b-a)+v*(c-a) if u+v<=1 else d+(1-u)*(c-d)+(1-v)*(b-d)
    # Routes retain bridge/sidewalk elevations, but never start below a sampled slope.
    for index,record in enumerate(records):
        if record[0] not in ('V','P'): continue
        values=list(record)
        start=5 if record[0]=='V' else 6
        for k in range(start,len(values),3):
            values[k+1]=max(values[k+1],baked_height(values[k],values[k+2]))
        records[index]=tuple(values)
    blob=b'STAISL1\n'+struct.pack('<Ifff',N,STEP,SEA,float(SEED))
    blob+=struct.pack('<%sf'%len(heights),*heights)+struct.pack('<%sf'%len(masks),*masks)
    (output/'island.bin').write_bytes(blob)
    scene='STA_SCENE 1\n'+'\n'.join(' '.join(str(round(v,4)) if isinstance(v,float) else str(v) for v in r) for r in records)+'\n'
    (output/'island.scene').write_text(scene)
    # Lossless overview PNG, made from the same baked height and road data.
    scan=bytearray()
    for j in range(N):
        scan.append(0)
        for i in range(N):
            k=j*N+i; h=heights[k]; r=masks[k]
            if h<0: c=(24,65+int(max(-30,h)+30),103+int(max(-30,h)+30))
            elif h<3: c=(184,172,126)
            else: c=(int(62+h*.5),int(103+h*.3),int(64+h*.48))
            if r>.3: c=(94,100,103)
            scan.extend(c)
    def chunk(name,data): return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',N,N,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan,9))+chunk(b'IEND',b'')
    (output/'island-overview.png').write_bytes(png)
    manifest={'version':1,'seed':SEED,'grid':N,'spacing_metres':STEP,'extent_metres':HALF*2,'sea_level':SEA,
              'height_range':[min(heights),max(heights)],'buildings':buildings,'scene_records':len(records),
              'vehicles':sum(r[0]=='V' for r in records),'pedestrians':sum(r[0]=='P' for r in records),
              'road_segments':sum(r[0]=='L' for r in records),
              'districts':['Downtown','East gardens','Highland','West harbor'],
              'sha256':{name:hashlib.sha256((output/name).read_bytes()).hexdigest() for name in ('island.bin','island.scene','island-overview.png')}}
    (output/'island.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'resources/maps')
    main(parser.parse_args().output)
