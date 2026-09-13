"""CAD regression checks. Run with the same Python environment as build.py."""
from copy import deepcopy
import json
from pathlib import Path
import struct
import unittest
import cadquery as cq
from OCP.BRepCheck import BRepCheck_Shell, BRepCheck_NoError
import numpy as np
from assembly import TransformerAssembly
from build import inspect_model
from section import section_parts
from spec import Specification

ROOT=Path(__file__).resolve().parent.parent
ASSETS=ROOT/'dist/cad/assets'


def closed_solid(solid):
    # TopoDS_Solid.Closed is an unused flag, even on a kernel-created box.
    # Test the actual boundary shells for free edges instead.
    return solid.isValid() and bool(solid.Shells()) and all(
        BRepCheck_Shell(shell.wrapped).Closed()==BRepCheck_NoError for shell in solid.Shells())


class SolidDesignTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.spec=Specification.load(ASSETS/'source-result.json',ROOT/'tx-3d/cad_assumptions.json')
        cls.model=TransformerAssembly(cls.spec).build()
        cls.by_id={p.name:p for p in cls.model.parts}

    def test_unique_named_valid_positive_solids(self):
        self.assertEqual(len(self.by_id),len(self.model.parts))
        for p in self.model.parts:
            with self.subTest(part=p.name):
                self.assertTrue(p.shape.isValid())
                self.assertGreater(p.shape.Volume(),0)
                self.assertTrue(all(closed_solid(s) for s in p.shape.Solids()))

    def test_c_reconciliation_and_real_warning(self):
        _,checks,_=inspect_model(self.model)
        self.assertFalse(any(c['status']=='FAIL' for c in checks))
        clearance=next(c for c in checks if c['name']=='Adjacent HV winding clearance')
        self.assertAlmostEqual(clearance['actual'],self.spec.pitch-self.spec.hv['outerDiameterMm'])
        self.assertEqual(clearance['status'],'WARN' if clearance['actual']<15 else 'PASS')

    def test_hollow_tank_and_tubes(self):
        tank=self.by_id['tank_body']
        s=self.spec
        self.assertLess(tank.shape.Volume(),s.tank['lengthM']*s.tank['widthM']*s.tank['heightM']*1e9*.15)
        tubes=[p for p in self.model.parts if p.group=='cooling']
        self.assertEqual(len(tubes),s.tank['coolingTubes'])
        for p in tubes:
            self.assertEqual(len(p.shape.Solids()),1)
            self.assertLess(p.shape.Volume(),np.pi*(s.tank['tubeDiameterM']*500)**2*s.tank['tubeHeightM']*1000*.35)

    def test_concentric_clearance_and_disc_height(self):
        s=self.spec
        for phase in range(3):
            discs=[p for p in self.model.parts if p.name.startswith(f'hv_{phase}_')]
            self.assertEqual(len(discs),s.hv['coils'])
            zmin=min(p.position[2]+p.shape.BoundingBox().zmin for p in discs)
            zmax=max(p.position[2]+p.shape.BoundingBox().zmax for p in discs)
            self.assertAlmostEqual(zmax-zmin,s.hv['activeHeightMm'],places=3)
            for p in discs:
                self.assertAlmostEqual(p.shape.BoundingBox().xlen,s.hv['outerDiameterMm'],places=4)

    def test_closed_half_section_has_cut_faces(self):
        sample=[p for p in self.model.parts if p.name in ('yoke_upper','hv_0_0','tank_body')]
        section=section_parts(sample)
        self.assertEqual(len(section),3)
        for p in section:
            self.assertTrue(p.shape.isValid())
            self.assertGreaterEqual(p.shape.BoundingBox().ymin+p.position[1],-1e-5)
            self.assertLess(p.shape.Volume(),self.by_id[p.name].shape.Volume()*.501)
            self.assertTrue(all(closed_solid(s) for s in p.shape.Solids()))

    def test_rejects_partial_and_invalid_designs(self):
        for mutation in ('partial','overlap','inverted','tube_wall'):
            source,cad=deepcopy(self.spec.source),deepcopy(self.spec.cad)
            if mutation=='partial':source['meta']['completedSections']=['frame']
            if mutation=='overlap':source['sections']['frame']['centreDistanceM']=.1
            if mutation=='inverted':source['sections']['lv']['outerDiameterMm']=100
            if mutation=='tube_wall':cad['tubeWallMm']=100
            with self.subTest(case=mutation),self.assertRaises(ValueError):Specification(source,cad).validate()

    def test_parameters_drive_geometry_and_connection_fittings(self):
        source=deepcopy(self.spec.source)
        source['sections']['frame']['centreDistanceM']+=.02
        modified=TransformerAssembly(Specification(source,deepcopy(self.spec.cad)))
        modified.core()
        p=next(p for p in modified.parts if p.name=='core_2_0_+1')
        self.assertAlmostEqual(p.position[0],self.spec.pitch+20)
        source['input']['hvConnection']='Star';source['input']['lvConnection']='Delta'
        modified.enclosure();modified.accessories()
        self.assertEqual(len([p for p in modified.parts if p.name.startswith('hv_bushing_') and 'flange' not in p.name]),4)
        self.assertEqual(len([p for p in modified.parts if p.name.startswith('lv_bushing_') and 'flange' not in p.name]),3)

    def test_step_round_trip_preserves_solids_and_volume(self):
        imported=cq.importers.importStep(str(ASSETS/'transformer.step')).val()
        parts=[p for p in self.model.parts if p.group!='fluid']
        self.assertEqual(len(imported.Solids()),sum(len(p.shape.Solids()) for p in parts))
        self.assertAlmostEqual(imported.Volume()/sum(p.shape.Volume() for p in parts),1,places=6)

    def test_glb_integrity_and_lightweight_budget(self):
        manifest=json.loads((ASSETS/'manifest.json').read_text(encoding='utf-8'))
        for filename in ('transformer.glb','transformer-section.glb'):
            raw=(ASSETS/filename).read_bytes()
            magic,version,length=struct.unpack_from('<III',raw)
            self.assertEqual((magic,version,length),(0x46546c67,2,len(raw)))
            json_size,json_tag=struct.unpack_from('<II',raw,12)
            self.assertEqual(json_tag,0x4e4f534a)
            doc=json.loads(raw[20:20+json_size])
            binary=raw[28+json_size:]
            if filename=='transformer.glb':self.assertEqual(len(doc['nodes']),manifest['partCount'])
            self.assertLess(len(raw),8_000_000)
            for acc in doc['accessors']:
                view=doc['bufferViews'][acc['bufferView']]
                self.assertLessEqual(view['byteOffset']+view['byteLength'],len(binary))
                if acc['componentType']==5126:
                    values=np.frombuffer(binary,dtype='<f4',count=view['byteLength']//4,offset=view['byteOffset'])
                    self.assertTrue(np.isfinite(values).all())


if __name__=='__main__':unittest.main(verbosity=2)
