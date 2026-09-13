"""Generate validated CAD, STEP, browser assets and a traceable report."""
import argparse
from collections import Counter
from datetime import datetime, timezone
import json
from pathlib import Path
import tempfile
import os

import cadquery as cq
from assembly import TransformerAssembly, MATERIALS
from export_mesh import write_glb
from spec import Specification, fingerprint
from section import section_parts

ROOT=Path(__file__).resolve().parent.parent


def inspect_model(model):
    s=model.s
    checks=list(model.checks)
    entries=[]
    for p in model.parts:
        if not p.shape.isValid() or not p.shape.Solids() or p.shape.Volume()<=0:
            raise ValueError(f'Invalid final CAD solid: {p.name}')
        volume=p.shape.Volume()
        bounds=p.shape.BoundingBox()
        density=MATERIALS[p.material]['density']
        if p.material in ('core','steel','paint'): density=s.assumptions['steelDensityKgM3']
        if p.material=='copper': density=s.assumptions['copperDensityKgM3']
        if p.material=='pressboard': density=s.cad['pressboardDensityKgM3']
        if p.material=='porcelain': density=s.cad['porcelainDensityKgM3']
        mass=volume*1e-9*density if density is not None else None
        if p.group=='fluid': mass=None
        entries.append({'id':p.name,'label':p.label,'group':p.group,'material':p.material,
            'solidCount':len(p.shape.Solids()),'volumeMm3':volume,'cadMaterialMassKg':mass,
            'analyticalCopperMassKg':p.analytical_mass,'stage':p.stage,'provenance':p.provenance,
            'dimensions':p.dimensions,'positionMm':p.position,'explodeMm':p.explode,
            'boundsMm':[bounds.xmin+p.position[0],bounds.ymin+p.position[1],bounds.zmin+p.position[2],
                        bounds.xmax+p.position[0],bounds.ymax+p.position[1],bounds.zmax+p.position[2]]})
    core_mass=sum(p['cadMaterialMassKg'] for p in entries if p['group']=='core')
    checks.append({'name':'Core gross-envelope steel accounting','status':'PASS' if abs(core_mass-s.frame['totalIronMassKg'])<.05 else 'FAIL',
                   'actual':core_mass,'expected':s.frame['totalIronMassKg'],'unit':'kg'})
    for family in ('lv','hv'):
        expected=getattr(s,family)['copperMassKg']*3
        actual=sum(p['analyticalCopperMassKg'] or 0 for p in entries if p['group']==family)
        checks.append({'name':f'{family.upper()} copper across three phases','status':'PASS' if abs(actual-expected)<.01 else 'FAIL','actual':actual,'expected':expected,'unit':'kg'})
    for family in ('lv','hv'):
        clearance=s.window_height-getattr(s,family)['occupiedHeightMm']
        checks.append({'name':f'{family.upper()} axial assembly fit','status':'PASS' if clearance>=7 else 'WARN','actual':clearance,'expected':7,'unit':'mm minimum'})
    checks.append({'name':'Adjacent HV winding clearance','status':'PASS' if s.pitch-s.hv['outerDiameterMm']>=15 else 'WARN',
                   'actual':s.pitch-s.hv['outerDiameterMm'],'expected':15,'unit':'mm minimum'})
    warnings=[
        'C LV and HV copper masses are per phase. The C tank total adds each once; this assembly contains three windings of each kind. The C totals are retained in the source snapshot and are not treated as the CAD assembly mass.',
        'Winding solids represent copper/insulation packs. Their geometric volume must not be multiplied by pure copper density. Copper mass is separately assigned from the C winding calculation.',
        'Core packet cross-section is area-matched to C gross area. Packet schedule and butt-joint geometry are construction assumptions, not a fabrication lamination schedule.',
        'The C core mass uses gross area and steel density. CAD repeats that convention; insulation between laminations is not resolved.',
        'C tank dimensions are interpreted as the clear internal cavity. Exterior walls, flange, cover, cooling tubes and fittings increase overall size.',
        'Bushings, clamps, lead routing, conservator and hardware are assumed general-arrangement components. Dielectric, pressure, weld and mechanical design are not certified by their geometry.',
        'Nominal oil domain is a display aid, excluded from STEP and mass totals; it is not a boolean-subtracted oil-volume calculation.',
        'Cooling tube heat-transfer height follows the C parameter; bends and connection details are CAD assumptions.'
        ,'Star-connected windings display an accessible neutral bushing; delta windings display three bushings. Internal vector-group jumpers and neutral lead topology are not resolved.'
    ]
    if any(c['status']=='FAIL' for c in checks): raise ValueError('CAD reconciliation failed: '+str(checks))
    return entries,checks,warnings


