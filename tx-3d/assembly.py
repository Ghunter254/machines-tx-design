"""OpenCascade solids. All dimensions are mm; assembly placement is Z-up.

Winding packs include the unresolved strand insulation. Their displayed CAD
volume is an envelope; copper mass always comes from the C result per phase.
"""
from dataclasses import dataclass
from math import cos, sin, pi, sqrt, ceil
from functools import lru_cache
import cadquery as cq


MATERIALS = {
    'core': {'color': (0.30, 0.35, 0.40), 'density': 7850, 'metalness': 0.65, 'roughness': 0.34},
    'copper_pack': {'color': (0.64, 0.29, 0.12), 'density': None, 'metalness': 0.60, 'roughness': 0.29},
    'copper': {'color': (0.77, 0.43, 0.20), 'density': 8960, 'metalness': 0.75, 'roughness': 0.26},
    'pressboard': {'color': (0.73, 0.56, 0.32), 'density': 1100, 'metalness': 0.0, 'roughness': 0.78},
    'paint': {'color': (0.24, 0.34, 0.41), 'density': 7850, 'metalness': 0.35, 'roughness': 0.31},
    'steel': {'color': (0.52, 0.57, 0.61), 'density': 7850, 'metalness': 0.8, 'roughness': 0.30},
    'porcelain': {'color': (0.24, 0.14, 0.09), 'density': 2400, 'metalness': 0.0, 'roughness': 0.20},
    'rubber': {'color': (0.10, 0.11, 0.12), 'density': 1200, 'metalness': 0.0, 'roughness': 0.8},
    'oil': {'color': (0.94, 0.66, 0.20, 0.15), 'density': 860, 'metalness': 0.0, 'roughness': 0.1},
    'glass': {'color': (0.40, 0.70, 0.78, 0.38), 'density': 2500, 'metalness': 0.0, 'roughness': 0.12},
    'silica': {'color': (0.84, 0.45, 0.11), 'density': None, 'metalness': 0.0, 'roughness': 0.6},
    'enamel': {'color': (0.86, 0.88, 0.86), 'density': None, 'metalness': 0.0, 'roughness': 0.45},
}


@lru_cache(maxsize=512)
def box(x, y, z):
    return cq.Workplane('XY').box(x, y, z, centered=(True, True, False)).val()


@lru_cache(maxsize=512)
def ring(outer, inner, height):
    return cq.Workplane('XY').circle(outer / 2).circle(inner / 2).extrude(height).val()


@lru_cache(maxsize=512)
def cylinder(diameter, height):
    return cq.Workplane('XY').circle(diameter / 2).extrude(height).val()


def rounded_plate(length, width, height, radius):
    return cq.Workplane('XY').box(length, width, height, centered=(True, True, False)).edges('|Z').fillet(radius).val()


def flange(length, width, overhang, height, radius):
    return rounded_plate(length + 2 * overhang, width + 2 * overhang, height, radius + overhang).cut(
        rounded_plate(length, width, height + 2, radius).translate((0, 0, -1)))


def rod_between(a, b, diameter):
    va, vb = cq.Vector(*a), cq.Vector(*b)
    delta = vb - va
    return cq.Solid.makeCylinder(diameter / 2, delta.Length, va, delta.normalized())


@dataclass
class Part:
    name: str
    label: str
    group: str
    material: str
    shape: object
    position: tuple
    explode: tuple
    stage: int
    provenance: str
    dimensions: dict
    analytical_mass: float | None = None


