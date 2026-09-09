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
# The road mask is a separate, finer grid than the heightfield: at the 6 m
# terrain spacing a narrow mountain road is barely one sample wide, and its edges
# come out as staircase steps rather than as a road.
ROAD_N = 4097
# Kept in step with VehicleModels in src/Game/model_catalog.hpp: every model has
# to appear in the baked map, which tests/island_tests.cpp checks.
VEHICLE_MODELS = 19
N = 1025
STEP = 6.0
HALF = (N - 1) * STEP / 2
SEA = 0.0


def smooth(a, b, x):
    t = max(0., min(1., (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def spline(points, step=6.0):
    """Catmull-Rom through the given control points, sampled every `step` metres.

    Hand-authored roads are a handful of corners, and a corner in a polyline is
    a corner on the ground: the terrain is cut to the segments, so a straight
    road reads as ruled and a bend reads as a crease. Sampling a spline through
    the same corners keeps the authored route and loses the ruled look.
    """
    if len(points) < 3:
        return list(points)
    pts = [points[0]] + list(points) + [points[-1]]
    out = [points[0]]
    for i in range(len(pts) - 3):
        p0, p1, p2, p3 = (tuple(p) for p in pts[i:i + 4])
        length = math.dist((p1[0], p1[2]), (p2[0], p2[2]))
        for k in range(1, max(2, int(length / step)) + 1):
            t = k / max(2, int(length / step))
            t2, t3 = t * t, t * t * t
            out.append(tuple(
                0.5 * ((2 * p1[c]) + (-p0[c] + p2[c]) * t
                       + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t2
                       + (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t3)
                for c in range(3)))
    return out


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
    # Two distinct shorelines, broad continental foothills, and an open tidal bay.
    def island(cx, cz, rx, rz):
        u,v=(x-cx)/rx,(z-cz)/rz
        angle=math.atan2(v,u)
        coast=1+.035*math.sin(angle*5)+.022*math.cos(angle*9)
        return 1-smooth(coast-.10,coast+.045,math.hypot(u,v))
    east=island(720,0,1000,2100)
    west=island(-1620+95*math.sin(z/420),-100,1050,2200)
    mountains=(310*math.exp(-((x+1750)**2/460**2+(z+1200)**2/530**2))
               +120*math.exp(-((x+1250)**2/370**2+(z+1500)**2/450**2)))
    land=10+3*fbm(x/220,z/220)+mountains*(.85+.15*fbm(x/130,z/130))
    coverage=max(east,west)
    h=-38*(1-coverage)+land*coverage
    # Wide, gently sloping sand on the Atlantic frontage and a southern cove.
    beach=smooth(1515,1585,x)*(1-smooth(450,1050,abs(z)))
    cove=(1-smooth(110,240,abs(x-1000)))*smooth(1450,1580,z)*(1-smooth(1690,1840,z))
    shore=max(beach,cove)*coverage
    return h*(1-shore)+(2.2+.8*noise(x/90,z/90))*shore


def main(output):
    output.mkdir(parents=True, exist_ok=True)
    records = []
    roads = []
    markings = []
    pads = []
    buildings = 0

    def box(x, y, z, sx, sy, sz, color, solid=True, facade=False, yaw=0):
        nonlocal buildings
        buildings += int(facade)
        records.append(('B' if solid else 'D', x, y + sy / 2, z, sx, sy, sz, *color, int(facade), yaw))

    def road(points, width=7, curve=False):
        if curve:
            points = spline(points)
        for a, b in zip(points, points[1:]):
            roads.append((a, b, width))

    def clear_of_roads(x,z,radius):
        for a,b,width in roads:
            ax,_,az=a;bx,_,bz=b
            length2=(bx-ax)**2+(bz-az)**2
            if not length2: continue
            t=max(0,min(1,((x-ax)*(bx-ax)+(z-az)*(bz-az))/length2))
            if math.hypot(x-ax-t*(bx-ax),z-az-t*(bz-az))<width+radius: return False
        return True

    def marking(x, z, sx, sz, color):
        records.append(('M', x, z, sx, sz, *color))

    def tree(x, y, z, size=1):
        # Stable variety; palms by low coastal terrain, pines in the highlands.
        seed = (int(x*1000)*73856093 ^ int(z*1000)*19349663) & 0xffffffff
        seed ^= seed >> 16
        seed = (seed * 0x45d9f3b) & 0xffffffff
        seed ^= seed >> 16
        variant = (10 + seed % 2) if abs(x) > 650 and y < 15 else (6 + seed % 4) if y > 30 else seed % 6
        records.append(('T', x, y, z, 7*size, seed % 360, variant))

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
                # The pavement reaches the kerb: leaving a bare verge between
                # slab and asphalt is what put street furniture on the dirt.
                box(x,elevation,z,50,.16,50,(.56,.57,.55))
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
                    records.append(('U',x+side*23,elevation+.16,z+22,0,5,0))
                prop=(ix+iz*3)%8
                ph=[5,.9,1,.9,1.3,1,1.5,.7][prop]
                records.append(('U',x+22,elevation+.16,z-15,90,ph,prop))
                # Street lamps are placed at runtime from the scanned prop set,
                # against the kerb this pad creates - see World::placeProps().
                for side in (-1,1):
                    tree(x+side*16.5,elevation+.16,z+17.5)
                if (ix + iz) % 2 == 0:
                    route = [(x-19,elevation+.16,z-19),(x+19,elevation+.16,z-19),(x+19,elevation+.16,z+19),(x-19,elevation+.16,z+19)]
                    records.append(('P',.4+(ix%3)*.14,.4+(iz%3)*.12,.58,1.5+(ix%3)*.15,len(route),*(v for p in route for v in p)))
                if ix%2 == 0 and iz%2 == 0:
                    rx, rz = x-30, z-30
                    route = [(rx+12,elevation,rz+3.5),(rx+56.5,elevation,rz+3.5),(rx+56.5,elevation,rz+56.5),(rx+3.5,elevation,rz+56.5),(rx+3.5,elevation,rz+3.5)]
                    records.append(('V',(ix+iz)%2,90,6,len(route),*(v for p in route for v in p)))
        # Markings are surface records, not slabs: the loader lays them over
        # whatever shape the ground turned out to be. A junction next to a
        # graded connector is not level, and a flat slab there floats.
        for ix in range(nx+1):
            for iz in range(nz+1):
                x,z = xmin+ix*60,zmin+iz*60
                for side in (-1,1):
                    for stripe in range(-3,4):
                        marking(x+stripe*1.8,z+side*6.5,.85,2.6,(.83,.84,.78))
                        marking(x+side*6.5,z+stripe*1.8,2.6,.85,(.83,.84,.78))
        for ix in range(nx+1):
            for iz in range(nz):
                for off in (20,30,40):
                    marking(xmin+ix*60,zmin+iz*60+off,.16,4,(.85,.73,.35))
        for iz in range(nz+1):
            for ix in range(nx):
                for off in (20,30,40):
                    marking(xmin+ix*60+off,zmin+iz*60,4,.16,(.85,.73,.35))

    landmarks=[]
    def landmark(name,kind,x,y,z):
        landmarks.append({'name':name,'kind':kind,'position':[x,y,z]})
        records.append(('A',kind,x,y,z,name.replace(' ','_')))

    # Smaller, differently proportioned neighborhoods follow the arterial roads.
    # Downtown keeps the original test yard, now at the bay end of the beach island.
    district(0,0,6,6,8,'Downtown')
    district(600,120,5,7,10,'Art Deco quarter')
    district(1140,600,3,6,10,'Ocean hotels')
    district(780,-660,7,4,12,'Palm residential',True)
    district(360,1020,4,3,10,'Marina village',True)
    district(-1440,240,8,5,10,'Civic center')
    district(-1800,840,5,4,12,'Garden suburb',True)
    district(-1260,-420,4,3,16,'Old town',True)
    landmark('Bayfront','spawn',0,8,0)
    landmark('Art Deco quarter','district',630,10,90)
    landmark('Palm residential','district',780,12,-660)
    landmark('Marina village','district',360,10,1020)
    landmark('Garden suburb','district',-1800,12,840)

    links=[[(180,8,0),(300,9,30),(450,10,120)],
           [(750,10,120),(900,10,210),(1140,10,420)],
           [(600,10,-90),(640,11,-260),(780,12,-540)],
           [(0,8,180),(30,9,430),(240,10,720),(360,10,930)],
           [(510,10,1020),(740,10,1100),(1140,10,780)],
           [(-1440,10,390),(-1510,11,600),(-1650,12,840)],
           [(-1440,10,90),(-1420,12,-120),(-1260,16,-330)]]
    # Ocean Drive runs along the outer coast; frontage sits on the inland side.
    ocean=spline([(1000,10,-1560),(1270,10,-1140),(1460,10,-600),
                  (1540,10,0),(1490,10,600),(1280,10,1170),(870,10,1650)],12)
    road(ocean,12)
    links += [[(990,12,-660),(1200,11,-690),(1450,10,-640)],
              [(1230,10,600),(1360,10,620),(1490,10,600)],
              [(1140,10,780),(1250,10,950),(1280,10,1170)]]
    for link in links: road(link,9,True)
    landmark('Ocean Drive','road',1490,10,600)
    landmark('Sunrise Beach','beach',1590,3,250)
    landmark('South Beach','beach',1000,3,1570)
    # Grade-separated decks leave the bay open underneath both crossings.
    for z in (-180,900):
        left,right=-720,-300
        road([(-1260,16 if z<0 else 12,z),(left,18,z)],10)
        road([(right,18,z),(0,8 if z<0 else 10,z)],10)
        if z>0:
            road([(0,10,z),(240,10,960)],9)
            road([(-1800,12,960),(-1650,12,960),(-1260,12,900)],9)
        else:
            road([(-1260,16,-330),(-1260,16,z)],9)
        box((left+right)/2,16.8,z,right-left+12,1.2,24,(.52,.56,.57))
        records.append(('L',11,2,left,18,z,right,18,z))
        for side in (-1,1):
            box((left+right)/2,18,z+side*11.5,right-left,1.1,.5,(.8,.81,.74))
        for x in range(left+60,right,90):
            for side in (-1,1): box(x,-35,z+side*8,5,52,5,(.48,.51,.52))
    landmark('Bay Causeway','bridge',-450,18,-180)

    # Mountain road traverses the lower slopes before climbing in broad switchbacks.
    scenic=spline([(-1260,16,-510),(-1040,35,-700),(-1150,70,-920),
                   (-1450,110,-900),(-1890,145,-850),(-2160,180,-1100),
                   (-2110,210,-1370),(-1830,250,-1470),(-1670,278,-1240)],12)
    road(scenic,7)
    pads.append((-1670,-1240,26,24,278))
    landmark('Sierra Lookout','lookout',-1670,278,-1240)
    # A narrower earth trail winds through the forest to a second ridge.
    trail=spline([(-1670,278,-1240),(-1570,270,-1360),(-1500,242,-1520),
                  (-1370,200,-1610),(-1220,160,-1480)],8)
    trail=[(x,natural_height(x,z),z) for x,_,z in trail]
    trail[0]=scenic[-1]
    for i in range(1,len(trail)):
        x,y,z=trail[i]; a=trail[i-1]
        rise=.3*math.hypot(x-a[0],z-a[2])
        trail[i]=(x,max(a[1]-rise,min(a[1]+rise,y)),z)
    road(trail,2)
    landmark('Pine Ridge Trail','trail',-1500,242,-1520)
    landmark('Sierra Forest','forest',-2050,170,-1050)

    # Recognizable civic silhouettes, courtyards, entrance aprons and rooftop signs.
    for name,kind,cx,cz,color in [('Mercy Hospital','hospital',-1120,270,(.86,.86,.77)),
                                 ('Bay Police','police',-1760,240,(.30,.43,.58))]:
        pads.append((cx,cz,62,56,10))
        road([(cx,10,cz-70),(cx,10,cz-44)],8)
        road([(-1200 if cx>-1440 else -1680,10,90),(cx,10,cz-70)],8)
        box(cx,10,cz,100,.16,90,(.58,.59,.57))
        box(cx,10.16,cz+12,54,19,34,color,True,True)
        for side in (-1,1): box(cx+side*36,10.16,cz,16,10,48,color,True,True)
        box(cx,10.16,cz-10,18,4,8,(.18,.35,.38),False)
        if kind=='hospital':
            box(cx,31,cz+12,12,1,3,(.85,.12,.12),False)
            box(cx,31,cz+12,3,1,12,(.85,.12,.12),False)
        else:
            box(cx,29.16,cz+12,8,4,8,(.15,.24,.40),False)
        landmark(name,kind,cx,10.16,cz-32)
    for cx,cz,y in [(1050,-250,11),(-1940,330,11),(420,600,10)]:
        pads.append((cx,cz,75,85,y))
        landmark(('Lagoon Park' if cx==420 else 'Palms Park') if cx>0 else 'Cypress Park','park',cx,y,cz)
        for dx in (-45,-20,20,45):
            for dz in (-55,-25,25,55): tree(cx+dx,y,cz+dz,1.3)
        road([(cx-65,y,cz),(cx+65,y,cz)],2)
        road([(cx,y,cz-75),(cx,y,cz+75)],2)
    neighborhood_links=[[(780,12,-780),(490,12,-950),(180,11,-850),(0,9,-180)],
                        [(-1680,10,90),(-1920,12,-40),(-2240,13,140),(-2250,13,470),(-1950,12,840)],
                        [(510,10,1020),(600,10,1390),(850,10,1450),(960,10,1250),(1160,10,1170),(1280,10,1170)],
                        [(0,8,-180),(100,10,-420),(570,12,-660)],
                        [(450,10,120),(390,10,460),(420,10,600),(240,10,720)],
                        [(-1680,10,240),(-1820,11,330),(-1940,11,330)],
                        [(990,12,-660),(1080,11,-430),(1050,11,-250)]]
    neighborhood_paths=[spline(controls,12) for controls in neighborhood_links]
    for path in neighborhood_paths:
        road(path,7)

    for i,(a,b) in enumerate(zip(ocean,ocean[1:])):
        x,y,z=((a[k]+b[k])*.5 for k in range(3))
        length=math.hypot(b[0]-a[0],b[2]-a[2])
        dx,dz=(b[2]-a[2])/length,-(b[0]-a[0])/length
        yaw=math.degrees(math.atan2(b[0]-a[0],b[2]-a[2]))
        box(x,y+.025,z,.18,.02,4,(.9,.78,.36),False,False,yaw)
        for side in (-1,1):
            if clear_of_roads(x+dx*side*16,z+dz*side*16,3):
                box(x+dx*side*16,y,z+dz*side*16,5,.16,length+1,(.68,.67,.60),True,False,yaw)
        if i%4==0 and clear_of_roads(x+dx*18,z+dz*18,1):
            records.append(('U',x+dx*18,y+.16,z+dz*18,yaw,5,0))
    # Coastal palms and distinct pastel hotels along the long strip.
    for i in range(20,len(ocean)-20,5):
        x,y,z=ocean[i]
        px,_,pz=ocean[i-1]; nx,_,nz=ocean[i+1]
        length=math.hypot(nx-px,nz-pz); dx,dz=(nz-pz)/length,-(nx-px)/length
        tree(x+dx*20,y,z+dz*20,1.6)
        bx,bz=x-dx*50,z-dz*50
        if not clear_of_roads(bx,bz,24): continue
        pads.append((bx,bz,24,24,10))
        yaw=math.degrees(math.atan2(dx,dz))
        h=12+(i%5)*5
        box(bx,10,bz,32,h,30,(.76+(i%3)*.06,.62+(i%4)*.06,.63+(i%2)*.12),True,True,yaw)
        box(bx,10+h,bz,34,.6,32,(.88,.85,.74),False,False,yaw)
    # Preserve the entire vehicle catalog with curbside parking around the districts.
    for i in range(VEHICLE_MODELS):
        records.append(('V',i,90,0,1,-150+i*15,8,3.5))
    for a,b,width in roads: records.append(('L',width,2,*a,*b))

    # Curved low-density streets stitch the engineered districts into the landscape.
    # Lots face their street tangent; gaps become gardens rather than another grid.
    for path in neighborhood_paths:
        for i in range(4,len(path)-4,5):
            x,y,z=path[i]; px,_,pz=path[i-1]; nx,_,nz=path[i+1]
            length=math.hypot(nx-px,nz-pz); dx,dz=(nz-pz)/length,-(nx-px)/length
            yaw=math.degrees(math.atan2(dx,dz))
            for side in (-1,1):
                bx,bz=x+side*dx*32,z+side*dz*32
                # Existing district pads own their lots; preserve intersections.
                if any(abs(bx-cx)<rx+35 and abs(bz-cz)<rz+35 for cx,cz,rx,rz,_ in pads): continue
                if not clear_of_roads(bx,bz,18): continue
                pads.append((bx,bz,15,15,y))
                box(bx,y,bz,16,5+i%4,19,(.72+(i%3)*.07,.66+(i%4)*.04,.57+(i%2)*.12),True,True,yaw)
                box(bx,y+5+i%4,bz,18,.5,21,(.46,.27,.20),False,False,yaw)
                tree(bx+dx*side*15,y,bz+dz*side*15,1.1)
    # Long-distance traffic uses a closed, continuous road itinerary.
    # The sampled frontage can also be explored without traffic turning in place.
    pad_cells={}
    for pad in pads:
        cx,cz,rx,rz,_=pad
        for gz in range(math.floor((cz-rz-42)/128),math.floor((cz+rz+42)/128)+1):
            for gx in range(math.floor((cx-rx-42)/128),math.floor((cx+rx+42)/128)+1):
                pad_cells.setdefault((gx,gz),[]).append(pad)
    print('Sampling fractal heightmap and shaping terraces...', flush=True)
    heights = []
    for j in range(N):
        z = j*STEP-HALF
        for i in range(N):
            x = i*STEP-HALF
            h = natural_height(x,z)
            for cx,cz,rx,rz,y in pad_cells.get((math.floor(x/128),math.floor(z/128)),[]):
                d = max(abs(x-cx)-rx,abs(z-cz)-rz)
                influence = 1-smooth(0,42,d)
                h = h*(1-influence)+y*influence
            heights.append(h)
    masks = [0.]*(N*N)
    road_step = (N-1)*STEP/(ROAD_N-1)
    fine = bytearray(ROAD_N*ROAD_N)
    # Rasterize only each road's local bounding rectangle, keeping baking cheap.
    for a,b,width in roads:
        ax,ay,az = a; bx,by,bz = b
        length2 = (bx-ax)**2+(bz-az)**2
        reach = width+(70 if max(ay,by)>40 else 24)
        imin=max(0,math.floor((min(ax,bx)-reach+HALF)/STEP)); imax=min(N-1,math.ceil((max(ax,bx)+reach+HALF)/STEP))
        jmin=max(0,math.floor((min(az,bz)-reach+HALF)/STEP)); jmax=min(N-1,math.ceil((max(az,bz)+reach+HALF)/STEP))
        for j in range(jmin,jmax+1):
            for i in range(imin,imax+1):
                x,z=i*STEP-HALF,j*STEP-HALF
                t=max(0,min(1,((x-ax)*(bx-ax)+(z-az)*(bz-az))/length2))
                d=math.hypot(x-ax-t*(bx-ax),z-az-t*(bz-az))
                influence=1-smooth(width+4,reach,d)
                k=j*N+i
                heights[k]=heights[k]*(1-influence)+(ay+t*(by-ay))*influence
                masks[k]=max(masks[k],1-smooth(width-1,width+1,d))
        # Same rasterization again on the fine grid, which is what gets drawn.
        fmin=max(0,math.floor((min(ax,bx)-reach+HALF)/road_step)); fmax=min(ROAD_N-1,math.ceil((max(ax,bx)+reach+HALF)/road_step))
        gmin=max(0,math.floor((min(az,bz)-reach+HALF)/road_step)); gmax=min(ROAD_N-1,math.ceil((max(az,bz)+reach+HALF)/road_step))
        for g in range(gmin,gmax+1):
            row=g*ROAD_N
            z=g*road_step-HALF
            for f in range(fmin,fmax+1):
                x=f*road_step-HALF
                t=max(0,min(1,((x-ax)*(bx-ax)+(z-az)*(bz-az))/length2))
                d=math.hypot(x-ax-t*(bx-ax),z-az-t*(bz-az))
                value=int((145 if width<=2 else 255)*(1-smooth(width-1.2,width+1.2,d)))
                if value>fine[row+f]: fine[row+f]=value
    # Scatter natural vegetation away from roads, terrace edges and the shoreline.
    for cell_z in range(-2220,2221,16):
        for cell_x in range(-2680,1701,16):
            if random_at(cell_x,cell_z) < (-.35 if cell_x<-900 and cell_z<-550 else .45): continue
            x=cell_x+random_at(cell_x+31,cell_z)*7
            z=cell_z+random_at(cell_x,cell_z+57)*7
            if any(abs(x-cx)<rx+25 and abs(z-cz)<rz+25 for cx,cz,rx,rz,y in pads): continue
            i,j=round((x+HALF)/STEP),round((z+HALF)/STEP); k=j*N+i
            h=heights[k]
            if h<4 or h>290 or masks[k]>.01: continue
            if max(masks[k-2],masks[k+2],masks[k-2*N],masks[k+2*N])>.01: continue
            slope=max(abs(heights[k+1]-h),abs(heights[k+N]-h))/STEP
            if slope>.95: continue
            tree(i*STEP-HALF,h,j*STEP-HALF,(2.5 if h>45 else 1.3)+random_at(z,x)*.35)
    def baked_height(x,z):
        gx,gz=(x+HALF)/STEP,(z+HALF)/STEP
        ix,iz=min(N-2,int(gx)),min(N-2,int(gz))
        u,v=gx-ix,gz-iz
        a,b=heights[iz*N+ix],heights[iz*N+ix+1]
        c,d=heights[(iz+1)*N+ix],heights[(iz+1)*N+ix+1]
        return a+u*(b-a)+v*(c-a) if u+v<=1 else d+(1-u)*(c-d)+(1-v)*(b-d)
    # Earthworks may move a verge after it was dressed. Ground vegetation and
    # props against the final surface, and leave a clear shoulder on every road.
    road_cells={}
    for segment in [(r[3:6],r[6:9],r[1]) for r in records if r[0]=='L']:
        a,b,width=segment
        reach=width+4
        for gz in range(math.floor((min(a[2],b[2])-reach)/128),math.floor((max(a[2],b[2])+reach)/128)+1):
            for gx in range(math.floor((min(a[0],b[0])-reach)/128),math.floor((max(a[0],b[0])+reach)/128)+1):
                road_cells.setdefault((gx,gz),[]).append(segment)
    all_roads=roads
    platforms=[r for r in records if r[0]=='B' and r[5]<.3]
    grounded=[]
    for record in records:
        if record[0] not in ('T','U'): grounded.append(record);continue
        values=list(record);x,z=values[1],values[3]
        roads=road_cells.get((math.floor(x/128),math.floor(z/128)),[])
        if record[0]=='T' and not clear_of_roads(x,z,2): continue
        y=baked_height(x,z)
        for slab in platforms:
            if abs(x-slab[1])<slab[4]/2 and abs(z-slab[3])<slab[6]/2:
                y=max(y,slab[2]+slab[5]/2)
        values[2]=y
        grounded.append(tuple(values))
    records=grounded
    roads=all_roads
    # Routes retain bridge/sidewalk elevations, but never start below a sampled slope.
    vehicle_variant = 0
    for index,record in enumerate(records):
        if record[0] not in ('V','P'): continue
        values=list(record)
        if record[0] == 'V':
            values[1] = vehicle_variant % VEHICLE_MODELS
            vehicle_variant += 1
        start=5 if record[0]=='V' else 6
        for k in range(start,len(values),3):
            values[k+1]=max(values[k+1],baked_height(values[k],values[k+2]))
        records[index]=tuple(values)
    blob=b'STAISL2\n'+struct.pack('<Ifff',N,STEP,SEA,float(SEED))
    blob+=struct.pack('<%sf'%len(heights),*heights)+struct.pack('<%sf'%len(masks),*masks)
    # Fine road coverage, one byte per sample, after the two float grids.
    blob+=struct.pack('<I',ROAD_N)+bytes(fine)
    (output/'island.bin').write_bytes(blob)
    scene='STA_SCENE 3\n'+'\n'.join(' '.join(str(round(v,4)) if isinstance(v,float) else str(v) for v in r) for r in records)+'\n'
    (output/'island.scene').write_text(scene)
    # Lossless overview PNG, made from the same baked height and road data.
    footprints={}
    for r in records:
        if r[0]!='B' or (not r[10] and r[4]<100): continue
        _,x,y,z,sx,sy,sz,*_=r
        for j in range(max(0,int((z-sz/2+HALF)/STEP)),min(N,int((z+sz/2+HALF)/STEP)+1)):
            for i in range(max(0,int((x-sx/2+HALF)/STEP)),min(N,int((x+sx/2+HALF)/STEP)+1)):
                footprints[j*N+i]=(165,166,147)
    scan=bytearray()
    for j in range(N):
        scan.append(0)
        for i in range(N):
            k=j*N+i; h=heights[k]; r=masks[k]
            if h<0: c=(24,65+int(max(-30,h)+30),103+int(max(-30,h)+30))
            elif h<3: c=(184,172,126)
            else: c=(int(62+h*.5),int(103+h*.3),int(64+h*.48))
            if r>.3: c=(94,100,103)
            if k in footprints: c=footprints[k]
            scan.extend(max(0,min(255,v)) for v in c)
    def chunk(name,data): return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',N,N,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan,9))+chunk(b'IEND',b'')
    (output/'island-overview.png').write_bytes(png)
    manifest={'version':3,'seed':SEED,'grid':N,'spacing_metres':STEP,'extent_metres':HALF*2,'sea_level':SEA,
              'height_range':[min(heights),max(heights)],'road_mask_grid':ROAD_N,'buildings':buildings,'scene_records':len(records),
              'vehicles':sum(r[0]=='V' for r in records),'pedestrians':sum(r[0]=='P' for r in records),
              'road_segments':sum(r[0]=='L' for r in records),
              'name':'Twin Palms','landmarks':landmarks,
              'sha256':{name:hashlib.sha256((output/name).read_bytes()).hexdigest() for name in ('island.bin','island.scene','island-overview.png')}}
    (output/'island.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'resources/maps')
    main(parser.parse_args().output)