def generate(input_path, out_dir, assumptions_path, preview=False):
    spec=Specification.load(input_path,assumptions_path)
    model=TransformerAssembly(spec).build()
    parts,checks,warnings=inspect_model(model)
    out_dir=Path(out_dir); out_dir.mkdir(parents=True,exist_ok=True)
    # Previous successful assets remain untouched until all exports validate.
    with tempfile.TemporaryDirectory(prefix='tx-cad-',dir=out_dir.parent) as temporary:
        temp=Path(temporary)
        print(f'Exporting {len(parts)} named parts to STEP...',flush=True)
        assembly=model.cad_assembly()
        assembly.export(str(temp/'transformer.step'))
        print('Tessellating solid surfaces for the browser...',flush=True)
        mesh=write_glb(model.parts,temp/'transformer.glb',spec.cad['meshDeflectionMm'],spec.cad['meshAngularToleranceRad'])
        print('Generating closed CAD half-section...',flush=True)
        section_mesh=write_glb(section_parts(model.parts),temp/'transformer-section.glb',spec.cad['meshDeflectionMm'],spec.cad['meshAngularToleranceRad'])
        manifest={'schemaVersion':1,'generator':f'CadQuery {cq.__version__} / OpenCascade','generatedAt':datetime.now(timezone.utc).isoformat(),
            'fingerprint':fingerprint(spec.source),'name':f"{spec.source['input']['kva']:g} kVA · {spec.source['input']['connectionCode']} · ONAN",
            'coordinateSystem':'CAD millimetres Z-up; GLB metres Y-up','parts':parts,'checks':checks,'notes':warnings,
            'groups':dict(Counter(p['group'] for p in parts)),'partCount':len(parts),'solidCount':sum(p['solidCount'] for p in parts),
            'packetSchedule':model.packet_schedule,'mesh':mesh,'sectionMesh':section_mesh,'input':spec.source['input'],'cadAssumptions':spec.cad,
            'dimensions':{'tankInternalMm':[spec.tank[k]*1000 for k in ('lengthM','widthM','heightM')],
                          'limbEnvelopeMm':spec.d,'limbPitchMm':spec.pitch,'lvOutsideMm':spec.lv['outerDiameterMm'],'hvOutsideMm':spec.hv['outerDiameterMm']},
            'stages':['Core and lower clamps','Insulating cylinders and end blocks','LV winding packs','HV disc windings and spacers',
                      'Upper yoke and clamps','Connection leads','Tank and supporting base','Cooling-tube banks','Cover and gasket','Bushings and terminals','Conservator and accessories']}
        (temp/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
        (temp/'source-result.json').write_text(json.dumps(spec.source,indent=2),encoding='utf-8')
        lines=[f'TX-C SOLID CAD REPORT — {manifest["name"]}',f'Geometry ID: {manifest["fingerprint"]}',
               f'{manifest["partCount"]} named parts / {manifest["solidCount"]} valid solids',f'Browser mesh: {mesh["sizeBytes"]/1e6:.2f} MB / {mesh["renderedTriangles"]:,} triangles','', 'RECONCILIATION']
        lines += [f'{c["status"]}: {c["name"]}: {c["actual"]:.6g} (reference {c["expected"]:.6g}) {c["unit"]}' for c in checks]
        lines += ['', 'CONSTRUCTION NOTES']+['- '+note for note in warnings]
        (temp/'cad-report.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        if preview:
            from preview import render_preview
            render_preview(model,temp/'assembly.png',False)
            render_preview(model,temp/'active-part.png',True)
        for file in temp.iterdir(): os.replace(file,out_dir/file.name)
    print(json.dumps({'ok':True,'parts':len(parts),'solids':manifest['solidCount'],'meshMB':round(mesh['sizeBytes']/1e6,2),'output':str(out_dir)}),flush=True)
    return manifest


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=ROOT/'data'/'output.json')
    parser.add_argument('--out',type=Path,default=ROOT/'dist'/'cad'/'assets')
    parser.add_argument('--assumptions',type=Path,default=Path(__file__).with_name('cad_assumptions.json'))
    parser.add_argument('--preview',action='store_true')
    args=parser.parse_args()
    generate(args.input,args.out,args.assumptions,args.preview)