class TransformerAssembly:
    def __init__(self, spec):
        self.s = spec
        self.parts = []
        self.checks = []
        self.packet_schedule = []
        self.tube_positions = []

    def add(self, name, label, group, material, shape, position=(0, 0, 0), explode=(0, 0, 0), stage=1,
            provenance='CAD construction assumption', dimensions=None, analytical_mass=None):
        if any(p.name == name for p in self.parts): raise ValueError(f'Duplicate part name: {name}')
        if not shape.isValid() or not shape.Solids() or shape.Volume() <= 0:
            raise ValueError(f'Invalid or non-solid component: {name}')
        self.parts.append(Part(name, label, group, material, shape, position, explode, stage, provenance,
                               dimensions or {}, analytical_mass))

    def build(self):
        for name in ('core', 'windings', 'supports', 'enclosure', 'cooling', 'accessories'):
            print(f'Building {name}...', flush=True)
            getattr(self, name)()
        return self

    def core(self):
        s = self.s
        r = s.d / 2
        depths = [v * r for v in s.cad['corePacketHalfDepthFractions']]
        widths = [min(s.frame['yokeWidthM'] * 1000, 2 * sqrt(r*r-y*y)) for y in depths[1:]]
        raw_area = sum(2 * (b-a) * w for a, b, w in zip(depths, depths[1:], widths))
        target = s.frame['grossCoreAreaM2'] * 1e6
        scale = target / raw_area
        if scale > 1: raise ValueError('Configured core packets cannot fit the required gross area within the limb circle')
        widths = [w * scale for w in widths]
        for band, (a, b, width) in enumerate(zip(depths, depths[1:], widths)):
            self.packet_schedule.append({'band': band + 1, 'widthMm': width, 'depthMm': b-a, 'copiesPerLimb': 2})
            for side in (-1, 1):
                y = side * (a+b)/2
                for phase, x in enumerate((-s.pitch, 0, s.pitch)):
                    self.add(f'core_{phase}_{band}_{side:+d}', f'{"ABC"[phase]} limb · packet {band+1} {"front" if side < 0 else "rear"}',
                        'core', 'core', box(width, b-a, s.window_height),
                        (x, y, s.core_bottom+s.yoke_height), (phase * 60 - 60, side*(band+1)*26, 0), 1,
                        'C gross area and limb envelope; area-matched stepped packet schedule',
                        {'widthMm': width, 'depthMm': b-a, 'heightMm': s.window_height})
        for top in (False, True):
            self.add('yoke_upper' if top else 'yoke_lower', 'Upper yoke' if top else 'Lower yoke', 'core', 'core',
                box(s.frame['yokeLengthM']*1000, s.frame['yokeWidthM']*1000, s.yoke_height),
                (0, 0, s.core_bottom+(s.yoke_height+s.window_height if top else 0)),
                (0, 0, 520 if top else -150), 5 if top else 1, 'C frame.yokeLengthM / yokeWidthM / yokeHeightM',
                {'heightMm': s.yoke_height, 'grossAreaMm2': s.frame['yokeGrossAreaM2']*1e6})
        self.checks.append({'name': 'Stepped limb cross-section', 'status': 'PASS', 'actual': target, 'expected': target, 'unit': 'mm²'})

    def windings(self):
        s, a = self.s, self.s.assumptions
        for phase, x in enumerate((-s.pitch, 0, s.pitch)):
            phase_label = 'ABC'[phase]
            exp = ((phase-1)*260, -330, 240)
            # Distinct pressboard formers with the C oil ducts left physically open.
            for family, inner, thick in (
                ('lv', s.d+2*a['coreToLvOilDuctMm'], a['coreToLvCylinderMm']),
                ('hv', s.lv['outerDiameterMm']+2*a['lvToHvOilDuctMm'], a['lvToHvCylinderMm'])):
                w = getattr(s, family)
                self.add(f'{family}_former_{phase}', f'{phase_label} · {family.upper()} pressboard former', 'insulation', 'pressboard',
                    ring(inner+2*thick, inner, w['occupiedHeightMm']), (x, 0, s.winding_mid-w['occupiedHeightMm']/2),
                    exp, 2, 'C radial insulation and duct assumptions', {'thicknessMm': thick})
            # LV: four axial packs, each resolves the seven radial winding layers.
            w = s.lv
            na, nr = int(w['turnsAxially']), int(w['turnsRadially'])
            axial_pitch = w['activeHeightMm']/na
            layer_pitch = (w['radialWidthMm']-a['lvRadialInsulationMm'])/nr
            axial_height = axial_pitch-a['lvInterTurnInsulationMm']
            for axial in range(na):
                for radial in range(nr):
                    inner = w['innerDiameterMm']+2*radial*layer_pitch
                    outer = inner+2*(layer_pitch-0.22)
                    self.add(f'lv_{phase}_{axial}_{radial}', f'{phase_label} LV · axial pack {axial+1} / layer {radial+1}', 'lv', 'copper_pack',
                        ring(outer, inner, axial_height),
                        (x, 0, s.winding_mid-w['activeHeightMm']/2+axial*axial_pitch),
                        ((phase-1)*420, -480, 40+axial*34), 3,
                        'C winding dimensions; homogenized strand and insulation pack',
                        {'innerDiameterMm': inner, 'outerDiameterMm': outer, 'heightMm': axial_height, 'parallelStrands': w['parallelStrands']},
                        w['copperMassKg']/(na*nr))
            self.add(f'lv_wrap_{phase}', f'{phase_label} LV · outer insulation', 'insulation', 'pressboard',
                ring(w['outerDiameterMm'], w['outerDiameterMm']-2*a['lvRadialInsulationMm'], w['activeHeightMm']),
                (x, 0, s.winding_mid-w['activeHeightMm']/2), exp, 3, 'C LV radial insulation allowance')
            # HV: individual solid discs, spaced by the exact inter-coil duct.
            w = s.hv
            discs = int(w['coils'])
            disc_height = w['axialStrands']*(w['strandWidthMm']+a['conductorInsulationMm'])
            for disc in range(discs):
                end = disc in (0, discs-1)
                turns = w['endCoilTurns'] if end else w['middleCoilTurns']
                self.add(f'hv_{phase}_{disc}', f'{phase_label} HV · disc {disc+1:02d} ({turns:g} turns)', 'hv', 'copper_pack',
                    ring(w['outerDiameterMm'], w['innerDiameterMm'], disc_height),
                    (x, 0, s.winding_mid-w['activeHeightMm']/2+disc*(disc_height+a['hvInterCoilInsulationMm'])),
                    ((phase-1)*560, 470, (disc-(discs-1)/2)*25), 4,
                    'C HV coil envelope; end discs retain the design radial envelope with lower copper fill',
                    {'turns': turns, 'innerDiameterMm': w['innerDiameterMm'], 'outerDiameterMm': w['outerDiameterMm'], 'heightMm': disc_height},
                    w['copperMassKg']*turns/w['turns'])
                if disc < discs-1:
                    # Eight separated spacers leave real open oil passages.
                    pieces=[]
                    for n in range(8):
                        theta=2*pi*n/8
                        rad=(w['outerDiameterMm']+w['innerDiameterMm'])/4
                        pieces.append(box(13,w['radialWidthMm']-2,a['hvInterCoilInsulationMm']).rotate((0,0,0),(0,0,1),90+n*45).translate((rad*cos(theta),rad*sin(theta),0)))
                    self.add(f'hv_spacers_{phase}_{disc}', f'{phase_label} HV · spacer set {disc+1}', 'insulation', 'pressboard',
                        cq.Compound.makeCompound(pieces),
                        (x,0,s.winding_mid-w['activeHeightMm']/2+disc*(disc_height+a['hvInterCoilInsulationMm'])+disc_height),
                        exp,4,'C duct height; assumed eight radial spacer blocks')
            for family in ('lv','hv'):
                w=getattr(s,family)
                end_height=(w['occupiedHeightMm']-w['activeHeightMm'])/2
                for upper in (False,True):
                    self.add(f'{family}_end_{phase}_{int(upper)}', f'{phase_label} {family.upper()} · {"upper" if upper else "lower"} end insulation',
                        'insulation','pressboard',ring(w['outerDiameterMm'],w['innerDiameterMm'],end_height),
                        (x,0,s.winding_mid+(w['activeHeightMm']/2 if upper else -w['occupiedHeightMm']/2)),
                        ((phase-1)*260,0,280 if upper else -220), 2 if not upper else 5,'C occupied/active winding heights')

    def supports(self):
        s=self.s
        length=s.frame['yokeLengthM']*1000+50
        y=s.frame['yokeWidthM']*500+16
        for upper in (False,True):
            z=s.core_bottom+(s.yoke_height+s.window_height+40 if upper else 15)
            for sign in (-1,1):
                channel=box(length,10,80).fuse(box(length,45,8).translate((0,sign*17.5,0)))
                self.add(f'clamp_{int(upper)}_{sign}', f'{"Top" if upper else "Bottom"} yoke clamp', 'structure','steel',channel,
                    (0,sign*y,z),(0,sign*230,350 if upper else -70),5 if upper else 1)
        for x in (-s.pitch,s.pitch):
            self.add(f'core_foot_{x}', 'Active-part support shoe', 'structure','steel',box(85,300,24),
                (x,0,s.core_bottom-24),(0,0,-80),1)

    def enclosure(self):
        s=self.s
        l,w,h=[s.tank[k]*1000 for k in ('lengthM','widthM','heightM')]
        t=s.assumptions['tankPlateThicknessM']*1000
        r=s.cad['tankCornerRadiusMm']
        # C tank dimensions are taken as the clear internal cavity; steel is outward.
        outer=rounded_plate(l+2*t,w+2*t,h+t,r+t).translate((0,0,-t))
        inner=rounded_plate(l,w,h+2,r)
        shell=outer.cut(inner)
        self.add('tank_body','Fabricated tank · open top','tank','paint',shell,(0,0,s.floor),(0,0,-1120),7,
            'C clear tank length / width / height and plate thickness; CAD corner radius',
            {'internalLengthMm':l,'internalWidthMm':w,'internalHeightMm':h,'wallMm':t})
        for x in (-l*.31,l*.31):
            channel=box(100,w+280,10).fuse(box(10,w+280,s.floor-t).translate((-45,0,0))).fuse(box(10,w+280,s.floor-t).translate((45,0,0)))
            self.add(f'base_{x}','Base channel','structure','paint',channel,(x,0,0),(0,0,-260),7)
        over=s.cad['flangeOverhangMm']
        f=flange(l+2*t,w+2*t,over,s.cad['flangeThicknessMm'],r+t)
        g=flange(l+2*t,w+2*t,over-5,s.cad['gasketThicknessMm'],r+t)
        cover=rounded_plate(l+2*t+2*over,w+2*t+2*over,s.cad['coverThicknessMm'],r+t+over)
        self.add('tank_flange','Welded cover flange','tank','paint',f,(0,0,s.tank_top),(0,0,420),7)
        self.add('cover_gasket','Cover gasket','cover','rubber',g,(0,0,s.tank_top+s.cad['flangeThicknessMm']),(0,0,700),9)
        self.add('tank_cover','Removable cover plate','cover','paint',cover,(0,0,s.cover_top-s.cad['coverThicknessMm']),(0,0,900),9)
        bolts=[]
        for x in [(-l/2+i*l/8) for i in range(9)]:
            for y in (-w/2-t-over/2,w/2+t+over/2): bolts.append((x,y))
        for y in (-w*.25,0,w*.25):
            for x in (-l/2-t-over/2,l/2+t+over/2): bolts.append((x,y))
        bolt=cq.Workplane('XY').polygon(6,22).extrude(9).val().fuse(cylinder(24,2).translate((0,0,-2)))
        for i,(x,y) in enumerate(bolts):
            self.add(f'cover_bolt_{i}','M14 cover fastener','cover','steel',bolt,(x,y,s.cover_top),(0,0,960),9)
        # Oil is a separate nominal cavity visualization; it is hidden by default.
        self.add('oil_domain','Nominal oil domain · simplified cavity','fluid','oil',rounded_plate(l-1,w-1,h-90,r),
            (0,0,s.floor),(0,0,-1120),8,'Visualization domain; overlaps active parts, excluded from mass and interference totals')

    def cooling(self):
        s=self.s
        l,w,h=[s.tank[k]*1000 for k in ('lengthM','widthM','heightM')]
        dia=s.tank['tubeDiameterM']*1000
        count=s.tank['coolingTubes']
        min_pitch=dia+s.cad['tubeMinimumGapMm']
        capacities=[max(1,int((length-100)//min_pitch)+1) for length in (l,w,l,w)]
        if count>sum(capacities): raise ValueError('Calculated cooling tubes cannot fit a single perimeter bank with 6 mm separation. Increase the tank or define a new bank layout.')
        allocations=[0]*4
        for _ in range(count):
            face=max(range(4),key=lambda i: capacities[i]/(allocations[i]+1) if allocations[i]<capacities[i] else -1)
            allocations[face]+=1
        rise=s.tank['tubeHeightM']*1000
        stand=s.cad['tubeStandOffMm']
        bend=min(stand*.65,40)
        path=(cq.Workplane('XZ').moveTo(0,0).lineTo(stand-bend,0).radiusArc((stand,bend),-bend)
              .lineTo(stand,rise-bend).radiusArc((stand-bend,rise),-bend).lineTo(0,rise))
        tube=(cq.Workplane('YZ').circle(dia/2).circle(dia/2-s.cad['tubeWallMm']).sweep(path, isFrenet=True)).val()
        z=s.floor+(h-rise)/2
        ports=[]
        for face,(n,length) in enumerate(zip(allocations,(l,w,l,w))):
            angle=(-90,0,90,180)[face]
            shape=tube.rotate((0,0,0),(0,0,1),angle)
            for i in range(n):
                along=0 if n==1 else -(length-100)/2+i*(length-100)/(n-1)
                # Local tube points along +X; rotate its manifold face around Z.
                x,y=((along,-w/2),(l/2,along),(along,w/2),(-l/2,along))[face]
                vector=((0,-360,0),(360,0,0),(0,360,0),(-360,0,0))[face]
                self.add(f'tube_{face}_{i}',f'Cooling tube · bank {face+1} / {i+1}','cooling','paint',shape,(x,y,z),vector,8,
                    'C tube count, diameter and height; assumed wall and bend/stand-off',{'diameterMm':dia,'heightMm':rise,'wallMm':s.cad['tubeWallMm']})
                self.tube_positions.append({'face':face,'positionMm':[x,y,z]})
                vx,vy=cos(angle*pi/180),sin(angle*pi/180)
                for level in (z,z+rise):
                    ports.append(rod_between((x-vx*4,y-vy*4,level),(x+vx*25,y+vy*25,level),dia-2*s.cad['tubeWallMm']))
        if ports:
            tank=next(p for p in self.parts if p.name=='tank_body')
            tank.shape=tank.shape.cut(cq.Compound.makeCompound(ports).translate((0,0,-s.floor)))
            if not tank.shape.isValid(): raise ValueError('Tank cooling ports produced invalid geometry')
        self.checks.append({'name':'Cooling tubes','status':'PASS','actual':sum(allocations),'expected':count,'unit':'parts'})

    def accessories(self):
        s=self.s
        l,w,h=[s.tank[k]*1000 for k in ('lengthM','widthM','heightM')]
        cover=s.cover_top
        cover_ports=[]
        # Bushing proportions are construction assumptions, never an insulation rating.
        for family,height,y in (('hv',s.cad['hvBushingHeightMm'],-w*.27),('lv',s.cad['lvBushingHeightMm'],w*.27)):
            count=4 if s.source['input'][family+'Connection'].lower()=='star' else 3
            pitch=s.pitch*2/(count-1)
            for i in range(count):
                x=(i-(count-1)/2)*pitch
                base_d=100 if family=='hv' else 75
                stem=32 if family=='hv' else 29
                body=cylinder(stem,height-35)
                n=6 if family=='hv' else 3
                for shed in range(n):
                    zz=18+shed*(height-60)/n
                    body=body.fuse(cq.Solid.makeCone(base_d/2-5,stem/2+3,14,cq.Vector(0,0,zz)))
                body=body.fuse(cylinder(base_d*.70,12))
                self.add(f'{family}_bushing_{i}',f'{family.upper()} bushing · {"N" if i==3 else "ABC"[i]}','terminals','porcelain',body,
                    (x,y,cover+8),(0,-220 if family=='hv' else 220,1050),10,
                    'Assumed porcelain fitting proportions; electrical rating requires a selected catalogue component',{'heightMm':height})
                self.add(f'{family}_bushing_flange_{i}',f'{family.upper()} bushing mounting flange','terminals','steel',ring(base_d,stem,8),(x,y,cover),(0,0,1030),10)
                self.add(f'{family}_terminal_{i}',f'{family.upper()} terminal stud','terminals','copper',cylinder(18 if family=='hv' else 24,45),
                    (x,y,cover+height-30),(0,0,1100),10)
                cover_ports.append(cylinder(stem,30).translate((x,y,-10)))
        cover_part=next(p for p in self.parts if p.name=='tank_cover')
        cover_part.shape=cover_part.shape.cut(cq.Compound.makeCompound(cover_ports))
        # Routing is schematic but made of closed copper solids with physical thickness.
        for i,x in enumerate((-s.pitch,0,s.pitch)):
            for family,y in (('hv',-w*.27),('lv',w*.27)):
                start=(x,(-1 if family=='hv' else 1)*(getattr(s,family)['outerDiameterMm']/2-12),s.winding_mid+getattr(s,family)['activeHeightMm']/2)
                count=4 if s.source['input'][family+'Connection'].lower()=='star' else 3
                end=((i-(count-1)/2)*s.pitch*2/(count-1),y,cover-20)
                self.add(f'lead_{family}_{i}',f'{"ABC"[i]} {family.upper()} lead · routing envelope','leads','copper',rod_between(start,end,12 if family=='hv' else 22),
                    explode=(0,-200 if family=='hv' else 200,520),stage=6,provenance='Assumed straight lead routing; connection topology is not a manufacturing wiring schedule')
        # Conservator above the rear side with closed walls and flat end plates.
        diameter=s.cad['conservatorDiameterMm']; length=l*s.cad['conservatorLengthFactor']; wall=s.cad['conservatorWallMm']
        cz=cover+s.cad['hvBushingHeightMm']+110
        cy=w/2+diameter*.48
        vessel=cylinder(diameter,length).cut(cylinder(diameter-2*wall,length-2*wall).translate((0,0,wall)))
        vessel=vessel.rotate((0,0,0),(0,1,0),90).translate((-length/2,0,0))
        self.add('conservator','Oil conservator','accessories','paint',vessel,(0,cy,cz),(0,340,1020),11,
            'Assumed conservator proportions; expansion capacity is not included in the C oil result',{'diameterMm':diameter,'lengthMm':length})
        for side in (-1,1):
            self.add(f'conservator_bracket_{side}','Conservator cantilever bracket','accessories','paint',box(65,cy-w*.15+45,15),
                (side*length*.32,(cy+w*.15)/2,cover),(0,340,790),11)
            self.add(f'conservator_support_{side}','Conservator support','accessories','paint',box(55,65,cz-cover-diameter/2+4),
                (side*length*.32,cy,cover),(0,340,800),11)
        connection=rod_between((0,w*.2,cover),(0,cy,cz-diameter/2),32)
        self.add('conservator_pipe','Conservator connection pipe · envelope','accessories','paint',connection,explode=(0,340,800),stage=11)
        # Separate end-mounted oil-level dial, bezel and pointer; assumed hardware.
        gx=length/2
        self.add('oil_gauge_bezel','Oil-level indicator bezel','accessories','steel',ring(92,76,9).rotate((0,0,0),(0,1,0),90),
            (gx,cy,cz),(130,340,1020),11)
        self.add('oil_gauge_face','Oil-level indicator face','accessories','enamel',cylinder(76,2).rotate((0,0,0),(0,1,0),90),
            (gx+7,cy,cz),(130,340,1020),11,'General-arrangement indicator; pointer is illustrative, not a simulated oil-level sensor')
        ticks=[box(2,6,1).translate((0,30,0)).rotate((0,0,0),(0,0,1),i*30) for i in range(12)]
        ticks.append(box(3,27,1).translate((0,10,0)).rotate((0,0,0),(0,0,1),-40))
        dial=cq.Compound.makeCompound(ticks).rotate((0,0,0),(0,1,0),90)
        self.add('oil_gauge_marks','Oil-level indicator pointer and graduations','accessories','rubber',dial,(gx+9,cy,cz),(130,340,1020),11)
        # Breather, drain valve, tap selector, lifting eyes, and readable nameplate field.
        self.add('breather_body','Silica-gel breather housing','accessories','glass',ring(64,55,135),(length/2+35,cy,cz-330),(260,340,800),11)
        self.add('breather_desiccant','Breather desiccant','accessories','silica',cylinder(52,115),(length/2+35,cy,cz-320),(260,340,800),11)
        self.add('breather_pipe','Breather connection pipe','accessories','steel',rod_between((length/2+35,cy,cz-185),(length/2-25,cy,cz-60),18),
            explode=(260,340,800),stage=11)
        for zz in (cz-342,cz-195):
            self.add(f'breather_cap_{zz}','Breather end cap','accessories','steel',cylinder(70,12),(length/2+35,cy,zz),(260,340,800),11)
        self.add('tap_selector_base','Off-circuit tap-selector spindle','accessories','steel',cylinder(64,45),(l*.37,0,cover),(0,0,950),10)
        self.add('tap_selector_handle','Off-circuit tap-selector handle','accessories','rubber',rounded_plate(105,20,15,8),(l*.37,0,cover+45),(0,0,970),10)
        eye=cq.Workplane('XZ').rect(70,85).extrude(14).edges('|Y').fillet(15).faces('>Y').workplane().hole(32).val()
        for i,x in enumerate((-l*.37,l*.37)):
            self.add(f'lifting_eye_{i}','Lifting eye','cover','paint',eye,(x,-w*.30,cover+42.5),(0,0,920),9)
        valve=cylinder(40,75).rotate((0,0,0),(1,0,0),90)
        self.add('drain_valve','Oil drain valve body','accessories','steel',valve,(l*.42,-w/2-10,s.floor+60),(0,-280,0),11)
        self.add('drain_handle','Drain valve handle','accessories','rubber',box(80,15,12),(l*.42,-w/2-50,s.floor+85),(0,-280,0),11)
        plate=rounded_plate(230,135,3,8).rotate((0,0,0),(1,0,0),90)
        plate_y=-w/2-s.cad['tubeStandOffMm']-s.tank['tubeDiameterM']*500-8
        self.add('nameplate',f"Rating plate · {s.source['input']['kva']:g} kVA",'accessories','steel',plate,(l*.18,plate_y,s.floor+h*.63),(0,-230,0),11)
        for dx in (-90,90):
            stand=rod_between((l*.18+dx,plate_y,s.floor+h*.63),(l*.18+dx,-w/2,s.floor+h*.63),8)
            self.add(f'nameplate_mount_{dx}','Rating-plate stand-off','accessories','steel',stand,explode=(0,-230,0),stage=11,
                provenance='Assumed cooling-bank nameplate support; not a fabrication attachment detail')

    def cad_assembly(self):
        result=cq.Assembly(name=f"TX_{self.s.source['input']['kva']:g}kVA_{self.s.source['input']['connectionCode']}_ONAN")
        groups={}
        for part in self.parts:
            if part.group=='fluid': continue
            if part.group not in groups: groups[part.group]=cq.Assembly(name=part.group)
            material=MATERIALS[part.material]
            groups[part.group].add(part.shape,name=part.name,loc=cq.Location(cq.Vector(*part.position)),color=cq.Color(*material['color']))
        for group in groups.values(): result.add(group)
        return result
